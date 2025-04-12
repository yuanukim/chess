/*
    @author yuanukim
    @brief  chess game with alpha-beta prunning AI, written in C++14.
*/
#include <iostream>
#include <fstream>
#include <algorithm>
#include <stdexcept>
#include <string>
#include <array>
#include <vector>
#include <deque>
#include <limits>
#include <cstdio>
#include <cstdint>

#ifdef _WIN32
#include <windows.h>
#endif

/*
    As defined below, the total number of pieces is 14.
    exclude P_EE and P_EO, then left 12.
*/
constexpr int32_t PIECE_TOTAL_NUM = 14;
constexpr int32_t PIECE_TOTAL_NUM_EXCLUDE_EXTRA = 12;

constexpr int32_t EDGE_LEN = 12;    // chess board is a 8 x 8 square, to speed up bound checking, I add 2 lines to both left, right or top, down.
constexpr int32_t LINE_BEGIN = 2;   // the inner 8 x 8 chess board's row/column index begins with 2.
constexpr int32_t LINE_END = 10;    // the inner 8 x 8 chess board's row/column index ends with 10.

constexpr int32_t UPPER_PAWN_BEGIN_ROW = 3;
constexpr int32_t DOWN_PAWN_BEGIN_ROW = 8;
constexpr int32_t UPPER_PAWN_PROMOTE_ROW = 9;
constexpr int32_t DOWN_PAWN_PROMOTE_ROW = 2;

enum Side {
    PS_UPPER,  // upper side.
    PS_DOWN,   // down side.
    PS_EXTRA   // neither upper nor down side, such as empty.
};

enum Type {
    PT_PAWN,     // pawn.
    PT_ROOK,     // rook.
    PT_KNIGHT,   // knight.
    PT_BISHOP,   // bishop.
    PT_QUEEN,    // queen.
    PT_KING,     // king.
    PT_EMPTY,    // empty.
    PT_OUT,      // out of chess board.
};

enum Piece {
    P_UP = 0,    // upper pawn.
    P_UR = 1,    // upper rook.
    P_UN = 2,    // upper knight.
    P_UB = 3,    // upper bishop.
    P_UQ = 4,    // upper queen.
    P_UK = 5,    // upper king.
    P_DP = 6,    // down pawn.
    P_DR = 7,    // down rook.
    P_DN = 8,    // down knight.
    P_DB = 9,    // down bishop.
    P_DQ = 10,   // down queen.
    P_DK = 11,   // down king.
    P_EE = 12,   // empty.
    P_EO = 13,   // out of chess board.
};

static std::array<std::string, PIECE_TOTAL_NUM> piece_str_mapping = {
    "P", "R", "N", "B", "Q", "K",
    "p", "r", "n", "b", "q", "k",
    ".", "#"
};

static std::array<float, PIECE_TOTAL_NUM_EXCLUDE_EXTRA> piece_value_mapping;

using PosValue = std::array<std::array<float, EDGE_LEN>, EDGE_LEN>;
static std::array<PosValue, PIECE_TOTAL_NUM_EXCLUDE_EXTRA> piece_pos_value_mapping;

constexpr Type get_type(Piece p) noexcept { 
    switch(p){
        case P_UP:
        case P_DP:
            return PT_PAWN;
        case P_UR:
        case P_DR:
            return PT_ROOK;
        case P_UN:
        case P_DN:
            return PT_KNIGHT;
        case P_UB:
        case P_DB:
            return PT_BISHOP;
        case P_UQ:
        case P_DQ:
            return PT_QUEEN;
        case P_UK:
        case P_DK:
            return PT_KING;
        case P_EE:
            return PT_EMPTY;
        case P_EO:
        default:
            return PT_OUT;
    }
}

constexpr Side get_side(Piece p) noexcept {
    switch(p){
        case P_UP: 
        case P_UR: 
        case P_UN: 
        case P_UB: 
        case P_UQ: 
        case P_UK:
            return PS_UPPER;
        case P_DP:
        case P_DR:
        case P_DN:
        case P_DB:
        case P_DQ:
        case P_DK:
            return PS_DOWN;
        case P_EE:
        case P_EO:
        default:
            return PS_EXTRA;
    }
}

std::string get_str(Piece p) noexcept {
    return piece_str_mapping[static_cast<uint32_t>(p)];
}

Piece str_to_piece(const std::string& str) {
    for (uint32_t i = 0; i < piece_str_mapping.size(); ++i) {
        if (str == piece_str_mapping[i]) {
            return static_cast<Piece>(i);
        }
    }

    throw std::runtime_error{ "str_to_piece: failed, cannot convert \"" + str + "\" to a piece" };
}

enum class MoveType{
    INVALID,
    NORMAL,
    EN_PASSANT,
    LONG_CASTLE,
    SHORT_CASTLE,
    GO_AND_PROMOTE,
    PAWN_2_STEPS
};

