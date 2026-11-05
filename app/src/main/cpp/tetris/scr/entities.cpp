#include "headers/entities.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <iterator>
#include <random>

std::mt19937 gen(std::random_device{}());

const std::map<char, Matrix> tetro = {
    {'I', {{0, 0, 0, 0}, {1, 1, 1, 1}, {0, 0, 0, 0}, {0, 0, 0, 0}}},
    {'J', {{1, 0, 0}, {1, 1, 1}, {0, 0, 0}}},
    {'L', {{0, 0, 1}, {1, 1, 1}, {0, 0, 0}}},
    {'O', {{1, 1}, {1, 1}}},
    {'S', {{0, 1, 1}, {1, 1, 0}, {0, 0, 0}}},
    {'T', {{0, 1, 0}, {1, 1, 1}, {0, 0, 0}}},
    {'Z', {{1, 1, 0}, {0, 1, 1}, {0, 0, 0}}}
};

const std::map<char, std::string> tiles_path = {
    {'b', "assets/tiles/blue.png"},
    {'d', "assets/tiles/dark_b.png"},
    {'g', "assets/tiles/green.png"},
    {'o', "assets/tiles/orange.png"},
    {'p', "assets/tiles/purple.png"},
    {'r', "assets/tiles/red.png"},
    {'y', "assets/tiles/yellow.png"}
};

const std::vector<char> colors = {
    'b',
    'd',
    'g',
    'o',
    'p',
    'r',
    'y'
};

char get_piece_color(char type) {
    switch (type) {
        case 'I': return 'b';
        case 'J': return 'd';
        case 'L': return 'o';
        case 'O': return 'y';
        case 'S': return 'g';
        case 'T': return 'p';
        case 'Z': return 'r';
        default:
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Unknown piece type '%c', defaulting to yellow.", type);
            return 'y';
    }
}

void Tetro::setup_piece(char piece_type, int play_colmn) {
    auto it = tetro.find(piece_type);
    if (it != tetro.end()) {
        shape = it->second;
        type = piece_type;
    } else {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Invalid piece type '%c' in setup_piece, fallback to 'T'", piece_type);
        shape = tetro.at('T');
        type = 'T';
    }
    int shape_w = shape.empty() ? 4 : static_cast<int>(shape[0].size());
    x = (play_colmn - shape_w) / 2;
    y = 0;
    rotation = 0;
}

void Tetro::setup(int play_colmn){
    if (tetro.empty()) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "tetro map is empty in setup()!");
        return;
    }

    std::uniform_int_distribution<> distr(0, static_cast<int>(tetro.size()) - 1);

    int index = distr(gen);
    auto it = tetro.begin();
    
    std::advance(it, index); 

    setup_piece(it->first, play_colmn);
}

void Tetro::rotate(int steps) {
    int k = (steps % 4 + 4) % 4; 
    
    if (k == 0 || shape.empty()) return;

    rotation = (rotation + k) % 4;

    int n = static_cast<int>(shape.size());

    while (k--) {
        for (int i = 0; i < n; ++i) {
            for (int j = i + 1; j < n; ++j) {
                std::swap(shape[i][j], shape[j][i]);
            }
        }

        for (int i = 0; i < n; ++i) {
            std::reverse(shape[i].begin(), shape[i].end());
        }
    }
}

Matrix Tetro::get_shape() const{
    return shape;
}

int Tetro::get_rotation(){
    return rotation % 4;
}
