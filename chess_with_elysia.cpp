/*
    @author elysia

    chess game written in C++20. 
    This game supports an AI called Elysia, she is based on alpha-beta pruning.
    This game is running under the command, and it supports colorful output.

    If you have any questions, please contact me in email with: yangtianyuan2023@163.com
    I'm very glad to hear from your feedback.
*/
#include <iostream>
#include <fstream>
#include <algorithm>
#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>
#include <array>
#include <deque>
#include <exception>
#include <stdexcept>
#include <chrono>
#include <map>
#include <future>
#include <span>
#include <cstdlib>
#include <cstdint>
#include <cassert>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

using namespace std::string_view_literals;

enum class Side {
    upper,
    down,
    extra
};

enum class Type {
    pawn,
    rook,
    knight,
    bishop,
    queen,
    king,
    empty,
    out
};

enum class MoveType {
    invalid,
    normal,
    en_passant,
    long_castling,
    short_castling,
    pawn_move_and_promote,
    pawn_2_steps
};

using Piece = char;

// pieces.
static constexpr Piece P_UP = 'P';
static constexpr Piece P_UR = 'R';
static constexpr Piece P_UN = 'N';
static constexpr Piece P_UB = 'B';
static constexpr Piece P_UQ = 'Q';
static constexpr Piece P_UK = 'K';
static constexpr Piece P_DP = 'p';
static constexpr Piece P_DR = 'r';
static constexpr Piece P_DN = 'n';
static constexpr Piece P_DB = 'b';
static constexpr Piece P_DQ = 'q';
static constexpr Piece P_DK = 'k';
static constexpr Piece P_EE = '.';
static constexpr Piece P_EO = '#';

struct Pos {
    int32_t row;
    int32_t col;

    Pos() : row{ 0 }, col{ 0 } {}
    Pos(int32_t row, int32_t col) : row{ row }, col{ col } {}
    
    bool operator==(const Pos& other) const noexcept { return row == other.row && col == other.col; }
    bool operator!=(const Pos& other) const noexcept { return !(*this == other); }
};

struct Move {
    Pos from, to;
    MoveType moveType;
    Piece promoteP;

    Move() 
        : from{ 0, 0 }, to{ 0, 0 }, moveType{ MoveType::invalid }, promoteP{ P_EO } 
    {}

    Move(Pos from, Pos to, MoveType moveType) 
        : from{ from }, to{ to }, moveType{ moveType }, promoteP{ P_EO } 
    {}

    Move(Pos from, Pos to, MoveType moveType, Piece _promoteP) 
        : from{ from }, to{ to }, moveType{ moveType }, promoteP{ _promoteP } 
    {}

    // no need to check the moveType and promoteP.
    bool operator==(const Move& other) const noexcept { return from == other.from && to == other.to; }
    bool operator!=(const Move& other) const noexcept { return !(*this == other); }
};

constexpr Type piece_type(Piece p) noexcept { 
    switch(p){
        case P_UP:
        case P_DP:
            return Type::pawn;
        case P_UR:
        case P_DR:
            return Type::rook;
        case P_UN:
        case P_DN:
            return Type::knight;
        case P_UB:
        case P_DB:
            return Type::bishop;
        case P_UQ:
        case P_DQ:
            return Type::queen;
        case P_UK:
        case P_DK:
            return Type::king;
        case P_EE:
            return Type::empty;
        default:
            return Type::out;
    }
}

constexpr Side piece_side(Piece p) noexcept {
    switch(p){
        case P_UP: 
        case P_UR: 
        case P_UN: 
        case P_UB: 
        case P_UQ: 
        case P_UK:
            return Side::upper;
        case P_DP:
        case P_DR:
        case P_DN:
        case P_DB:
        case P_DQ:
        case P_DK:
            return Side::down;
        default:
            return Side::extra;
    }
}

class Board {
    std::string data;
    std::deque<std::string> history;

    static constexpr int32_t upper_king_start_pos = 30;
    static constexpr int32_t down_king_start_pos = 114;

    static constexpr int32_t upper_castle_flag_pos = 144;
    static constexpr int32_t down_castle_flag_pos = 145;
    static constexpr int32_t en_passant_row_pos = 146;
    static constexpr int32_t en_passant_col_pos = 147;

    void set(int32_t r, int32_t c, Piece p) noexcept {
        data[r * width + c] = p;
    }

    void set(Pos pos, Piece p) noexcept {
        set(pos.row, pos.col, p);
    }

    bool get_upper_castle_flag() const noexcept {
        return data[upper_castle_flag_pos] - '0';
    }

    bool get_down_castle_flag() const noexcept {
        return data[down_castle_flag_pos] - '0';
    }

    void set_upper_castle_flag(bool flag) noexcept {
        data[upper_castle_flag_pos] = (flag ? '1' : '0');
    }

    void set_down_castle_flag(bool flag) noexcept {
        data[down_castle_flag_pos] = (flag ? '1' : '0');
    }

    void set_en_passant_pos(int32_t r, int32_t c) noexcept {
        data[en_passant_row_pos] = r + '0';
        data[en_passant_col_pos] = c + '0';
    }

    void reset_en_passant_pos() noexcept {
        set_en_passant_pos(0, 0);
    }
public:
    // to speed up bounds checking, expand the 8x8 board to 12x12.
    static constexpr int32_t width = 12;

    static constexpr int32_t line_begin = 2;
    static constexpr int32_t line_end = 9;