struct Pos{
    int32_t row, col;

    Pos() : row{ 0 }, col{ 0 } {}
    Pos(int32_t row, int32_t col) : row{ row }, col{ col } {}
    
    bool operator==(Pos const& other) const noexcept { return row == other.row && col == other.col; }
    bool operator!=(Pos const& other) const noexcept { return !(*this == other); }
};

struct Move{
    Pos from, to;
    MoveType moveType;

    Move() : moveType{ MoveType::INVALID } {}
    Move(Pos from, Pos to, MoveType moveType) :  from{ from }, to{ to }, moveType{ moveType } {}

    bool operator==(Move const& other) const noexcept { return from == other.from && to == other.to; }
    bool operator!=(Move const& other) const noexcept { return !(*this == other); }
};

using BoardData = std::array<std::array<Piece, EDGE_LEN>, EDGE_LEN>;

class ChessBoard{
    struct HistoryNode{
        Pos from, to;
        MoveType moveType;
        Piece fromP, toP;

        HistoryNode(Move const& mv, Piece fromP, Piece toP) : from{ mv.from }, to{ mv.to }, moveType{ mv.moveType }, fromP{ fromP }, toP{ toP } {}
    };

    BoardData data;
    std::deque<HistoryNode> history;
    bool canUpperCastle, canDownCastle;
    Pos enPassantPos;

    void set(int32_t r, int32_t c, Piece p) noexcept { 
        data[r][c] = p; 
    }

    void set(Pos pos, Piece p) noexcept { 
        set(pos.row, pos.col, p); 
    }

    void reset_castle(Side side) noexcept { 
        side == PS_UPPER ? canUpperCastle = true : canDownCastle = true; 
    }

    void reset_en_passant() noexcept { 
        enPassantPos.row = enPassantPos.col = 0; 
    }
public:
    ChessBoard() : canUpperCastle{ true }, canDownCastle{ true } {
        std::string path = "res/default_board.txt";
        std::ifstream in{ path };
        if (!in.is_open()) {
            throw std::runtime_error{ "build default chess board failed, cannot open \"" + path + "\"" };
        }

        for (int32_t r = 0; r < EDGE_LEN; ++r) {
            for (int32_t c = 0; c < EDGE_LEN; ++c) {
                data[r][c] = P_EO;
            }
        }

        std::string sign;
        Piece p;

        for (int32_t r = LINE_BEGIN; r < LINE_END; ++r) {
            for (int32_t c = LINE_BEGIN; c < LINE_END; ++c) {
                in >> sign;
                data[r][c] = str_to_piece(sign);
            }
        }
    }

    Piece get(int32_t r, int32_t c) const noexcept { 
        return data[r][c]; 
    }

    Piece get(Pos pos) const noexcept { 
        return get(pos.row, pos.col); 
    }

    bool can_upper_castle() const noexcept { 
        return canUpperCastle; 
    }

    bool can_down_castle() const noexcept { 
        return canDownCastle; 
    }

    Pos get_en_passant_pos() const noexcept { 
        return enPassantPos; 
    }

    void move(Move const& mv) {
        Piece fromP = get(mv.from);

        history.emplace_back(mv, fromP, get(mv.to));

        set(mv.to, fromP);
        set(mv.from, P_EE);

        reset_en_passant();

        if (fromP == P_UK){   // if king moves, then castle can't be done later.
            canUpperCastle = false;
        }
        else if (fromP == P_DK){
            canDownCastle = false;
        }

        if (mv.moveType == MoveType::LONG_CASTLE){
            set(mv.from.row, mv.from.col - 1, get(mv.from.row, mv.from.col - 4));
            set(mv.from.row, mv.from.col - 4, P_EE);
        }
        else if (mv.moveType == MoveType::SHORT_CASTLE){
            set(mv.from.row, mv.from.col + 1, get(mv.from.row, mv.from.col + 3));
            set(mv.from.row, mv.from.col + 3, P_EE);
        }
        else if (mv.moveType == MoveType::EN_PASSANT){
            set(mv.from.row, mv.to.col, P_EE);
        }
        else if (mv.moveType == MoveType::PAWN_2_STEPS){
            auto s = get_side(fromP);
            auto reversePawn = (s == PS_UPPER ? P_DP : P_UP);

            if (get(mv.from.row, mv.from.col - 1) == reversePawn){
                enPassantPos = Pos{ mv.from.row, mv.from.col - 1 };
            }
            else if (get(mv.from.row, mv.from.col + 1) == reversePawn){
                enPassantPos = Pos{ mv.from.row, mv.from.col + 1 };
            }
        }
    }

    void move(Move const& mv, Piece promoteP) {
        history.emplace_back(mv, get(mv.from), get(mv.to));

        set(mv.to, promoteP);
        set(mv.from, P_EE);
    }

