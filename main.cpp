// Author: Kasianchuk Anna K-25
// Variant 14
// Compiler: Bundled MinGW 14.0 w64 (GCC 14), CLion toolchain
// Language standard: C++20

#include <iostream>
#include <vector>
#include <coroutine>
#include <random>
#include <exception>
#include <optional>
#include <string>
#include <sstream>
#include <iomanip>
#include <stdexcept>

constexpr int MIN_SIZE = 3;
constexpr int MAX_SIZE = 30;

constexpr char EMPTY = ' ';
constexpr char SYMBOL_X = 'X';
constexpr char SYMBOL_O = 'O';

struct Move {
    int row;
    int col;
};

enum class Outcome {
    XWins,
    OWins,
    Draw
};

class Board {
private:
    int n_;
    std::vector<std::vector<char>> grid_;

    bool in_bounds(int r, int c) const {
        return r >= 0 && r < n_ && c >= 0 && c < n_;
    }

    void print_header() const {
        std::cout << "   ";
        for (int j = 0; j < n_; ++j) {
            std::cout << std::setw(2) << (j + 1) << "  ";
        }
        std::cout << "\n";
    }

    void print_separator() const {
        std::cout << "   ";
        for (int j = 0; j < n_; ++j) {
            std::cout << "---";
            if (j < n_ - 1) std::cout << "+";
        }
        std::cout << "\n";
    }

public:
    explicit Board(int size) : n_(size), grid_(size, std::vector<char>(size, EMPTY)) {}

    int size() const { return n_; }

    bool is_empty(int r, int c) const {
        return in_bounds(r, c) && grid_[r][c] == EMPTY;
    }

    void place(int r, int c, char symbol) {
        if (!in_bounds(r, c)) {
            throw std::out_of_range("Coordinates out of board bounds.");
        }
        if (grid_[r][c] != EMPTY) {
            throw std::invalid_argument("Cell is already occupied.");
        }
        grid_[r][c] = symbol;
    }

    void display() const {
        print_header();
        for (int i = 0; i < n_; ++i) {
            std::cout << std::setw(2) << (i + 1) << " ";
            for (int j = 0; j < n_; ++j) {
                std::cout << " " << grid_[i][j] << " ";
                if (j < n_ - 1) std::cout << "|";
            }
            std::cout << "\n";

            if (i < n_ - 1) {
                print_separator();
            }
        }
        std::cout << "\n";
    }

    bool check_win(char player) const {
        for (int i = 0; i < n_; ++i) {
            bool row_win = true, col_win = true;
            for (int j = 0; j < n_; ++j) {
                if (grid_[i][j] != player) row_win = false;
                if (grid_[j][i] != player) col_win = false;
            }
            if (row_win || col_win) return true;
        }

        bool diag1 = true, diag2 = true;
        for (int i = 0; i < n_; ++i) {
            if (grid_[i][i] != player) diag1 = false;
            if (grid_[i][n_ - 1 - i] != player) diag2 = false;
        }
        return diag1 || diag2;
    }

    bool is_full() const {
        for (int i = 0; i < n_; ++i) {
            for (int j = 0; j < n_; ++j) {
                if (grid_[i][j] == EMPTY) return false;
            }
        }
        return true;
    }
};

struct PlayerCoroutine {
    struct promise_type {
        Move current_move{0, 0};
        std::exception_ptr exception_ = nullptr;