    static constexpr int32_t upper_pawn_begin_row = 3;
    static constexpr int32_t upper_pawn_promote_row = 9;
    static constexpr int32_t down_pawn_begin_row = 8;
    static constexpr int32_t down_pawn_promote_row = 2;

    Board() {
        reset();
    }

    void reset() {
        data = "############"
                "############"
                "##RNBQKBNR##"
                "##PPPPPPPP##"
                "##........##"
                "##........##"
                "##........##"
                "##........##"
                "##pppppppp##"
                "##rnbqkbnr##"
                "############"
                "############"
                "1100";

        history.clear();
    }

    bool can_upper_short_castle() const noexcept {
        if (get_upper_castle_flag()) {
            return std::string_view{ data.data() + upper_king_start_pos + 1, 3 } == "..R"sv;
        }

        return false;
    }

    bool can_upper_long_castle() const noexcept {
        if (get_upper_castle_flag()) {
            return std::string_view{ data.data() + upper_king_start_pos - 4, 4 } == "R..."sv;
        }

        return false;
    }

    bool can_down_short_castle() const noexcept {
        if (get_down_castle_flag()) {
            return std::string_view{ data.data() + down_king_start_pos + 1, 3 } == "..r"sv;
        }

        return false;
    }

    bool can_down_long_castle() const noexcept {
        if (get_down_castle_flag()) {
            return std::string_view{ data.data() + down_king_start_pos - 4, 4 } == "r..."sv;
        }

        return false;
    }

    bool has_chance_to_do_en_passant() const noexcept {
        return data[en_passant_row_pos] != '0' && data[en_passant_col_pos] != '0';
    }

    Pos en_passant_pos() const noexcept {
        return Pos {
            line_begin + data[en_passant_row_pos] - '0',
            line_begin + data[en_passant_col_pos] - '0'
        };
    }

    Piece get(int32_t r, int32_t c) const noexcept {
        return data[r * width + c];
    }

    Piece get(Pos pos) const noexcept {
        return get(pos.row, pos.col);
    }
    
    void move(const Move& mv) {
        if (mv.moveType == MoveType::invalid) {
            return;
        }

        history.emplace_back(data);

        Piece fromP = get(mv.from);
        reset_en_passant_pos();

        if (mv.moveType == MoveType::pawn_move_and_promote) {
            set(mv.to, mv.promoteP);
            set(mv.from, P_EE);
            return;
        }

        set(mv.to, fromP);
        set(mv.from, P_EE);

        if (fromP == P_UK) {
            set_upper_castle_flag(false);
        }
        else if (fromP == P_DK) {
            set_down_castle_flag(false);
        }

        if (mv.moveType == MoveType::long_castling) {
            set(mv.from.row, mv.from.col - 1, get(mv.from.row, mv.from.col - 4));
            set(mv.from.row, mv.from.col - 4, P_EE);
        }
        else if (mv.moveType == MoveType::short_castling) {
            set(mv.from.row, mv.from.col + 1, get(mv.from.row, mv.from.col + 3));
            set(mv.from.row, mv.from.col + 3, P_EE);
        }
        else if (mv.moveType == MoveType::en_passant) {
            set(mv.from.row, mv.to.col, P_EE);
        }
        else if (mv.moveType == MoveType::pawn_2_steps) {
            Side s = piece_side(fromP);
            Piece reversePawn = (s == Side::upper ? P_DP : P_UP);

            if (get(mv.from.row, mv.from.col - 1) == reversePawn) {
                set_en_passant_pos(mv.from.row, mv.from.col - 1);
            }
            else if (get(mv.from.row, mv.from.col + 1) == reversePawn) {
                set_en_passant_pos(mv.from.row, mv.from.col + 1);
            }
        }
    }

    void undo() {
        if (!history.empty()) {
            data = std::move(history.back());
            history.pop_back();
        }
    }
};

class MovesGen {
    static bool try_add_possible_move(const Board& board, Pos from, Pos to, std::vector<Move>& vec){
        Piece fromP = board.get(from);
        Piece toP = board.get(to);

        if (toP == P_EO) {
            return false;
        }
        else if (toP == P_EE) {
            vec.emplace_back(from, to, MoveType::normal);
            return true;
        }
        else {
            // because this function could be used in for-loop, when we meets a enemy, this for-loop could be done.
            if (piece_side(fromP) != piece_side(toP)) {
                vec.emplace_back(from, to, MoveType::normal);
            }

            return false;
        }
    }

    static void gen_crossing(const Board& board, Pos from, std::vector<Move>& vec) {
        for (int32_t rr = from.row - 1; try_add_possible_move(board, from, Pos{ rr, from.col }, vec); --rr);
        for (int32_t rr = from.row + 1; try_add_possible_move(board, from, Pos{ rr, from.col }, vec); ++rr);
        for (int32_t cc = from.col - 1; try_add_possible_move(board, from, Pos{ from.row, cc }, vec); --cc);
        for (int32_t cc = from.col + 1; try_add_possible_move(board, from, Pos{ from.row, cc }, vec); ++cc);
    }

    static void gen_diagonal(const Board& board, Pos from, std::vector<Move>& vec) {
        for (int32_t rr = from.row - 1, cc = from.col - 1; try_add_possible_move(board, from, Pos{ rr, cc }, vec); --rr, --cc);
        for (int32_t rr = from.row - 1, cc = from.col + 1; try_add_possible_move(board, from, Pos{ rr, cc }, vec); --rr, ++cc);
        for (int32_t rr = from.row + 1, cc = from.col - 1; try_add_possible_move(board, from, Pos{ rr, cc }, vec); ++rr, --cc);
        for (int32_t rr = from.row + 1, cc = from.col + 1; try_add_possible_move(board, from, Pos{ rr, cc }, vec); ++rr, ++cc);
    }