    void undo() {
        if (history.empty()){
            return;
        }

        const HistoryNode& hist = history.back();

        set(hist.from, hist.fromP);
        set(hist.to, hist.toP);

        history.pop_back();

        if (hist.moveType == MoveType::LONG_CASTLE){
            set(hist.from.row, hist.from.col - 4, get(hist.from.row, hist.from.col - 1));
            set(hist.from.row, hist.from.col - 1, P_EE);
            reset_castle(get_side(hist.fromP));
        }
        else if (hist.moveType == MoveType::SHORT_CASTLE){
            set(hist.from.row, hist.from.col + 3, get(hist.from.row, hist.from.col + 1));
            set(hist.from.row, hist.from.col + 1, P_EE);
            reset_castle(get_side(hist.fromP));
        }
        else if (hist.moveType == MoveType::EN_PASSANT){
            set(hist.from.row, hist.to.col, (get_side(hist.fromP) == PS_UPPER ? P_DP : P_UP));
            enPassantPos = Pos{ hist.from.row, hist.to.col };
        }
    }
};

void load_piece_value_mapping() {
    std::string path = "res/pvalues.txt";

    std::ifstream in { path };
    if (!in.is_open()) {
        throw std::invalid_argument{ "cannot load piece -> value mapping from \"" + path + "\"" };
    }

    for (int32_t i = 0; i < piece_value_mapping.size(); ++i) {
        in >> piece_value_mapping[i];

        if (!in) {
            throw std::runtime_error{ "load piece -> value mapping from \"" + path + "\" failed, data maybe broken" };
        }
    }
}

void load_single_piece_pos_value_mapping(Piece p, const std::string& path) {
    std::ifstream in { path };
    if (!in.is_open()) {
        throw std::invalid_argument{ "cannot load piece -> pos value mapping from \"" + path + "\"" };
    }

    PosValue& slice = piece_pos_value_mapping[static_cast<uint32_t>(p)];

    for (int32_t r = 0; r < EDGE_LEN; ++r) {
        for (int32_t c = 0; c < EDGE_LEN; ++c) {
            slice[r][c] = 0;
        }
    }

    for (int32_t r = LINE_BEGIN; r < LINE_END; ++r) {
        for (int32_t c = LINE_BEGIN; c <= LINE_END; ++c) {
            in >> slice[r][c];

            if (!in) {
                throw std::runtime_error{ "load piece -> pos value mapping from \"" + path + "\" failed, data maybe broken" };
            }
        }
    }
}

void load_piece_pos_value_mapping() {
    load_single_piece_pos_value_mapping(P_UP, "res/pos_value_upper_pawn.txt");
    load_single_piece_pos_value_mapping(P_UR, "res/pos_value_upper_rook.txt");
    load_single_piece_pos_value_mapping(P_UN, "res/pos_value_upper_knight.txt");
    load_single_piece_pos_value_mapping(P_UB, "res/pos_value_upper_bishop.txt");
    load_single_piece_pos_value_mapping(P_UQ, "res/pos_value_upper_queen.txt");
    load_single_piece_pos_value_mapping(P_UK, "res/pos_value_upper_king.txt");
    load_single_piece_pos_value_mapping(P_DP, "res/pos_value_down_pawn.txt");
    load_single_piece_pos_value_mapping(P_DR, "res/pos_value_down_rook.txt");
    load_single_piece_pos_value_mapping(P_DN, "res/pos_value_down_knight.txt");
    load_single_piece_pos_value_mapping(P_DB, "res/pos_value_down_bishop.txt");
    load_single_piece_pos_value_mapping(P_DQ, "res/pos_value_down_queen.txt");
    load_single_piece_pos_value_mapping(P_DK, "res/pos_value_down_king.txt");
}

inline float get_piece_value(Piece p) {
    return piece_value_mapping[static_cast<uint32_t>(p)];
}

inline float get_piece_pos_value(Piece p, int32_t row, int32_t col){ 
    return piece_pos_value_mapping[static_cast<uint32_t>(p)][row][col];
}

bool try_add_possible_move(ChessBoard const& cb, Pos from, Pos to, std::vector<Move>& vec){
    Piece fromP = cb.get(from);
    Piece toP = cb.get(to);

    switch(toP){
        case P_EO: 
            return false;
        case P_EE:
            vec.emplace_back(from, to, MoveType::NORMAL);
            return true;
        default:
            if (get_side(fromP) != get_side(toP)){
                vec.emplace_back(from, to, MoveType::NORMAL);
            }

            return false;
    }
}

void gen_crossing(ChessBoard const& cb, Pos from, std::vector<Move>& vec) {
    for (int32_t rr = from.row - 1; try_add_possible_move(cb, from, Pos{ rr, from.col }, vec); --rr);
    for (int32_t rr = from.row + 1; try_add_possible_move(cb, from, Pos{ rr, from.col }, vec); ++rr);
    for (int32_t cc = from.col - 1; try_add_possible_move(cb, from, Pos{ from.row, cc }, vec); --cc);
    for (int32_t cc = from.col + 1; try_add_possible_move(cb, from, Pos{ from.row, cc }, vec); ++cc);
}

