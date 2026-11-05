#pragma once

#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <memory>
#include <string>
#include <vector>
#include <map>

#include "entities.hpp"
#include "ui.hpp"
#include "touch_controller.hpp"

enum class GameState {
    START_SCREEN,
    PLAYING,
    PAUSED,
    GAME_OVER
};

struct PieceInfo {
    char type = 'I';
    char color = 'b';
};

struct ClearAnimation {
    std::vector<int> rows;
    Uint64 start_time = 0;
    const Uint64 duration = 220;
    bool active = false;
};

class Game {
public:
    Game();
    ~Game();

    bool init();
    void run();

private:
    void update_layout(int width, int height);
    void handle_inputs(const SDL_Event& event);
    void handle_touch_down(SDL_FingerID finger, float x, float y);
    void handle_touch_up(SDL_FingerID finger, float x, float y);
    void handle_touch_motion(SDL_FingerID finger, float x, float y);

    void update_movement(Uint64 cur_time, const bool* key_state);
    void update_fall(Uint64 cur_time, int current_fall_delay);
    void update_music();
    void play_next_track();

    void refill_bag();
    PieceInfo pop_next_piece();
    void spawn_new_piece();
    void rotate_current_piece(int dir);
    bool move_current_piece(int dx);
    void hard_drop();
    void trigger_hold();
    void place_piece();
    void handle_line_clears();
    void reset_game();

    void load_high_score();
    void save_high_score();

    void render();
    void draw_grid();
    void draw_matrix(const Tetro& block, int tetro_x, int tetro_y, int ghost_y, char color_key);
    void draw_preview_piece(const PieceInfo& piece, float slot_x, float slot_y, float slot_w, float slot_h, float cell_sz, float alpha = 1.0f);
    void draw_hud();
    void draw_touch_controls();
    void draw_start_screen();
    void draw_pause_menu();
    void draw_game_over_screen();

    std::vector<std::string> load_music();
    std::map<char, SDL_Texture*> load_textures();

    int screen_w = 1080;
    int screen_h = 1920;
    bool is_portrait = true;

    int g_size = 32;
    int play_grid_x = 10;
    int play_grid_y = 20;
    float start_x = 0.0f;
    float start_y = 0.0f;

    SDL_FRect grid_rect{0, 0, 0, 0};
    SDL_FRect top_bar_rect{0, 0, 0, 0};
    SDL_FRect hold_box_rect{0, 0, 0, 0};
    SDL_FRect next_box_rect{0, 0, 0, 0};
    SDL_FRect info_box_rect{0, 0, 0, 0};

    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    MIX_Mixer* mixer = nullptr;
    MIX_Track* music_track = nullptr;
    MIX_Audio* music_audio = nullptr;

    MIX_Audio* sfx_land = nullptr;
    MIX_Audio* sfx_clear = nullptr;
    MIX_Audio* sfx_level_up = nullptr;
    MIX_Audio* sfx_click = nullptr;

    std::vector<std::string> playlist;
    int cur_track_index = 0;

    TTF_Font* font_large = nullptr;
    TTF_Font* font_medium = nullptr;
    TTF_Font* font_small = nullptr;

    std::unique_ptr<TextCache> text_title;
    std::unique_ptr<TextCache> text_score;
    std::unique_ptr<TextCache> text_high_score;
    std::unique_ptr<TextCache> text_level;
    std::unique_ptr<TextCache> text_lines;
    std::unique_ptr<TextCache> text_banner;

    std::map<char, SDL_Texture*> textures;

    GameState state = GameState::START_SCREEN;
    bool running = true;
    float volume = 0.8f;
    float norm_vol = 0.8f;

    // Pieces and Queue
    Tetro cur_tile;
    char cur_color = 'y';
    std::vector<PieceInfo> next_queue;
    PieceInfo hold_piece{' ', ' '};
    bool has_hold = false;
    bool can_hold = true;

    std::vector<std::vector<char>> board;
    ClearAnimation clear_anim;

    // Timing & lock delay
    int fall_delay = 500;
    int base_fall_delay = 500;
    int DAS = 130;
    int ARR = 20;
    int SDF = 20;

    int level = 1;
    int score = 0;
    int lines_cleared_total = 0;
    int high_score = 0;
    bool is_new_high_score = false;

    Uint64 last_fall_time = 0;
    bool is_grounded = false;
    Uint64 ground_start_time = 0;
    const Uint64 lock_delay_ms = 450;
    int lock_resets = 0;
    const int max_lock_resets = 15;

    bool is_soft_dropping = false;

    // Input states
    bool touch_left_pressed = false;
    bool touch_right_pressed = false;
    bool touch_down_pressed = false;

    enum Dir { LEFT = 0, RIGHT = 1 };
    std::vector<bool> active_dir{false, false};
    std::vector<bool> das_triggered{false, false};
    std::vector<Uint64> last_move_time{0, 0};

    // Touch button lists
    std::vector<TouchButton> game_buttons;
    std::vector<TouchButton> pause_buttons;
    std::vector<TouchButton> start_buttons;
    std::vector<TouchButton> game_over_buttons;

    // Gesture touch controls
    SDL_FRect gesture_zone{0.0f, 0.0f, 0.0f, 0.0f};
    TouchController touch_controller;
};