    static void pawn_add_and_check_promote(Side s, Pos from, Pos to, bool canPromote, std::vector<Move>& vec) {
        if (canPromote) {
            vec.emplace_back(from, to, MoveType::pawn_move_and_promote, (s == Side::upper ? P_UR : P_DR));
            vec.emplace_back(from, to, MoveType::pawn_move_and_promote, (s == Side::upper ? P_UN : P_DN));
            vec.emplace_back(from, to, MoveType::pawn_move_and_promote, (s == Side::upper ? P_UB : P_DB));
            vec.emplace_back(from, to, MoveType::pawn_move_and_promote, (s == Side::upper ? P_UQ : P_DQ));
        }
        else {
            vec.emplace_back(from, to, MoveType::normal);
        }
    }

    static void pawn_steps_upper(const Board& board, Pos from, std::vector<Move>& vec) {
        if (board.has_chance_to_do_en_passant()) {
            Pos en_passant_pos = board.en_passant_pos();

            if (from.row == en_passant_pos.row) {
                if (from.col + 1 == en_passant_pos.col || from.col - 1 == en_passant_pos.col) {
                    vec.emplace_back(from, Pos{ from.row + 1, en_passant_pos.col }, MoveType::en_passant);
                }
            }
        }

        if (board.get(from.row + 1, from.col) == P_EE){
            if (from.row == Board::upper_pawn_begin_row && board.get(from.row + 2, from.col) == P_EE){
                vec.emplace_back(from, Pos{ from.row + 2, from.col }, MoveType::pawn_2_steps);
            }

            pawn_add_and_check_promote(Side::upper, from, Pos{ from.row + 1, from.col }, from.row + 1 == Board::upper_pawn_promote_row, vec);
        }

        for (int32_t col : std::initializer_list<int32_t>{ from.col + 1, from.col - 1 }){
            Piece p = board.get(from.row + 1, col);
            Side s = piece_side(p);
            
            if (s != Side::extra && s != piece_side(board.get(from))) {
                pawn_add_and_check_promote(Side::upper, from, Pos{ from.row + 1, col }, from.row + 1 == Board::upper_pawn_promote_row, vec);
            }
        }
    }

    static void pawn_steps_down(const Board& board, Pos from, std::vector<Move>& vec) {
        if (board.has_chance_to_do_en_passant()) {
            Pos en_passant_pos = board.en_passant_pos();

            if (from.row == en_passant_pos.row) {
                if (from.col + 1 == en_passant_pos.col || from.col - 1 == en_passant_pos.col) {
                    vec.emplace_back(from, Pos{ from.row - 1, en_passant_pos.col }, MoveType::en_passant);
                }
            }
        }

        if (board.get(from.row - 1, from.col) == P_EE){
            if (from.row == Board::down_pawn_begin_row && board.get(from.row - 2, from.col) == P_EE){
                vec.emplace_back(from, Pos{ from.row - 2, from.col }, MoveType::pawn_2_steps);
            }

            pawn_add_and_check_promote(Side::down, from, Pos{ from.row - 1, from.col }, from.row - 1 == Board::down_pawn_promote_row, vec);
        }

        for (int32_t col : std::initializer_list<int32_t>{ from.col + 1, from.col - 1 }){
            Piece p = board.get(from.row - 1, col);
            Side s = piece_side(p);
            
            if (s != Side::extra && s != piece_side(board.get(from))) {
                pawn_add_and_check_promote(Side::down, from, Pos{ from.row - 1, col }, from.row - 1 == Board::down_pawn_promote_row, vec);
            }
        }
    }

    static void pawn_steps(const Board& board, Pos from, std::vector<Move>& vec) {
        if (board.get(from) == P_UP) {
            pawn_steps_upper(board, from, vec);
        }
        else {
            pawn_steps_down(board, from, vec);
        }
    }

    static void rook_steps(const Board& board, Pos from, std::vector<Move>& vec) {
        gen_crossing(board, from, vec);
    }

    static void knight_steps(const Board& board, Pos from, std::vector<Move>& vec) {
        try_add_possible_move(board, from, Pos{ from.row + 2, from.col - 1 }, vec);
        try_add_possible_move(board, from, Pos{ from.row + 2, from.col + 1 }, vec);
        try_add_possible_move(board, from, Pos{ from.row + 1, from.col - 2 }, vec);
        try_add_possible_move(board, from, Pos{ from.row + 1, from.col + 2 }, vec);
        try_add_possible_move(board, from, Pos{ from.row - 1, from.col - 2 }, vec);
        try_add_possible_move(board, from, Pos{ from.row - 1, from.col + 2 }, vec);
        try_add_possible_move(board, from, Pos{ from.row - 2, from.col - 1 }, vec);
        try_add_possible_move(board, from, Pos{ from.row - 2, from.col + 1 }, vec);
    }

    static void bishop_steps(const Board& board, Pos from, std::vector<Move>& vec) {
        gen_diagonal(board, from, vec);
    }

    static void queen_steps(const Board& board, Pos from, std::vector<Move>& vec) {
        gen_crossing(board, from, vec);
        gen_diagonal(board, from, vec);
    }