void gen_diagonal(ChessBoard const& cb, Pos from, std::vector<Move>& vec) {
    for (int32_t rr = from.row - 1, cc = from.col - 1; try_add_possible_move(cb, from, Pos{ rr, cc }, vec); --rr, --cc);
    for (int32_t rr = from.row - 1, cc = from.col + 1; try_add_possible_move(cb, from, Pos{ rr, cc }, vec); --rr, ++cc);
    for (int32_t rr = from.row + 1, cc = from.col - 1; try_add_possible_move(cb, from, Pos{ rr, cc }, vec); ++rr, --cc);
    for (int32_t rr = from.row + 1, cc = from.col + 1; try_add_possible_move(cb, from, Pos{ rr, cc }, vec); ++rr, ++cc);
}

void gen_moves_pawn(ChessBoard const& cb, Pos from, std::vector<Move>& vec){
    auto add_and_check_promote = [&vec, &from](Pos to, bool canPromote){
        if (canPromote){
            vec.emplace_back(from, to, MoveType::GO_AND_PROMOTE);
        }
        else {
            vec.emplace_back(from, to, MoveType::NORMAL);
        }
    };

    Piece fromP = cb.get(from);
    Pos enPassantPos = cb.get_en_passant_pos();

    if (fromP == P_UP){
        if (from.row == enPassantPos.row){
            if (from.col + 1 == enPassantPos.col || from.col - 1 == enPassantPos.col){
                vec.emplace_back(from, Pos{ from.row + 1, cb.get_en_passant_pos().col }, MoveType::EN_PASSANT);
            }
        }

        if (cb.get(from.row + 1, from.col) == P_EE){
            if (from.row == UPPER_PAWN_BEGIN_ROW && cb.get(from.row + 2, from.col) == P_EE){
                vec.emplace_back(from, Pos{ from.row + 2, from.col }, MoveType::PAWN_2_STEPS);
            }

            add_and_check_promote(Pos{ from.row + 1, from.col }, from.row + 1 == UPPER_PAWN_PROMOTE_ROW);
        }

        for (int32_t col : { from.col + 1, from.col - 1 }){
            Piece p = cb.get(from.row + 1, col);
            Side s = get_side(p);
            
            if (s != PS_EXTRA && s != get_side(fromP)){
                add_and_check_promote(Pos{ from.row + 1, col }, from.row + 1 == UPPER_PAWN_PROMOTE_ROW);
            }
        }
    }
    else {
        if (from.row == enPassantPos.row){
            if (from.col + 1 == enPassantPos.col || from.col - 1 == enPassantPos.col){
                vec.emplace_back(from, Pos{ from.row - 1, cb.get_en_passant_pos().col }, MoveType::EN_PASSANT);
            }
        }

        if (cb.get(from.row - 1, from.col) == P_EE){
            if (from.row == DOWN_PAWN_BEGIN_ROW && cb.get(from.row - 2, from.col) == P_EE){
                vec.emplace_back(from, Pos{ from.row - 2, from.col }, MoveType::PAWN_2_STEPS);
            }

            add_and_check_promote(Pos{ from.row - 1, from.col }, from.row - 1 == DOWN_PAWN_PROMOTE_ROW);
        }

        for (int32_t col : { from.col + 1, from.col - 1 }){
            Piece p = cb.get(from.row - 1, col);
            Side s = get_side(p);
            
            if (s != PS_EXTRA && s != get_side(fromP)){
                add_and_check_promote(Pos{ from.row - 1, col }, from.row - 1 == DOWN_PAWN_PROMOTE_ROW);
            }
        }
    }
}

void gen_moves_rook(ChessBoard const& cb, Pos from, std::vector<Move>& vec){
    gen_crossing(cb, from, vec);
}

void gen_moves_knight(ChessBoard const& cb, Pos from, std::vector<Move>& vec){
    try_add_possible_move(cb, from, Pos{ from.row + 2, from.col - 1 }, vec);
    try_add_possible_move(cb, from, Pos{ from.row + 2, from.col + 1 }, vec);
    try_add_possible_move(cb, from, Pos{ from.row + 1, from.col - 2 }, vec);
    try_add_possible_move(cb, from, Pos{ from.row + 1, from.col + 2 }, vec);
    try_add_possible_move(cb, from, Pos{ from.row - 1, from.col - 2 }, vec);
    try_add_possible_move(cb, from, Pos{ from.row - 1, from.col + 2 }, vec);
    try_add_possible_move(cb, from, Pos{ from.row - 2, from.col - 1 }, vec);
    try_add_possible_move(cb, from, Pos{ from.row - 2, from.col + 1 }, vec);
}