        PlayerCoroutine get_return_object() {
            return PlayerCoroutine{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        std::suspend_always initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        std::suspend_always yield_value(Move m) {
            current_move = m;
            return {};
        }
        void return_void() {}

        void unhandled_exception() {
            exception_ = std::current_exception();
        }
    };

    std::coroutine_handle<promise_type> handle;

    explicit PlayerCoroutine(std::coroutine_handle<promise_type> h) : handle(h) {}
    ~PlayerCoroutine() { if (handle) handle.destroy(); }

    PlayerCoroutine(const PlayerCoroutine&) = delete;
    PlayerCoroutine& operator=(const PlayerCoroutine&) = delete;

    PlayerCoroutine(PlayerCoroutine&& other) noexcept : handle(other.handle) {
        other.handle = nullptr;
    }
    PlayerCoroutine& operator=(PlayerCoroutine&& other) noexcept {
        if (this != &other) {
            if (handle) handle.destroy();
            handle = other.handle;
            other.handle = nullptr;
        }
        return *this;
    }

    bool next() {
        if (!handle || handle.done()) return false;
        handle.resume();

        if (handle.done() && handle.promise().exception_) {
            std::rethrow_exception(handle.promise().exception_);
        }
        return !handle.done();
    }

    Move get_move() const { return handle.promise().current_move; }
};

PlayerCoroutine bot_player(const Board& board, int seed_offset) {
    std::mt19937 rng(std::random_device{}() + seed_offset);

    while (true) {
        std::vector<Move> empty_cells;
        for (int i = 0; i < board.size(); ++i) {
            for (int j = 0; j < board.size(); ++j) {
                if (board.is_empty(i, j)) {
                    empty_cells.push_back({i, j});
                }
            }
        }

        if (empty_cells.empty()) co_return;

        std::uniform_int_distribution<int> dist(0, static_cast<int>(empty_cells.size() - 1));
        co_yield empty_cells[dist(rng)];
    }
}

void announce_move(char symbol, Move m) {
    std::cout << "Player " << symbol << " makes a move: ("
              << (m.row + 1) << ", " << (m.col + 1) << ")\n";
}

std::optional<Outcome> evaluate(const Board& board, char current_symbol, bool is_x_turn) {
    if (board.check_win(current_symbol)) {
        return is_x_turn ? Outcome::XWins : Outcome::OWins;
    }
    if (board.is_full()) {
        return Outcome::Draw;
    }
    return std::nullopt;
}

Outcome play_game(int n) {
    std::cout << "\n--- Tic-Tac-Toe Game Started (" << n << "x" << n << ") ---\n\n";
    Board board(n);

    PlayerCoroutine player_x = bot_player(board, 1);
    PlayerCoroutine player_o = bot_player(board, 2);

    board.display();
    bool is_x_turn = true;

    while (true) {
        char current_symbol = is_x_turn ? SYMBOL_X : SYMBOL_O;
        PlayerCoroutine& current_player = is_x_turn ? player_x : player_o;

        if (!current_player.next()) {
            throw std::runtime_error("Coroutine stopped unexpectedly.");
        }

        Move m = current_player.get_move();
        announce_move(current_symbol, m);

        board.place(m.row, m.col, current_symbol);
        board.display();

        if (auto result = evaluate(board, current_symbol, is_x_turn)) {
            return *result;
        }

        is_x_turn = !is_x_turn;
    }
}

void print_result(Outcome outcome) {
    switch (outcome) {
        case Outcome::XWins:
            std::cout << "*** Winner: Player X! ***\n\n";
            break;
        case Outcome::OWins:
            std::cout << "*** Winner: Player O! ***\n\n";
            break;
        case Outcome::Draw:
            std::cout << "*** The game ended in a draw! ***\n\n";
            break;
    }
}


std::optional<int> read_board_size() {
    std::string line;
    while (true) {
        std::cout << "Enter board size n (integer from " << MIN_SIZE
                  << " to " << MAX_SIZE << "): ";
        if (!std::getline(std::cin, line)) return std::nullopt;

        std::istringstream iss(line);
        int n = 0;
        char extra = 0;
        if (iss >> n && !(iss >> extra) && n >= MIN_SIZE && n <= MAX_SIZE) {
            return n;
        }
        std::cerr << "Invalid input. Please try again.\n";
    }
}

int main() {
    try {
        const std::optional<int> n = read_board_size();
        if (!n) {
            std::cerr << "\nInput interrupted (EOF). Exiting...\n";
            return 1;
        }

        Outcome result = play_game(*n);
        print_result(result);
    }
    catch (const std::exception& e) {
        std::cerr << "\nFatal error occurred: " << e.what() << "\n";
        return 1;
    }

    return 0;
}