    static void king_steps(const Board& board, Pos from, std::vector<Move>& vec) {
        try_add_possible_move(board, from, Pos{ from.row - 1, from.col - 1 }, vec);
        try_add_possible_move(board, from, Pos{ from.row - 1, from.col     }, vec);
        try_add_possible_move(board, from, Pos{ from.row - 1, from.col + 1 }, vec);
        try_add_possible_move(board, from, Pos{ from.row    , from.col - 1 }, vec);
        try_add_possible_move(board, from, Pos{ from.row    , from.col + 1 }, vec);
        try_add_possible_move(board, from, Pos{ from.row + 1, from.col - 1 }, vec);
        try_add_possible_move(board, from, Pos{ from.row + 1, from.col     }, vec);
        try_add_possible_move(board, from, Pos{ from.row + 1, from.col + 1 }, vec);

        Piece fromP = board.get(from);
        Side s = piece_side(fromP);

        if (s == Side::upper) {
            if (board.can_upper_short_castle()) {
                vec.emplace_back(from, Pos{ from.row, from.col + 2 }, MoveType::short_castling);
            }
                
            if (board.can_upper_long_castle()) {
                vec.emplace_back(from, Pos{ from.row, from.col - 2 }, MoveType::long_castling);
            }
        }
        else {
            if (board.can_down_short_castle()) {
                vec.emplace_back(from, Pos{ from.row, from.col + 2 }, MoveType::short_castling);
            }
                
            if (board.can_down_long_castle()) {
                vec.emplace_back(from, Pos{ from.row, from.col - 2 }, MoveType::long_castling);
            }
        }
    }
public:
    static std::vector<Move> gen_moves_for_one_side(const Board& board, Side s) {
        assert(s != Side::extra);

        std::vector<Move> possibleMoves;
        possibleMoves.reserve(256);

        for (int32_t r = Board::line_begin; r <= Board::line_end; ++r){
            for (int32_t c = Board::line_begin; c <= Board::line_end; ++c){
                Piece p = board.get(r, c);
                
                if (piece_side(p) == s) {
                    switch(piece_type(p)) {
                        case Type::pawn:
                            pawn_steps(board, Pos{ r, c }, possibleMoves);
                            break;
                        case Type::rook:
                            rook_steps(board, Pos{ r, c }, possibleMoves);
                            break;
                        case Type::knight:
                            knight_steps(board, Pos{ r, c }, possibleMoves);
                            break;
                        case Type::bishop:
                            bishop_steps(board, Pos{ r, c }, possibleMoves);
                            break;
                        case Type::queen:
                            queen_steps(board, Pos{ r, c }, possibleMoves);
                            break;
                        case Type::king:
                            king_steps(board, Pos{ r, c }, possibleMoves);
                            break;
                        default:
                            break;
                    }
                }
            }
        }

        return possibleMoves;
    }
};

using PosValue = std::array<std::array<float, Board::width>, Board::width>;

static std::map<Piece, float> piece_value;
static std::map<Piece, PosValue> piece_pos_value;

class ScoreEvaluator {
    static void load_single_piece_value(Piece p, std::ifstream& in) {
        float value;
        in >> value;

        if (!in) {
            throw std::runtime_error{ "load_single_piece_value: file maybe broken" };
        }

        piece_value[p] = value;
    }

    static void load_piece_values() {
        std::string path = "pvalues.txt";

        std::ifstream in { path };
        if (!in.is_open()) {
            throw std::runtime_error{ "load_piece_values: cannot open file: " + path };
        }

        load_single_piece_value(P_UP, in);
        load_single_piece_value(P_UR, in);
        load_single_piece_value(P_UN, in);
        load_single_piece_value(P_UB, in);
        load_single_piece_value(P_UQ, in);
        load_single_piece_value(P_UK, in);
        load_single_piece_value(P_DP, in);
        load_single_piece_value(P_DR, in);
        load_single_piece_value(P_DN, in);
        load_single_piece_value(P_DB, in);
        load_single_piece_value(P_DQ, in);
        load_single_piece_value(P_DK, in);
    }

    static void load_single_piece_pos_value(Piece p, const std::string& path) {
        std::ifstream in { path };
        if (!in.is_open()) {
            throw std::runtime_error{ "load_single_piece_pos_value: cannot open file: " + path };
        }

        PosValue posValue;

        for (int32_t r = 0; r < Board::width; ++r) {
            for (int32_t c = 0; c < Board::width; ++c) {
                posValue[r][c] = 0;
            }
        }

        for (int32_t r = Board::line_begin; r <= Board::line_end; ++r) {
            for (int32_t c = Board::line_begin; c <= Board::line_end; ++c) {
                in >> posValue[r][c];

                if (!in) {
                    throw std::runtime_error{ "load_single_piece_pos_value: file maybe broken: " + path };
                }
            }
        }

        piece_pos_value[p] = posValue;
    }

    static void load_piece_pos_values() {
        load_single_piece_pos_value(P_UP, "pos_value_upper_pawn.txt");
        load_single_piece_pos_value(P_UR, "pos_value_upper_rook.txt");
        load_single_piece_pos_value(P_UN, "pos_value_upper_knight.txt");
        load_single_piece_pos_value(P_UB, "pos_value_upper_bishop.txt");
        load_single_piece_pos_value(P_UQ, "pos_value_upper_queen.txt");
        load_single_piece_pos_value(P_UK, "pos_value_upper_king.txt");
        load_single_piece_pos_value(P_DP, "pos_value_down_pawn.txt");
        load_single_piece_pos_value(P_DR, "pos_value_down_rook.txt");
        load_single_piece_pos_value(P_DN, "pos_value_down_knight.txt");
        load_single_piece_pos_value(P_DB, "pos_value_down_bishop.txt");
        load_single_piece_pos_value(P_DQ, "pos_value_down_queen.txt");
        load_single_piece_pos_value(P_DK, "pos_value_down_king.txt");
    }
public:
    static void init_values() {
        load_piece_values();
        load_piece_pos_values();
    }