void gen_moves_bishop(ChessBoard const& cb, Pos from, std::vector<Move>& vec){
    gen_diagonal(cb, from, vec);
}

void gen_moves_queen(ChessBoard const& cb, Pos from, std::vector<Move>& vec){
    gen_crossing(cb, from, vec);
    gen_diagonal(cb, from, vec);
}

void gen_moves_king(ChessBoard const& cb, Pos from, std::vector<Move>& vec){
    try_add_possible_move(cb, from, Pos{ from.row + 1, from.col - 1 }, vec);
    try_add_possible_move(cb, from, Pos{ from.row + 1, from.col + 1 }, vec);
    try_add_possible_move(cb, from, Pos{ from.row + 1, from.col - 1 }, vec);
    try_add_possible_move(cb, from, Pos{ from.row + 1, from.col + 1 }, vec);
    try_add_possible_move(cb, from, Pos{ from.row - 1, from.col - 1 }, vec);
    try_add_possible_move(cb, from, Pos{ from.row - 1, from.col + 1 }, vec);
    try_add_possible_move(cb, from, Pos{ from.row - 1, from.col - 1 }, vec);
    try_add_possible_move(cb, from, Pos{ from.row - 1, from.col + 1 }, vec);

    Piece fromP = cb.get(from);
    switch(get_side(fromP)){
        case PS_UPPER:
            if (cb.can_upper_castle()){
                if (cb.get(from.row, from.col + 1) == P_EE && 
                    cb.get(from.row, from.col + 2) == P_EE && 
                    cb.get(from.row, from.col + 3) == P_UR){
                    vec.emplace_back(from, Pos{ from.row, from.col + 2 }, MoveType::SHORT_CASTLE);
                }

                if (cb.get(from.row, from.col - 1) == P_EE &&
                    cb.get(from.row, from.col - 2) == P_EE && 
                    cb.get(from.row, from.col - 3) == P_EE && 
                    cb.get(from.row, from.col - 4) == P_UR){
                    vec.emplace_back(from, Pos{ from.row, from.col - 2 }, MoveType::LONG_CASTLE);
                }
            }

            break;
        case PS_DOWN:
            if (cb.can_down_castle()){
                if (cb.get(from.row, from.col + 1) == P_EE && 
                    cb.get(from.row, from.col + 2) == P_EE && 
                    cb.get(from.row, from.col + 3) == P_DR){
                    vec.emplace_back(from, Pos{ from.row, from.col + 2 }, MoveType::SHORT_CASTLE);
                }

                if (cb.get(from.row, from.col - 1) == P_EE &&
                    cb.get(from.row, from.col - 2) == P_EE && 
                    cb.get(from.row, from.col - 3) == P_EE && 
                    cb.get(from.row, from.col - 4) == P_DR){
                    vec.emplace_back(from, Pos{ from.row, from.col - 2 }, MoveType::LONG_CASTLE);
                }
            }

            break;
        default:
            break;
    }
}

std::vector<Move> gen_one_position_moves(ChessBoard const& cb, Pos from){
    std::vector<Move> possibleMoves;
    possibleMoves.reserve(30);

    Piece p = cb.get(from);

    switch(get_type(p)){
        case PT_PAWN:
            gen_moves_pawn(cb, from, possibleMoves);
            break;
        case PT_ROOK:
            gen_moves_rook(cb, from, possibleMoves);
            break;
        case PT_KNIGHT:
            gen_moves_knight(cb, from, possibleMoves);
            break;
        case PT_BISHOP:
            gen_moves_bishop(cb, from, possibleMoves);
            break;
        case PT_QUEEN:
            gen_moves_queen(cb, from, possibleMoves);
            break;
        case PT_KING:
            gen_moves_king(cb, from, possibleMoves);
            break;
        default:
            break;
    }

    return possibleMoves;
}

std::vector<Move> gen_one_side_moves(ChessBoard const& cb, Side s){
    std::vector<Move> possibleMoves;
    possibleMoves.reserve(160);

    for (int32_t r = LINE_BEGIN; r < LINE_END; ++r){
        for (int32_t c = LINE_BEGIN; c < LINE_END; ++c){
            Piece p = cb.get(r, c);
            
            if (get_side(p) == s){
                switch(get_type(p)){
                    case PT_PAWN:
                        gen_moves_pawn(cb, Pos{ r, c }, possibleMoves);
                        break;
                    case PT_ROOK:
                        gen_moves_rook(cb, Pos{ r, c }, possibleMoves);
                        break;
                    case PT_KNIGHT:
                        gen_moves_knight(cb, Pos{ r, c }, possibleMoves);
                        break;
                    case PT_BISHOP:
                        gen_moves_bishop(cb, Pos{ r, c }, possibleMoves);
                        break;
                    case PT_QUEEN:
                        gen_moves_queen(cb, Pos{ r, c }, possibleMoves);
                        break;
                    case PT_KING:
                        gen_moves_king(cb, Pos{ r, c }, possibleMoves);
                        break;
                    default:
                        break;
                }
            }
        }
    }

    return possibleMoves;
}
    
