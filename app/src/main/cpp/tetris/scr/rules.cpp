#include "headers/rules.hpp"
#include <SDL3/SDL.h>
#include <random>
#include <algorithm>

const KickTable jlstz_kicks = {
    {{0, 1}, {{0, 0}, {-1, 0}, {-1,  1}, {0, -2}, {-1, -2}}},
    {{1, 0}, {{0, 0}, { 1, 0}, { 1, -1}, {0,  2}, { 1,  2}}},
    {{1, 2}, {{0, 0}, { 1, 0}, { 1, -1}, {0,  2}, { 1,  2}}},
    {{2, 1}, {{0, 0}, {-1, 0}, {-1,  1}, {0, -2}, {-1, -2}}},
    {{2, 3}, {{0, 0}, { 1, 0}, { 1,  1}, {0, -2}, { 1, -2}}},
    {{3, 2}, {{0, 0}, {-1, 0}, {-1, -1}, {0,  2}, {-1,  2}}},
    {{3, 0}, {{0, 0}, {-1, 0}, {-1, -1}, {0,  2}, {-1,  2}}},
    {{0, 3}, {{0, 0}, { 1, 0}, { 1,  1}, {0, -2}, { 1, -2}}}
};

const KickTable i_kicks = {
    {{0, 1}, {{0, 0}, {-2, 0}, { 1,  0}, {-2, -1}, { 1,  2}}},
    {{1, 0}, {{0, 0}, { 2, 0}, {-1,  0}, { 2,  1}, {-1, -2}}},
    {{1, 2}, {{0, 0}, {-1, 0}, { 2,  0}, {-1,  2}, { 2, -1}}},
    {{2, 1}, {{0, 0}, { 1, 0}, {-2,  0}, { 1, -2}, {-2,  1}}},
    {{2, 3}, {{0, 0}, { 2, 0}, {-1,  0}, { 2,  1}, {-1, -2}}},
    {{3, 2}, {{0, 0}, {-2, 0}, { 1,  0}, {-2, -1}, { 1,  2}}},
    {{3, 0}, {{0, 0}, { 1, 0}, {-2,  0}, { 1, -2}, {-2,  1}}},
    {{0, 3}, {{0, 0}, {-1, 0}, { 2,  0}, {-1,  2}, { 2, -1}}}
};

const KickList wall_kicks = {
    {'J', jlstz_kicks}, {'L', jlstz_kicks}, {'S', jlstz_kicks},
    {'T', jlstz_kicks}, {'Z', jlstz_kicks}, {'I', i_kicks}, {'O', {}}
};

char rand_choice(const std::vector<char>& vec) {
    if (vec.empty()) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "rand_choice called with empty vector! Returning default 'y'");
        return 'y';
    }
    static std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<size_t> dist(0, vec.size() - 1);
    return vec[dist(gen)];
}

bool collide(const Tetro& block, int off_x, int off_y, int play_grid_x, int play_grid_y, const std::vector<std::vector<char>>& board) {
    const auto& shape = block.get_shape();
    for (size_t row = 0; row < shape.size(); row++) {
        for (size_t col = 0; col < shape[row].size(); col++) {
            if (shape[row][col] != 0) {
                int new_x = block.x + static_cast<int>(col) + off_x;
                int new_y = block.y + static_cast<int>(row) + off_y;

                if (new_x < 0 || new_x >= play_grid_x) return true;
                if (new_y >= play_grid_y) return true; 
                if (new_y >= 0 && static_cast<size_t>(new_y) < board.size() &&
                    static_cast<size_t>(new_x) < board[new_y].size() &&
                    board[new_y][new_x] != 0) return true;
            }
        }
    }
    return false;
}

int get_ghost_y(const Tetro& block, int play_grid_x, int play_grid_y, const std::vector<std::vector<char>>& board) {
    int ghost_off_y = 0;
    while (!collide(block, 0, ghost_off_y + 1, play_grid_x, play_grid_y, board)) {
        ghost_off_y++;
    }
    return block.y + ghost_off_y;
}

std::vector<int> find_full_lines(const std::vector<std::vector<char>>& board, int play_x, int play_y) {
    std::vector<int> full_rows;
    if (board.empty()) return full_rows;

    for (int r = play_y - 1; r >= 0; --r) {
        if (static_cast<size_t>(r) >= board.size()) continue;
        bool full = true;
        for (int c = 0; c < play_x; ++c) {
            if (static_cast<size_t>(c) >= board[r].size() || board[r][c] == 0) {
                full = false;
                break;
            }
        }
        if (full) {
            full_rows.push_back(r);
        }
    }
    return full_rows;
}

void remove_lines(std::vector<std::vector<char>>& board, const std::vector<int>& rows, int play_x, int play_y) {
    if (board.empty() || rows.empty()) return;

    int insert_row = play_y - 1;

    for (int r = play_y - 1; r >= 0; --r) {
        if (static_cast<size_t>(r) >= board.size()) continue;

        bool is_cleared = (std::find(rows.begin(), rows.end(), r) != rows.end());
        if (!is_cleared) {
            if (insert_row != r && static_cast<size_t>(insert_row) < board.size()) {
                board[insert_row] = board[r];
            }
            insert_row--;
        }
    }

    while (insert_row >= 0) {
        if (static_cast<size_t>(insert_row) < board.size()) {
            std::fill(board[insert_row].begin(), board[insert_row].end(), 0);
        }
        insert_row--;
    }
}