    // the bigger the score, the better for down side.
    static float evaluate(const Board& board) {
        float score = 0.0f;

        for (int32_t r = Board::line_begin; r <= Board::line_end; ++r) {
            for (int32_t c = Board::line_begin; c <= Board::line_end; ++c) {
                Piece p = board.get(r, c);

                if (p != P_EE) {
                    score += piece_value[p];
                    score += piece_pos_value[p][r][c];
                }
            }
        }

        return score;
    }
};

/**
 * for single thread.
*/
class BestMoveGen {
    static constexpr float LOWER_BOUND = -500'0000.0f;  // be cautions here, if you use std::numeric_limits<float>::min, that's a number near 0, not negative.
    static constexpr float UPPER_BOUND = 500'0000.0f;

    // upper is min, down is max.
    static float min_max(Board& board, int32_t thinkDepth, float alpha, float beta, bool isMax) {
        if (thinkDepth == 0) {
            return ScoreEvaluator::evaluate(board);
        }

        if (isMax) {
            auto moves = MovesGen::gen_moves_for_one_side(board, Side::down);
            float bestVal = LOWER_BOUND;

            for (const Move& mv : moves) {
                board.move(mv);
                bestVal = std::max(bestVal, min_max(board, thinkDepth - 1, alpha, beta, !isMax));
                board.undo();

                alpha = std::max(alpha, bestVal);

                if (alpha >= beta) {
                    break;
                }
            }

            return bestVal;
        }
        else {
            auto moves = MovesGen::gen_moves_for_one_side(board, Side::upper);
            float bestVal = UPPER_BOUND;

            for (const Move& mv : moves) {
                board.move(mv);
                bestVal = std::min(bestVal, min_max(board, thinkDepth - 1, alpha, beta, !isMax));
                board.undo();

                beta = std::min(beta, bestVal);

                if (alpha >= beta) {
                    break;
                }
            }

            return bestVal;
        }
    }
public:
    // upper is min, down is max.
    static Move gen_best_for(Board& board, Side s, int32_t thinkDepth) {
        assert(s != Side::extra);

        float alpha = LOWER_BOUND;
        float beta = UPPER_BOUND;
        Move bestMove;

        if (s == Side::upper) {
            float minValue = UPPER_BOUND;
            auto moves = MovesGen::gen_moves_for_one_side(board, Side::upper);

            for (const Move& mv : moves) {
                board.move(mv);
                float val = min_max(board, thinkDepth, alpha, beta, true);
                board.undo();

                if (val <= minValue) {
                    minValue = val;
                    bestMove = mv;
                }
            }
        }
        else {
            float maxValue = LOWER_BOUND;
            auto moves = MovesGen::gen_moves_for_one_side(board, Side::down);

            for (const Move& mv : moves) {
                board.move(mv);
                float val = min_max(board, thinkDepth, alpha, beta, false);
                board.undo();

                if (val >= maxValue) {
                    maxValue = val;
                    bestMove = mv;
                }
            }
        }

        return bestMove;
    }
};

/**
 * on multi-process computer, this works very fast!
*/
class BestMoveGenParallel {
    static constexpr float LOWER_BOUND = -500'0000.0f;  // be cautions here, if you use std::numeric_limits<float>::min, that's a number near 0, not negative.
    static constexpr float UPPER_BOUND = 500'0000.0f;
    static constexpr int32_t split_chunk_num = 32;

    static std::vector<std::span<const Move>> 
    split_vector(const std::vector<Move>& vec, size_t chunkNum) {
        std::vector<std::span<const Move>> result;

        size_t chunkLength = vec.size() / chunkNum;
        if (chunkLength == 0) {
            chunkNum = vec.size();
            chunkLength = 1;
        }
        
        size_t counter;
        for (counter = 0; counter != chunkNum - 1; ++counter) {
            result.emplace_back(vec.begin() + counter * chunkLength, chunkLength);
        }

        result.emplace_back(vec.begin() + counter * chunkLength, vec.end());
        return result;
    }