float calc_board_score(ChessBoard const& cb){
    float score = 0.0;

    for (int32_t r = LINE_BEGIN; r < LINE_END; ++r){
        for (int32_t c = LINE_BEGIN; c < LINE_END; ++c){
            Piece p = cb.get(r, c);

            if (p != P_EE) {
                score += get_piece_value(p);
                score += get_piece_pos_value(p, r, c);
            }
        }
    }

    return score;
}

float min_max(ChessBoard & cb, int32_t searchDepth, float alpha, float beta, Side side) {
    if (searchDepth == 0) {
        return calc_board_score(cb);
    }

    auto possibleMoves = gen_one_side_moves(cb, side);

    if (side == PS_DOWN) {   // the higher the score, the more advantageous it is for the down side.
        float bestValue = std::numeric_limits<float>::min();
        for (const Move& mv : possibleMoves) {
            cb.move(mv);
            bestValue = std::max(bestValue, min_max(cb, searchDepth - 1, alpha, beta, PS_UPPER));
            cb.undo();

            alpha = std::max(alpha, bestValue);

            /**
             * alpha is lower bound, beta is upper bound.
             * so a value v should suit: alpha < v < beta.
             * if alpha >= beta, then could return.
            */
            if (alpha >= beta) {
                break;
            }
        }

        return bestValue;
    }
    else {   // the lower the score, the more advantageous it is for the upper side.
        float bestValue = std::numeric_limits<float>::max();
        for (const Move& mv : possibleMoves) {
            cb.move(mv);
            bestValue = std::min(bestValue, min_max(cb, searchDepth - 1, alpha, beta, PS_DOWN));
            cb.undo();

            beta = std::min(beta, bestValue);
            if (alpha >= beta) {
                break;
            }
        }

        return bestValue;
    }
}

Move gen_best_move(ChessBoard& cb, Side side, int32_t searchDepth){
    float value;
    float alpha = std::numeric_limits<float>::min();   // lower bound.
    float beta = std::numeric_limits<float>::max();    // upper bound.

    Move bestMove;

    if (side == PS_UPPER){   // the lower the score, the more advantageous it is for the upper side.
        float minValue = std::numeric_limits<float>::max();
        auto possibleMoves = gen_one_side_moves(cb, PS_UPPER);

        for (const Move& mv : possibleMoves){
            cb.move(mv);
            value = min_max(cb, searchDepth, alpha, beta, PS_DOWN);
            cb.undo();

            if (value <= minValue){
                minValue = value;
                bestMove = mv;
            }
        }
    }
    else if (side == PS_DOWN){   // The higher the score, the more advantageous it is for the down side.
        float maxValue = std::numeric_limits<float>::min();
        auto possibleMoves = gen_one_side_moves(cb, PS_DOWN);

        for (const Move& mv : possibleMoves){
            cb.move(mv);
            value = min_max(cb, searchDepth, alpha, beta, PS_UPPER);
            cb.undo();

            if (value >= maxValue){
                maxValue = value;
                bestMove = mv;
            }
        }
    }
    
    return bestMove;
}

bool is_this_your_piece(ChessBoard const& cb, Move const& move, Side s){
    Piece p = cb.get(move.from);
    return get_side(p) == s;
}

bool input_is_move(std::string const& input){
    if (input.size() < 4){
        return false;
    }

    return  (input[0] >= 'a' && input[0] <= 'h') &&
            (input[1] >= '1' && input[1] <= '8') &&
            (input[2] >= 'a' && input[2] <= 'h') &&
            (input[3] >= '1' && input[3] <= '8');
}

Move input_to_move(std::string const& input){
    Move mv;
    mv.from.row = LINE_BEGIN + '8' - input[1];
    mv.from.col = LINE_BEGIN + input[0] - 'a';
    mv.to.row   = LINE_BEGIN + '8' - input[3];
    mv.to.col   = LINE_BEGIN + input[2] - 'a';

    return mv;
}

std::string move_to_str(Move const& mv){
    std::string buf;

    buf += static_cast<char>(mv.from.col - LINE_BEGIN + 'a');
    buf += static_cast<char>(8 - (mv.from.row - LINE_BEGIN) + '0');
    buf += static_cast<char>(mv.to.col - LINE_BEGIN + 'a');
    buf += static_cast<char>(8 - (mv.to.row - LINE_BEGIN) + '0');
    return buf;
}

bool check_rule(ChessBoard const& cb, Move& mv){
    std::vector<Move> possibleMoves = gen_one_position_moves(cb, mv.from);
    auto iter = std::find(possibleMoves.cbegin(), possibleMoves.cend(), mv);

    if (iter != possibleMoves.cend()){
        mv.moveType = iter->moveType;
        return true;
    }
    else {
        mv.moveType = MoveType::INVALID;
        return false;
    }
}