    // upper is min, down is max.
    static float min_max(Board& board, int32_t thinkDepth, float alpha, float beta, bool isMax) {
        if (thinkDepth == 0) {
            return ScoreEvaluator::evaluate(board);
        }

        if (isMax) {
            auto moves = MovesGen::gen_moves_for_one_side(board, Side::down);
            float bestVal = LOWER_BOUND;

            for (const Move& mv : moves) {
                board.move(mv);
                bestVal = std::max(bestVal, min_max(board, thinkDepth - 1, alpha, beta, !isMax));
                board.undo();

                alpha = std::max(alpha, bestVal);

                if (alpha >= beta) {
                    break;
                }
            }

            return bestVal;
        }
        else {
            auto moves = MovesGen::gen_moves_for_one_side(board, Side::upper);
            float bestVal = UPPER_BOUND;

            for (const Move& mv : moves) {
                board.move(mv);
                bestVal = std::min(bestVal, min_max(board, thinkDepth - 1, alpha, beta, !isMax));
                board.undo();

                beta = std::min(beta, bestVal);

                if (alpha >= beta) {
                    break;
                }
            }

            return bestVal;
        }
    }
public:
    static Move gen_best_for(Board& board, Side s, int32_t thinkDepth) {
        assert(s != Side::extra);

        if (s == Side::upper) {
            auto moves = MovesGen::gen_moves_for_one_side(board, Side::upper);
            auto splitMoves = split_vector(moves, split_chunk_num);

            std::vector<Move> bestMoves;
            std::vector<float> bestValues;
            std::vector<std::future<void>> tasks;

            bestMoves.resize(splitMoves.size());
            bestValues.resize(splitMoves.size());
            tasks.resize(splitMoves.size());

            for (size_t i = 0; i < splitMoves.size(); ++i) {
                tasks[i] = std::async([&board, &bestMoves, &bestValues, &splitMoves, i, thinkDepth]() {
                    Board tempBoard = board;

                    float minValue = UPPER_BOUND;
                    float alpha = LOWER_BOUND;
                    float beta = UPPER_BOUND;
                    Move bestMove;

                    for (const Move& mv : splitMoves[i]) {
                        tempBoard.move(mv);
                        float val = min_max(tempBoard, thinkDepth, alpha, beta, true);
                        tempBoard.undo();

                        if (val <= minValue) {
                            minValue = val;
                            bestMove = mv;
                        }
                    }

                    bestMoves[i] = bestMove;
                    bestValues[i] = minValue;
                });
            }

            for (auto& task : tasks) {
                task.get();
            }

            float minVal = UPPER_BOUND;
            size_t minIndex;

            for (size_t i = 0; i < bestValues.size(); ++i) {
                if (bestValues[i] <= minVal) {
                    minVal = bestValues[i];
                    minIndex = i;
                }
            }

            return bestMoves[minIndex];
        }
        else {
            auto moves = MovesGen::gen_moves_for_one_side(board, Side::down);
            auto splitMoves = split_vector(moves, split_chunk_num);

            std::vector<Move> bestMoves;
            std::vector<float> bestValues;
            std::vector<std::future<void>> tasks;

            bestMoves.resize(splitMoves.size());
            bestValues.resize(splitMoves.size());
            tasks.resize(splitMoves.size());

            for (size_t i = 0; i < splitMoves.size(); ++i) {
                tasks[i] = std::async([&board, &bestMoves, &bestValues, &splitMoves, i, thinkDepth]() {
                    Board tempBoard = board;

                    float maxValue = LOWER_BOUND;
                    float alpha = LOWER_BOUND;
                    float beta = UPPER_BOUND;
                    Move bestMove;

                    for (const Move& mv : splitMoves[i]) {
                        tempBoard.move(mv);
                        float val = min_max(tempBoard, thinkDepth, alpha, beta, false);
                        tempBoard.undo();

                        if (val >= maxValue) {
                            maxValue = val;
                            bestMove = mv;
                        }
                    }

                    bestMoves[i] = bestMove;
                    bestValues[i] = maxValue;
                });
            }

            for (auto& task : tasks) {
                task.get();
            }

            float maxVal = LOWER_BOUND;
            size_t maxIndex;

            for (size_t i = 0; i < bestValues.size(); ++i) {
                if (bestValues[i] >= maxVal) {
                    maxVal = bestValues[i];
                    maxIndex = i;
                }
            }

            return bestMoves[maxIndex];
        }
    }
};

class ColorPrinter {
public:
    enum color {
        black,
        red,
        green,
        yellow,
        blue,
        magenta,
        cyan,
        white,
        bold_black,
        bold_red,
        bold_green,
        bold_yellow,
        bold_blue,
        bold_magenta,
        bold_cyan,
        bold_white,
        reset
    };

    ColorPrinter() {
        #ifdef _WIN32
        CONSOLE_SCREEN_BUFFER_INFO csbiInfo;
        hOutHandle = GetStdHandle(STD_OUTPUT_HANDLE);
        GetConsoleScreenBufferInfo(hOutHandle, &csbiInfo);
        oldColorAttrs = csbiInfo.wAttributes;
        #endif
    }

    ~ColorPrinter() {
        reset_color();
    }

    template<typename T>
    ColorPrinter& operator<<(T&& printable) {
        if constexpr (std::is_same_v<T, color>) {
            if (printable == color::reset) {
                reset_color();
            }
            else {
                set_color(printable);
            }
        }
        else {
            std::cout << printable;
        }

        return *this;
    }
private:
    void set_color(color c) {
        #ifdef _WIN32
        SetConsoleTextAttribute(hOutHandle, get_windows_color_attr(c));
        #else
        switch(c) {
            case color::black:
                std::cout << "\033[30m"; break;
            case color::red:
                std::cout << "\033[31m"; break;
            case color::green:
                std::cout << "\033[32m"; break;
            case color::yellow:       
                std::cout << "\033[33m"; break;
            case color::blue:         
                std::cout << "\033[34m"; break;
            case color::magenta:      
                std::cout << "\033[35m"; break;
            case color::cyan:         
                std::cout << "\033[36m"; break;
            case color::white:        
                std::cout << "\033[37m"; break;
            case color::bold_black:    
                std::cout << "\033[1m\033[30m"; break;
            case color::bold_red:      
                std::cout << "\033[1m\033[31m"; break;
            case color::bold_green:    
                std::cout << "\033[1m\033[32m"; break;
            case color::bold_yellow:   
                std::cout << "\033[1m\033[33m"; break;
            case color::bold_blue:     
                std::cout << "\033[1m\033[34m"; break;
            case color::bold_magenta:  
                std::cout << "\033[1m\033[35m"; break;
            case color::bold_cyan:     
                std::cout << "\033[1m\033[36m"; break;
            case color::bold_white:  
            default:  
                std::cout << "\033[1m\033[37m"; break;
        }
        #endif
    }

    void reset_color() {
        #ifdef _WIN32
        SetConsoleTextAttribute(hOutHandle, oldColorAttrs);
        #else
        std::cout << "\033[0m";
        #endif
    }

#ifdef _WIN32
    WORD get_windows_color_attr(color c) {
        switch(c) {
            case color::black: 
                return 0;
            case color::blue: 
                return FOREGROUND_BLUE;
            case color::green: 
                return FOREGROUND_GREEN;
            case color::cyan: 
                return FOREGROUND_GREEN | FOREGROUND_BLUE;
            case color::red: 
                return FOREGROUND_RED;
            case color::magenta: 
                return FOREGROUND_RED | FOREGROUND_BLUE;
            case color::yellow: 
                return FOREGROUND_RED | FOREGROUND_GREEN;
            case color::white:
                return FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
            case color::bold_black: 
                return 0 | FOREGROUND_INTENSITY;
            case color::bold_blue: 
                return FOREGROUND_BLUE | FOREGROUND_INTENSITY;
            case color::bold_green: 
                return FOREGROUND_GREEN | FOREGROUND_INTENSITY;
            case color::bold_cyan: 
                return FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
            case color::bold_red: 
                return FOREGROUND_RED | FOREGROUND_INTENSITY;
            case color::bold_magenta: 
                return FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
            case color::bold_yellow: 
                return FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
            case color::bold_white:
            default:
                return FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
        };
    }

    HANDLE hOutHandle;
    WORD oldColorAttrs;
#endif
};

class Game {
    Board board;
    int32_t thinkDepth;
    ColorPrinter cprinter;
    bool gameOver;
    Side userSide;
    Side elysiaSide;

    void clear_screen() {
        #ifdef _WIN32
        system("cls");
        #else
        system("clear");
        #endif
    }

    void show_board_on_console() {
        clear_screen();
        cprinter << "\n    x-----------------x\n";

        int32_t n = 8;
        for (int32_t r = Board::line_begin; r <= Board::line_end; ++r) {
            cprinter << ColorPrinter::bold_yellow << " " << n-- << ColorPrinter::reset;
            cprinter << "  | ";

            for (int32_t c = Board::line_begin; c <= Board::line_end; ++c) {
                Piece p = board.get(r, c);
                Side s = piece_side(p);

                if (s == Side::upper) {
                    cprinter << ColorPrinter::bold_blue << p << " " << ColorPrinter::reset;
                }
                else if (s == Side::down) {
                    cprinter << ColorPrinter::bold_red << p << " " << ColorPrinter::reset;
                }
                else {
                    cprinter << ColorPrinter::white << p << " " << ColorPrinter::reset;
                }
            }

            cprinter << "|\n";
        }

        cprinter << "    x-----------------x\n";
        cprinter << ColorPrinter::bold_green << "\n      a b c d e f g h\n\n" << ColorPrinter::reset;
    }

    void show_help_page(){
        clear_screen();

        cprinter << "\n=======================================\n";
        cprinter << "Help Page\n\n";
        cprinter << "    1. help         - this page.\n";
        cprinter << "    2. b2e2         - input like this will be parsed as a move.\n";
        cprinter << "    3. undo         - undo the previous move.\n";
        cprinter << "    4. exit or quit - exit the game.\n";
        cprinter << "    5. remake       - remake the game.\n";
        cprinter << "    6. prompt       - give me a best move.\n\n";
        cprinter << "  The characters on the board have the following relationships: \n\n";
        cprinter << "    P -> Elysia side pawn.\n";
        cprinter << "    R -> Elysia side rook.\n";
        cprinter << "    N -> Elysia side knight.\n";
        cprinter << "    B -> Elysia side bishop.\n";
        cprinter << "    Q -> Elysia side queen.\n";
        cprinter << "    K -> Elysia side king.\n";
        cprinter << "    p -> our pawn.\n";
        cprinter << "    r -> our rook.\n";
        cprinter << "    n -> our knight.\n";
        cprinter << "    b -> our bishop.\n";
        cprinter << "    q -> our queen.\n";
        cprinter << "    k -> our king.\n";
        cprinter << "    . -> no piece here.\n";
        cprinter << "=======================================\n";
        cprinter << "Press any key to continue.\n";

        std::string ignore_input;    
        std::getline(std::cin, ignore_input);
    }

    std::string desc_move(const Move& mv) {
        std::string desc;

        desc += static_cast<char>(mv.from.col - Board::line_begin + 'a');
        desc += static_cast<char>(8 - (mv.from.row - Board::line_begin) + '0');
        desc += static_cast<char>(mv.to.col - Board::line_begin + 'a');
        desc += static_cast<char>(8 - (mv.to.row - Board::line_begin) + '0');
        return desc;
    }

    bool is_input_a_move(const std::string& input) {
        if (input.size() < 4){
            return false;
        }

        return  (input[0] >= 'a' && input[0] <= 'h') &&
                (input[1] >= '1' && input[1] <= '8') &&
                (input[2] >= 'a' && input[2] <= 'h') &&
                (input[3] >= '1' && input[3] <= '8');
    }