Side check_winner(ChessBoard const& cb){
    bool findUpperKing = false;
    bool findDownKing = false;

    for (int32_t r = LINE_BEGIN; r < LINE_END; ++r) {
        for (int32_t c = LINE_BEGIN; c < LINE_END; ++c){
            Piece p = cb.get(r, c);

            if (p == P_UK){
                findUpperKing = true;
            }
            else if (p == P_DK){
                findDownKing = true;
            }
        }
    }

    if (findUpperKing && findDownKing){
        return Side::PS_EXTRA;
    }
    else {
        return findUpperKing ? PS_UPPER : PS_DOWN;
    }
}

class ConsoleColor {
    #ifdef _WIN32
    HANDLE hOutHandle;
    WORD oldColorAttrs;

    // https://stackoverflow.com/questions/17125440/c-win32-console-color
    WORD get_windows_color_attr(int color) {
        switch(color) {
            case Black: 
                return 0;
            case Blue: 
                return FOREGROUND_BLUE;
            case Green: 
                return FOREGROUND_GREEN;
            case Cyan: 
                return FOREGROUND_GREEN | FOREGROUND_BLUE;
            case Red: 
                return FOREGROUND_RED;
            case Magenta: 
                return FOREGROUND_RED | FOREGROUND_BLUE;
            case Yellow: 
                return FOREGROUND_RED | FOREGROUND_GREEN;
            case White:
                return FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
            case BoldBlack: 
                return 0 | FOREGROUND_INTENSITY;
            case BoldBlue: 
                return FOREGROUND_BLUE | FOREGROUND_INTENSITY;
            case BoldGreen: 
                return FOREGROUND_GREEN | FOREGROUND_INTENSITY;
            case BoldCyan: 
                return FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
            case BoldRed: 
                return FOREGROUND_RED | FOREGROUND_INTENSITY;
            case BoldMagenta: 
                return FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
            case BoldYellow: 
                return FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
            case BoldWhite:
            default:
                return FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
        };
    }
    #endif
public:
    enum {
        Black, 
        Red, 
        Green, 
        Yellow, 
        Blue, 
        Magenta, 
        Cyan, 
        White,
        BoldBlack, 
        BoldRed, 
        BoldGreen, 
        BoldYellow, 
        BoldBlue, 
        BoldMagenta, 
        BoldCyan, 
        BoldWhite
    };

    ConsoleColor() {
    #ifdef _WIN32
        CONSOLE_SCREEN_BUFFER_INFO csbiInfo;
        hOutHandle = GetStdHandle(STD_OUTPUT_HANDLE);
        GetConsoleScreenBufferInfo(hOutHandle, &csbiInfo);
        oldColorAttrs = csbiInfo.wAttributes;
    #endif
    }

    ~ConsoleColor() noexcept {
        reset();
    }

    void set(int color) noexcept {
        #ifdef __linux__
        switch(color) {
            case Black:
                std::cout << "\033[30m"; break;
            case Red:
                std::cout << "\033[31m"; break;
            case Green:
                std::cout << "\033[32m"; break;
            case Yellow:       
                std::cout << "\033[33m"; break;
            case Blue:         
                std::cout << "\033[34m"; break;
            case Magenta:      
                std::cout << "\033[35m"; break;
            case Cyan:         
                std::cout << "\033[36m"; break;
            case White:        
                std::cout << "\033[37m"; break;
            case BoldBlack:    
                std::cout << "\033[1m\033[30m"; break;
            case BoldRed:      
                std::cout << "\033[1m\033[31m"; break;
            case BoldGreen:    
                std::cout << "\033[1m\033[32m"; break;
            case BoldYellow:   
                std::cout << "\033[1m\033[33m"; break;
            case BoldBlue:     
                std::cout << "\033[1m\033[34m"; break;
            case BoldMagenta:  
                std::cout << "\033[1m\033[35m"; break;
            case BoldCyan:     
                std::cout << "\033[1m\033[36m"; break;
            case BoldWhite:  
            default:  
                std::cout << "\033[1m\033[37m"; break;
        }
        #elif defined(_WIN32)
            SetConsoleTextAttribute(hOutHandle, get_windows_color_attr(color));
        #endif
    }

    void reset() noexcept {
        #ifdef __linux__
            std::cout << "\033[0m";
        #elif defined(_WIN32)
            SetConsoleTextAttribute(hOutHandle, oldColorAttrs);
        #endif
    }
};