    bool is_win(Side s) {
        bool upper_king_alive = false;
        bool down_king_alive = false;

        for (int32_t r = Board::line_begin; r <= Board::line_end; ++r) {
            for (int32_t c = Board::line_begin; c <= Board::line_end; ++c){
                Piece p = board.get(r, c);

                if (p == P_UK){
                    upper_king_alive = true;
                }
                else if (p == P_DK){
                    down_king_alive = true;
                }
            }
        }

        if (upper_king_alive && down_king_alive) {
            return false;
        }
        else {
            return s == Side::upper ? upper_king_alive : down_king_alive;
        }
    }

    Piece ask_for_promotion() {
        std::string input;

        while (true) {
            cprinter << "please choose your promotion: rook, knight, bishop, queen\n";
            std::getline(std::cin, input);

            if (input == "rook") {
                return userSide == Side::upper ? P_UR : P_DR;
            }
            else if (input == "knight") {
                return userSide == Side::upper ? P_UN : P_DN;
            }
            else if (input == "bishop") {
                return userSide == Side::upper ? P_UB : P_DB;
            }
            else if (input == "queen") {
                return userSide == Side::upper ? P_UQ : P_DQ;
            }
            else {
                cprinter << "invalid, please re-enter\n\n";
            }
        }
    }

    Move input_to_move(const std::string& input) {
        Move mv;

        mv.from.row = Board::line_begin + '8' - input[1];
        mv.from.col = Board::line_begin + input[0] - 'a';
        mv.to.row   = Board::line_begin + '8' - input[3];
        mv.to.col   = Board::line_begin + input[2] - 'a';

        auto moves = MovesGen::gen_moves_for_one_side(board, userSide);
        auto iter = std::find(moves.cbegin(), moves.cend(), mv);

        if (iter == moves.cend()) {
            mv.moveType = MoveType::invalid;
        }
        else {
            mv.moveType = iter->moveType;

            if (mv.moveType == MoveType::pawn_move_and_promote) {
                if (piece_type(board.get(mv.to)) != Type::king) {
                    mv.promoteP = ask_for_promotion();
                }
            }
        }

        return mv;
    }

    void handle_prompt() {
        auto start_time = std::chrono::system_clock::now();
        Move prompt = BestMoveGenParallel::gen_best_for(board, userSide, thinkDepth);
        auto end_time = std::chrono::system_clock::now();

        cprinter << "You can try: " << desc_move(prompt);
        cprinter << ", piece is '" << board.get(prompt.from) << "'";
        cprinter << ", time cost " << std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time).count() << " seconds\n\n";
    }

    void handle_move(const std::string& input) {
        if (!is_input_a_move(input)) {
            cprinter << "unknown command, do nothing\n\n";
            return;
        }

        Move mv = input_to_move(input);
        if (mv.moveType == MoveType::invalid) {
            cprinter << "invalid move\n\n";
            return;
        }

        Piece p = board.get(mv.from);
        if (piece_side(p) != userSide) {
            cprinter << "this is not your piece, cannot move\n\n";
            return;
        }

        board.move(mv);
        show_board_on_console();

        if (is_win(userSide)) {
            gameOver = true;
            cprinter << ColorPrinter::bold_yellow << "Congratulations! You win!\n\n" << ColorPrinter::reset;
            return;
        }

        cprinter << ColorPrinter::bold_magenta << "Elysia" << ColorPrinter::reset << " thinking...\n";

        auto start_time = std::chrono::system_clock::now();
        Move elysiaMove = BestMoveGenParallel::gen_best_for(board, elysiaSide, thinkDepth);
        auto end_time = std::chrono::system_clock::now();
        p = board.get(elysiaMove.from);
        
        board.move(elysiaMove);
        show_board_on_console();

        cprinter << ColorPrinter::bold_magenta << "Elysia" << ColorPrinter::reset << " thought " << std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time).count() << " seconds, ";
        cprinter << "moves: " << desc_move(elysiaMove);
        cprinter << ", piece is '" << p << "'\n\n";

        if (is_win(elysiaSide)) {
            gameOver = true;
            cprinter << ColorPrinter::bold_red << "Sorry, Elysia wins!\n\n" << ColorPrinter::reset;
            return;
        }
    }
public:
    Game() : thinkDepth{ 5 }, gameOver{ false }, userSide{ Side::down }, elysiaSide{ Side::upper } {}

    void run() {
        std::string input;
        show_board_on_console();

        if (userSide == Side::upper) {
            cprinter << "the upper side is you. any question can be found in 'help'.\n\n";
        }
        else {
            cprinter << "the down side is you. any question can be found in 'help'.\n\n";
        }

        while (true) {
            if (gameOver) {
                return;
            }

            cprinter << ColorPrinter::bold_yellow << "Your turn: " << ColorPrinter::reset;
            std::getline(std::cin, input);

            if (input == "help") {
                show_help_page();
                show_board_on_console();
            }
            else if (input == "undo") {
                board.undo();
                board.undo();
                show_board_on_console();
            }
            else if (input == "quit") {
                cprinter << "Bye.\n\n";
                return;
            }
            else if (input == "exit") {
                cprinter << "Bye.\n\n";
                return;
            }
            else if (input == "remake") {
                board.reset();
                show_board_on_console();
            }
            else if (input == "prompt") {
                handle_prompt();
            }
            else {
                handle_move(input);
            }
        }
    }
};

int main() {
    ScoreEvaluator::init_values();

    Game game;
    game.run();
    return 0;
}