void print_board(ChessBoard const& cb, ConsoleColor& cc) {
    std::cout << "\n    +-----------------+\n";

    int32_t n = 8;
    Piece p;

    for (int32_t r = LINE_BEGIN; r < LINE_END; ++r) {
        cc.set(ConsoleColor::Yellow);
        std::cout << " " << n--;
        cc.reset();

        std::cout << "  | ";

        for (int32_t c = LINE_BEGIN; c < LINE_END; ++c) {
            p = cb.get(r, c);

            if (get_side(p) == PS_UPPER) {
                cc.set(ConsoleColor::BoldBlue);
            }
            else if (get_side(p) == PS_DOWN) {
                cc.set(ConsoleColor::BoldRed);
            }
            else {
                cc.set(ConsoleColor::White);
            }

            std::cout << get_str(p) << " ";
            cc.reset();
        }

        std::cout << "|\n";
    }

    std::cout << "    +-----------------+\n";

    cc.set(ConsoleColor::BoldGreen);
    std::cout << "\n      a b c d e f g h\n\n";
    cc.reset();
}

void print_help_page(){
    std::cout << "\n=======================================\n";
    std::cout << "Help Page\n\n";
    std::cout << "    1. help         - this page.\n";
    std::cout << "    2. b2e2         - input like this will be parsed as a move.\n";
    std::cout << "    3. undo         - undo the previous move.\n";
    std::cout << "    4. exit or quit - exit the game.\n";
    std::cout << "    5. remake       - remake the game.\n";
    std::cout << "    6. advice       - give me a best move.\n\n";
    std::cout << "  The characters on the board have the following relationships: \n\n";
    std::cout << "    P -> AI side pawn.\n";
    std::cout << "    R -> AI side rook.\n";
    std::cout << "    N -> AI side knight.\n";
    std::cout << "    B -> AI side bishop.\n";
    std::cout << "    Q -> AI side queen.\n";
    std::cout << "    K -> AI side king.\n";
    std::cout << "    p -> our pawn.\n";
    std::cout << "    r -> our rook.\n";
    std::cout << "    n -> our knight.\n";
    std::cout << "    b -> our bishop.\n";
    std::cout << "    q -> our queen.\n";
    std::cout << "    k -> our king.\n";
    std::cout << "    . -> no piece here.\n";
    std::cout << "=======================================\n";
    std::cout << "Press any key to continue.\n";

    std::string ignore_input;    
    std::getline(std::cin, ignore_input);
}

int main() {
    Side userSide = PS_DOWN;
    Side aiSide = PS_UPPER;
    int32_t searchDepth = 5;

    load_piece_value_mapping();
    load_piece_pos_value_mapping();

    ConsoleColor cc;
    ChessBoard cb;
    std::string userInput;

    print_board(cb, cc);

    while (1){
        std::cout << "Your move: ";
        std::getline(std::cin, userInput);

        if (userInput == "help"){
            print_help_page();
            print_board(cb, cc);
        }
        else if (userInput == "undo"){
            cb.undo();
            cb.undo();
            print_board(cb, cc);
        }
        else if (userInput == "quit"){
            return 0;
        }
        else if (userInput == "exit"){
            return 0;
        }
        else if (userInput == "remake"){
            cb = ChessBoard{};

            std::cout << "New cnchess started.\n";
            print_board(cb, cc);
            continue;
        }
        else if (userInput == "advice"){
            Move advice = gen_best_move(cb, userSide, searchDepth);
            std::string adviceStr = move_to_str(advice);
            std::cout << "Maybe you can try: " << adviceStr 
                        << ", piece is " << get_str(cb.get(advice.from))
                        << ".\n";
        }
        else{
            if (input_is_move(userInput)){
                Move userMove = input_to_move(userInput);
                
                if (!is_this_your_piece(cb, userMove, userSide)){
                    std::cout << "This piece is not yours, please choose your piece.\n";
                    continue;
                }

                if (check_rule(cb, userMove)){
                    if (userMove.moveType == MoveType::GO_AND_PROMOTE) {
                        cb.move(userMove, P_DQ);
                    }
                    else{
                        cb.move(userMove);
                    }

                    print_board(cb, cc);

                    if (check_winner(cb) == userSide){
                        std::cout << "Congratulations! You win!\n";
                    }

                    std::cout << "AI thinking...\n";
                    Move aiMove = gen_best_move(cb, aiSide, searchDepth);
                    std::string aiMoveStr = move_to_str(aiMove);
                    cb.move(aiMove);
                    print_board(cb, cc);
                    std::cout << "AI move: " << aiMoveStr
                             << ", piece is '" << get_str(cb.get(aiMove.to)) 
                             << "'.\n";

                    if (check_winner(cb) == aiSide){
                        std::cout << "Game over! You lose!\n";
                    }
                }
                else {
                    std::cout << "Given move doesn't fit for rules, please re-enter.\n";
                    continue;
                }
            }
            else {
                std::cout << "Input is not a valid move nor instruction, please re-enter(try help ?).\n";
                continue;
            }
        }
    }

    return 0;
}
