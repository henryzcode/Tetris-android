#include "headers/game.hpp"
#include "headers/parse.hpp"
#include "headers/rules.hpp"
#include <SDL3_image/SDL_image.h>
#include <algorithm>
#include <random>

const int FPS = 60;
const float GHOST_ALPHA = 70.0f / 255.0f;
const SDL_Color FONT_COLOR = {255, 255, 255, 255};

Game::Game() = default;

Game::~Game()
{
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Cleaning up Game resources...");

    for (auto &[_, tex] : textures)
    {
        if (tex)
            SDL_DestroyTexture(tex);
    }
    textures.clear();

    if (music_track)
    {
        MIX_DestroyTrack(music_track);
        music_track = nullptr;
    }
    if (music_audio)
    {
        MIX_DestroyAudio(music_audio);
        music_audio = nullptr;
    }
    if (sfx_land)
    {
        MIX_DestroyAudio(sfx_land);
        sfx_land = nullptr;
    }
    if (sfx_clear)
    {
        MIX_DestroyAudio(sfx_clear);
        sfx_clear = nullptr;
    }
    if (sfx_level_up)
    {
        MIX_DestroyAudio(sfx_level_up);
        sfx_level_up = nullptr;
    }
    if (sfx_click)
    {
        MIX_DestroyAudio(sfx_click);
        sfx_click = nullptr;
    }
    if (mixer)
    {
        MIX_DestroyMixer(mixer);
        mixer = nullptr;
    }

    text_title.reset();
    text_score.reset();
    text_high_score.reset();
    text_level.reset();
    text_lines.reset();
    text_banner.reset();

    if (font_large)
    {
        TTF_CloseFont(font_large);
        font_large = nullptr;
    }
    if (font_medium)
    {
        TTF_CloseFont(font_medium);
        font_medium = nullptr;
    }
    if (font_small)
    {
        TTF_CloseFont(font_small);
        font_small = nullptr;
    }

    if (renderer)
    {
        SDL_DestroyRenderer(renderer);
        renderer = nullptr;
    }
    if (window)
    {
        SDL_DestroyWindow(window);
        window = nullptr;
    }

    MIX_Quit();
    TTF_Quit();
    SDL_Quit();
}

bool Game::init()
{
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Initializing SDL Subsystems...");

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS))
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_Init failed: %s", SDL_GetError());
        return false;
    }
    if (!TTF_Init())
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "TTF_Init failed: %s", SDL_GetError());
        return false;
    }
    if (!MIX_Init())
    {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "MIX_Init failed: %s (Continuing without audio)", SDL_GetError());
    }

    mixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
    if (mixer)
    {
        MIX_SetMixerGain(mixer, volume);

        auto load_sfx = [this](const char* path) -> MIX_Audio* {
            MIX_Audio* a = MIX_LoadAudio(mixer, path, true);
            if (!a) {
                std::string fallback = std::string("assets/") + path;
                a = MIX_LoadAudio(mixer, fallback.c_str(), true);
            }
            if (!a) {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Failed to load audio '%s': %s", path, SDL_GetError());
            }
            return a;
        };

        sfx_land = load_sfx("landed.wav");
        sfx_clear = load_sfx("sound/click.wav");
        sfx_level_up = load_sfx("level_up.wav");
        sfx_click = load_sfx("sound/0.wav");
    }
    else
    {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Failed to create mixer device: %s", SDL_GetError());
    }

    playlist = load_music();
    if (mixer && !playlist.empty())
    {
        music_track = MIX_CreateTrack(mixer);
        if (music_track)
        {
            cur_track_index = 0;
            play_next_track();
        }
        else
        {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Failed to create music track: %s", SDL_GetError());
        }
    }

    const SDL_DisplayMode *dm = SDL_GetDesktopDisplayMode(SDL_GetPrimaryDisplay());
    screen_w = dm ? dm->w : 1080;
    screen_h = dm ? dm->h : 1920;

    window = SDL_CreateWindow("Tetris", screen_w, screen_h, SDL_WINDOW_FULLSCREEN | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!window)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateWindow failed: %s", SDL_GetError());
        return false;
    }
    SDL_SetWindowFullscreen(window, true);

    SDL_GetWindowSize(window, &screen_w, &screen_h);
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Window created with size %dx%d", screen_w, screen_h);

    renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateRenderer failed: %s", SDL_GetError());
        return false;
    }
    SDL_SetRenderVSync(renderer, 1);
    SDL_GetRenderOutputSize(renderer, &screen_w, &screen_h);

    parser::Config game_data = parser::loadConfig("0.json");

    auto open_font = [](const char* path, float pt) -> TTF_Font* {
        TTF_Font* f = TTF_OpenFont(path, pt);
        if (!f) {
            std::string fallback = std::string("assets/") + path;
            f = TTF_OpenFont(fallback.c_str(), pt);
        }
        if (!f) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Failed to open font '%s': %s", path, SDL_GetError());
        }
        return f;
    };

    font_large = open_font("pixel.ttf", 46.0f);
    font_medium = open_font("pixel.ttf", 26.0f);
    font_small = open_font("pixel.ttf", 26.0f);

    TTF_Font* f_main = font_large ? font_large : (font_medium ? font_medium : font_small);
    TTF_Font* f_med = font_medium ? font_medium : f_main;
    TTF_Font* f_sml = font_small ? font_small : f_med;

    if (f_main)
    {
        text_title = std::make_unique<TextCache>(f_main, FONT_COLOR);
        text_score = std::make_unique<TextCache>(f_med, FONT_COLOR);
        text_high_score = std::make_unique<TextCache>(f_sml, SDL_Color{255, 215, 60, 255});
        text_level = std::make_unique<TextCache>(f_med, FONT_COLOR);
        text_lines = std::make_unique<TextCache>(f_med, FONT_COLOR);
        text_banner = std::make_unique<TextCache>(f_main, SDL_Color{255, 220, 60, 255});
    }
    else
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not load any font! Text will not render.");
    }

    play_grid_x = game_data.cols ? game_data.cols : 10;
    play_grid_y = game_data.rows ? game_data.rows : 20;

    DAS = game_data.das ? game_data.das : 130;
    ARR = game_data.arr ? game_data.arr : 20;
    SDF = game_data.sdf ? game_data.sdf : 20;
    base_fall_delay = game_data.gravity_speed_ms ? game_data.gravity_speed_ms : 500;
    fall_delay = base_fall_delay;

    load_high_score();
    textures = load_textures();

    board.assign(play_grid_y, std::vector<char>(play_grid_x, 0));
    refill_bag();
    refill_bag();
    hold_piece = {' ', ' '};
    has_hold = false;
    can_hold = true;
    cur_tile.setup_piece('T', play_grid_x);
    cur_color = 'p';

    update_layout(screen_w, screen_h);
    state = GameState::START_SCREEN;

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Game init completed successfully.");
    return true;
}

void Game::update_layout(int width, int height)
{
    screen_w = width;
    screen_h = height;
    is_portrait = (screen_h > screen_w);

    // Intelligently compute and update global UI scaling based on screen size
    update_ui_scaling(width, height);

    game_buttons.clear();
    pause_buttons.clear();
    start_buttons.clear();
    game_over_buttons.clear();

    if (is_portrait)
    {
        // --- PORTRAIT LAYOUT ---
        // 1. Top Bar HUD (Horizontal)
        float top_margin = std::max(48.0f * ui_scaling, static_cast<float>(screen_h) * 0.045f);
        float top_bar_h = std::clamp(static_cast<float>(screen_h) * 0.088f, 95.0f * ui_scaling, 140.0f * ui_scaling);
        top_bar_rect = {12.0f * ui_scaling, top_margin, static_cast<float>(screen_w) - 24.0f * ui_scaling, top_bar_h};

        // Pause button inside/at the right edge of top bar
        float p_btn_s = std::clamp(top_bar_h * 0.55f, 40.0f * ui_scaling, 54.0f * ui_scaling);
        float p_btn_x = top_bar_rect.x + top_bar_rect.w - p_btn_s - 8.0f * ui_scaling;
        float p_btn_y = top_bar_rect.y + (top_bar_h - p_btn_s) * 0.5f;
        game_buttons.emplace_back(ButtonId::PAUSE, p_btn_x, p_btn_y, p_btn_s, p_btn_s, "||", "", 12.0f, 10.0f);

        // Top bar internal sections: HOLD on left, STATS on right
        float inner_w = p_btn_x - top_bar_rect.x - 12.0f * ui_scaling;
        float hold_w = std::clamp(inner_w * 0.28f, 85.0f * ui_scaling, 130.0f * ui_scaling);

        hold_box_rect = {top_bar_rect.x + 6.0f * ui_scaling, top_bar_rect.y + 6.0f * ui_scaling, hold_w, top_bar_h - 12.0f * ui_scaling};

        float stats_x = hold_box_rect.x + hold_box_rect.w + 8.0f * ui_scaling;
        float stats_w = p_btn_x - stats_x - 8.0f * ui_scaling;
        info_box_rect = {stats_x, top_bar_rect.y + 6.0f * ui_scaling, stats_w, top_bar_h - 12.0f * ui_scaling};

        // 2. Full-Screen Playfield Area (Bottom buttons removed for pure gesture immersion)
        float bottom_margin = std::max(18.0f * ui_scaling, static_cast<float>(screen_h) * 0.025f);
        float avail_grid_y = top_bar_rect.y + top_bar_rect.h + 10.0f * ui_scaling;
        float avail_grid_h = static_cast<float>(screen_h) - bottom_margin - avail_grid_y;
        float avail_grid_w = static_cast<float>(screen_w) - 24.0f * ui_scaling;

        float next_w = std::clamp(avail_grid_w * 0.24f, 70.0f * ui_scaling, 110.0f * ui_scaling);
        float gap = 8.0f * ui_scaling;
        float grid_avail_w = avail_grid_w - next_w - gap;

        g_size = static_cast<int>(std::min(avail_grid_h / static_cast<float>(play_grid_y),
                                           grid_avail_w / static_cast<float>(play_grid_x)));
        g_size = std::max(12, g_size);

        float grid_w = static_cast<float>(play_grid_x * g_size);
        float grid_h = static_cast<float>(play_grid_y * g_size);

        float total_content_w = grid_w + gap + next_w;
        start_x = (static_cast<float>(screen_w) - total_content_w) * 0.5f;
        start_y = avail_grid_y + (avail_grid_h - grid_h) * 0.5f;
        grid_rect = {start_x, start_y, grid_w, grid_h};

        next_box_rect = {start_x + grid_w + gap, start_y, next_w, grid_h};

        // Full-screen gesture zone below the top bar
        gesture_zone = {0.0f, top_bar_rect.y + top_bar_rect.h,
                        static_cast<float>(screen_w),
                        static_cast<float>(screen_h) - (top_bar_rect.y + top_bar_rect.h)};
    }
    else
    {
        // --- LANDSCAPE FULL-SCREEN GESTURE LAYOUT ---
        float avail_h = static_cast<float>(screen_h) * 0.90f;
        g_size = static_cast<int>(avail_h / static_cast<float>(play_grid_y));
        g_size = std::max(12, g_size);

        float grid_w = static_cast<float>(play_grid_x * g_size);
        float grid_h = static_cast<float>(play_grid_y * g_size);

        start_x = std::max(180.0f, (static_cast<float>(screen_w) - grid_w) * 0.35f);
        start_y = (static_cast<float>(screen_h) - grid_h) * 0.5f;
        grid_rect = {start_x, start_y, grid_w, grid_h};

        float hold_w = std::min((start_x - 16.0f) * 0.85f, 150.0f);
        float hold_h = std::min(hold_w * 0.9f, 130.0f);
        hold_box_rect = {start_x * 0.5f - hold_w * 0.5f, start_y, hold_w, hold_h};

        float side_x = start_x + grid_w + 16.0f;
        float next_w = std::min(160.0f, (static_cast<float>(screen_w) - side_x) * 0.45f);
        float next_h = std::min(grid_h * 0.55f, 260.0f);
        next_box_rect = {side_x, start_y, next_w, next_h};

        float info_y = next_box_rect.y + next_box_rect.h + 12.0f;
        float info_h = std::max(60.0f, start_y + grid_h - info_y);
        info_box_rect = {side_x, info_y, next_w, info_h};

        float p_btn_s = 48.0f;
        game_buttons.emplace_back(ButtonId::PAUSE, static_cast<float>(screen_w) - p_btn_s - 14.0f, 14.0f,
                                 p_btn_s, p_btn_s, "||", "");

        gesture_zone = {0.0f, 0.0f, static_cast<float>(screen_w), static_cast<float>(screen_h)};
    }

    touch_controller.update_config(static_cast<float>(g_size), ui_scaling);

    // Modal dialog buttons (Intelligently scaled via ui_scaling)
    float dlg_w = std::min(static_cast<float>(screen_w) * 0.85f, 380.0f * ui_scaling);
    float dlg_h = 290.0f * ui_scaling;
    float dlg_x = (static_cast<float>(screen_w) - dlg_w) * 0.5f;
    float dlg_y = (static_cast<float>(screen_h) - dlg_h) * 0.5f;

    float mb_w = dlg_w * 0.78f;
    float mb_h = 54.0f * ui_scaling;
    float mb_x = dlg_x + (dlg_w - mb_w) * 0.5f;

    pause_buttons.emplace_back(ButtonId::RESUME, mb_x, dlg_y + 80.0f * ui_scaling, mb_w, mb_h, "RESUME", "", 16.0f, 8.0f);
    pause_buttons.emplace_back(ButtonId::RETRY, mb_x, dlg_y + 144.0f * ui_scaling, mb_w, mb_h, "RESTART", "", 16.0f, 8.0f);
    pause_buttons.emplace_back(ButtonId::MENU, mb_x, dlg_y + 208.0f * ui_scaling, mb_w, mb_h, "MAIN MENU", "", 16.0f, 8.0f);

    float start_btn_w = std::min(static_cast<float>(screen_w) * 0.75f, 340.0f * ui_scaling);
    float start_btn_h = 68.0f * ui_scaling;
    start_buttons.emplace_back(ButtonId::START, (static_cast<float>(screen_w) - start_btn_w) * 0.5f,
                              static_cast<float>(screen_h) * 0.65f, start_btn_w, start_btn_h, "START GAME", "TAP TO PLAY", 20.0f, 10.0f);

    float go_dlg_w = std::min(static_cast<float>(screen_w) * 0.88f, 400.0f * ui_scaling);
    float go_dlg_h = 350.0f * ui_scaling;
    float go_dlg_x = (static_cast<float>(screen_w) - go_dlg_w) * 0.5f;
    float go_dlg_y = (static_cast<float>(screen_h) - go_dlg_h) * 0.5f;

    float go_btn_w = go_dlg_w * 0.78f;
    float go_btn_h = 54.0f * ui_scaling;
    float go_btn_x = go_dlg_x + (go_dlg_w - go_btn_w) * 0.5f;

    game_over_buttons.emplace_back(ButtonId::RETRY, go_btn_x, go_dlg_y + go_dlg_h - 130.0f * ui_scaling, go_btn_w, go_btn_h, "PLAY AGAIN", "", 16.0f, 8.0f);
    game_over_buttons.emplace_back(ButtonId::MENU, go_btn_x, go_dlg_y + go_dlg_h - 66.0f * ui_scaling, go_btn_w, go_btn_h, "MAIN MENU", "", 16.0f, 8.0f);
}

void Game::refill_bag()
{
    std::vector<char> bag = {'I', 'J', 'L', 'O', 'S', 'T', 'Z'};
    static std::mt19937 rng(std::random_device{}());
    std::ranges::shuffle(bag, rng);
    for (char t : bag)
    {
        next_queue.push_back({t, get_piece_color(t)});
    }
}

PieceInfo Game::pop_next_piece()
{
    if (next_queue.size() < 4)
    {
        refill_bag();
    }
    PieceInfo p = next_queue.front();
    next_queue.erase(next_queue.begin());
    return p;
}

void Game::spawn_new_piece()
{
    PieceInfo p = pop_next_piece();
    cur_tile.setup_piece(p.type, play_grid_x);
    cur_color = p.color;
    can_hold = true;
    is_grounded = false;
    lock_resets = 0;
    last_fall_time = SDL_GetTicks();

    if (collide(cur_tile, 0, 0, play_grid_x, play_grid_y, board))
    {
        state = GameState::GAME_OVER;
        if (score > high_score)
        {
            high_score = score;
            is_new_high_score = true;
            save_high_score();
        }
        else
        {
            is_new_high_score = false;
        }
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Game Over! Final score: %d, High score: %d", score, high_score);
    }
}

void Game::rotate_current_piece(int dir)
{
    if (state != GameState::PLAYING || clear_anim.active)
        return;

    int current_rot = cur_tile.get_rotation();
    cur_tile.rotate(dir);
    int next_rot = cur_tile.get_rotation();

    if (collide(cur_tile, 0, 0, play_grid_x, play_grid_y, board))
    {
        bool kicked = false;
        auto piece_it = wall_kicks.find(static_cast<char>(cur_tile.type));
        if (piece_it != wall_kicks.end())
        {
            auto kick_it = piece_it->second.find({current_rot, next_rot});
            if (kick_it != piece_it->second.end())
            {
                for (const auto &kick : kick_it->second)
                {
                    if (!collide(cur_tile, kick.x, -kick.y, play_grid_x, play_grid_y, board))
                    {
                        cur_tile.x += kick.x;
                        cur_tile.y -= kick.y;
                        kicked = true;
                        break;
                    }
                }
            }
        }
        if (!kicked)
        {
            cur_tile.rotate(-dir);
        }
        else
        {
            if (is_grounded && lock_resets < max_lock_resets)
            {
                ground_start_time = SDL_GetTicks();
                lock_resets++;
            }
        }
    }
    else
    {
        if (is_grounded && lock_resets < max_lock_resets)
        {
            ground_start_time = SDL_GetTicks();
            lock_resets++;
        }
    }
}

bool Game::move_current_piece(int dx)
{
    if (state != GameState::PLAYING || clear_anim.active)
        return false;

    if (!collide(cur_tile, dx, 0, play_grid_x, play_grid_y, board))
    {
        cur_tile.x += dx;
        if (is_grounded && lock_resets < max_lock_resets)
        {
            ground_start_time = SDL_GetTicks();
            lock_resets++;
        }
        return true;
    }
    return false;
}

void Game::hard_drop()
{
    if (state != GameState::PLAYING || clear_anim.active)
        return;

    int ghost_y = get_ghost_y(cur_tile, play_grid_x, play_grid_y, board);
    int dropped = ghost_y - cur_tile.y;
    cur_tile.y = ghost_y;
    score += std::max(0, dropped * 2);
    place_piece();
}

void Game::trigger_hold()
{
    if (!can_hold || state != GameState::PLAYING || clear_anim.active)
        return;

    if (!has_hold)
    {
        hold_piece = {static_cast<char>(cur_tile.type), cur_color};
        has_hold = true;
        spawn_new_piece();
    }
    else
    {
        PieceInfo temp = {static_cast<char>(cur_tile.type), cur_color};
        cur_tile.setup_piece(hold_piece.type, play_grid_x);
        cur_color = hold_piece.color;
        hold_piece = temp;
        is_grounded = false;
        lock_resets = 0;
        last_fall_time = SDL_GetTicks();
    }
    can_hold = false;
}

void Game::reset_game()
{
    board.assign(play_grid_y, std::vector<char>(play_grid_x, 0));
    score = 0;
    lines_cleared_total = 0;
    level = 1;
    fall_delay = base_fall_delay;

    next_queue.clear();
    refill_bag();
    refill_bag();

    has_hold = false;
    can_hold = true;
    hold_piece = {' ', ' '};

    clear_anim.active = false;
    clear_anim.rows.clear();

    touch_left_pressed = false;
    touch_right_pressed = false;
    touch_down_pressed = false;
    active_dir = {false, false};
    das_triggered = {false, false};

    touch_controller.reset();

    spawn_new_piece();
}

void Game::load_high_score()
{
    high_score = 0;

    char* pref_path = SDL_GetPrefPath("com.henryzcode", "ctetris");
    std::string path;
    if (pref_path)
    {
        path = std::string(pref_path) + "highscore.txt";
        SDL_free(pref_path);
    }
    else
    {
        path = "highscore.txt";
    }

    size_t datasize = 0;
    void* data = SDL_LoadFile(path.c_str(), &datasize);
    if (!data)
    {
        // Try fallback to local directory path
        data = SDL_LoadFile("data/highscore.txt", &datasize);
    }

    if (data)
    {
        std::string str(static_cast<const char*>(data), datasize);
        SDL_free(data);
        try
        {
            high_score = std::stoi(str);
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "High score loaded successfully: %d", high_score);
        }
        catch (const std::exception& e)
        {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Failed to parse high score '%s': %s", str.c_str(), e.what());
            high_score = 0;
        }
    }
    else
    {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "No previous high score found. Starting at 0.");
    }
}

void Game::save_high_score()
{
    char* pref_path = SDL_GetPrefPath("com.henryzcode", "ctetris");
    std::string path;
    if (pref_path)
    {
        path = std::string(pref_path) + "highscore.txt";
        SDL_free(pref_path);
    }
    else
    {
        path = "data/highscore.txt";
    }

    SDL_IOStream* io = SDL_IOFromFile(path.c_str(), "w");
    if (!io)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to open '%s' for saving high score: %s",
                     path.c_str(), SDL_GetError());
        return;
    }

    std::string str = std::to_string(high_score);
    size_t written = SDL_WriteIO(io, str.c_str(), str.size());
    SDL_CloseIO(io);

    if (written == str.size())
    {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "High score (%d) saved successfully to '%s'", high_score, path.c_str());
    }
    else
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to write complete high score data: wrote %zu of %zu bytes",
                     written, str.size());
    }
}

void Game::play_next_track()
{
    if (!mixer || !music_track || playlist.empty())
        return;

    if (music_audio)
    {
        MIX_DestroyAudio(music_audio);
        music_audio = nullptr;
    }

    music_audio = MIX_LoadAudio(mixer, playlist[cur_track_index].c_str(), false);
    if (!music_audio)
    {
        std::string alt = "assets/" + playlist[cur_track_index];
        music_audio = MIX_LoadAudio(mixer, alt.c_str(), false);
    }

    if (music_audio)
    {
        MIX_SetTrackAudio(music_track, music_audio);
        MIX_PlayTrack(music_track, 0);
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Playing track [%d]: %s",
                    cur_track_index, playlist[cur_track_index].c_str());
    }
    else
    {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Failed to load music track '%s': %s",
                    playlist[cur_track_index].c_str(), SDL_GetError());
    }

    cur_track_index = (cur_track_index + 1) % static_cast<int>(playlist.size());
}

void Game::update_music()
{
    if (!mixer || !music_track || playlist.empty() || state == GameState::PAUSED)
        return;

    if (!MIX_TrackPlaying(music_track))
    {
        play_next_track();
    }
}

void Game::place_piece()
{
    const auto &shape = cur_tile.get_shape();
    for (size_t r = 0; r < shape.size(); ++r)
    {
        for (size_t c = 0; c < shape[r].size(); ++c)
        {
            if (shape[r][c] != 0)
            {
                int bx = cur_tile.x + static_cast<int>(c);
                int by = cur_tile.y + static_cast<int>(r);
                if (by >= 0 && by < play_grid_y && bx >= 0 && bx < play_grid_x)
                {
                    if (static_cast<size_t>(by) < board.size() && static_cast<size_t>(bx) < board[by].size())
                    {
                        board[by][bx] = cur_color;
                    }
                }
            }
        }
    }

    if (mixer && sfx_land)
        MIX_PlayAudio(mixer, sfx_land);

    handle_line_clears();
}

void Game::handle_line_clears()
{
    std::vector<int> cleared_rows = find_full_lines(board, play_grid_x, play_grid_y);
    if (!cleared_rows.empty())
    {
        clear_anim.rows = cleared_rows;
        clear_anim.start_time = SDL_GetTicks();
        clear_anim.active = true;

        int lines = static_cast<int>(cleared_rows.size());
        static const int base_pts[] = {0, 100, 300, 500, 800};
        score += base_pts[std::min(4, lines)] * level;
        lines_cleared_total += lines;

        int old_level = level;
        level = (lines_cleared_total / 10) + 1;
        if (level > old_level)
        {
            fall_delay = std::max(60, base_fall_delay - (level - 1) * 50);
            if (mixer && sfx_level_up)
                MIX_PlayAudio(mixer, sfx_level_up);
        }
        else
        {
            if (mixer && sfx_clear)
                MIX_PlayAudio(mixer, sfx_clear);
        }

        if (score > high_score)
        {
            high_score = score;
            save_high_score();
        }
    }
    else
    {
        spawn_new_piece();
    }
}

void Game::handle_touch_down(SDL_FingerID finger, float x, float y)
{
    if (state == GameState::START_SCREEN)
    {
        for (auto &btn : start_buttons)
        {
            if (btn.handle_touch_down(finger, x, y))
            {
                if (mixer && sfx_click)
                    MIX_PlayAudio(mixer, sfx_click);
            }
        }
        return;
    }

    if (state == GameState::PAUSED)
    {
        for (auto &btn : pause_buttons)
        {
            if (btn.handle_touch_down(finger, x, y))
            {
                if (mixer && sfx_click)
                    MIX_PlayAudio(mixer, sfx_click);
            }
        }
        return;
    }

    if (state == GameState::GAME_OVER)
    {
        for (auto &btn : game_over_buttons)
        {
            if (btn.handle_touch_down(finger, x, y))
            {
                if (mixer && sfx_click)
                    MIX_PlayAudio(mixer, sfx_click);
            }
        }
        return;
    }

    if (state == GameState::PLAYING)
    {
        bool btn_consumed = false;
        for (auto &btn : game_buttons)
        {
            if (btn.handle_touch_down(finger, x, y))
            {
                btn_consumed = true;
                if (btn.id == ButtonId::PAUSE)
                {
                    state = GameState::PAUSED;
                    if (mixer)
                        MIX_SetMixerGain(mixer, volume * 0.4f);
                    if (mixer && sfx_click)
                        MIX_PlayAudio(mixer, sfx_click);
                }
                else if (btn.id == ButtonId::ROTATE_CW)
                {
                    rotate_current_piece(1);
                }
                else if (btn.id == ButtonId::ROTATE_CCW)
                {
                    rotate_current_piece(-1);
                }
                else if (btn.id == ButtonId::UP)
                {
                    hard_drop();
                }
                else if (btn.id == ButtonId::HOLD)
                {
                    trigger_hold();
                }
                else if (btn.id == ButtonId::LEFT)
                {
                    touch_left_pressed = true;
                    move_current_piece(-1);
                    active_dir[LEFT] = true;
                    das_triggered[LEFT] = false;
                    last_move_time[LEFT] = SDL_GetTicks();
                }
                else if (btn.id == ButtonId::RIGHT)
                {
                    touch_right_pressed = true;
                    move_current_piece(1);
                    active_dir[RIGHT] = true;
                    das_triggered[RIGHT] = false;
                    last_move_time[RIGHT] = SDL_GetTicks();
                }
                else if (btn.id == ButtonId::DOWN)
                {
                    touch_down_pressed = true;
                }
                break;
            }
        }

        if (!btn_consumed)
        {
            touch_controller.on_touch_down(finger, x, y, gesture_zone);
        }
    }
}

void Game::handle_touch_up(SDL_FingerID finger, float x, float y)
{
    if (state == GameState::START_SCREEN)
    {
        for (auto &btn : start_buttons)
        {
            if (btn.handle_touch_up(finger, x, y))
            {
                if (btn.id == ButtonId::START)
                {
                    reset_game();
                    state = GameState::PLAYING;
                }
            }
        }
        return;
    }

    if (state == GameState::PAUSED)
    {
        for (auto &btn : pause_buttons)
        {
            if (btn.handle_touch_up(finger, x, y))
            {
                if (btn.id == ButtonId::RESUME)
                {
                    state = GameState::PLAYING;
                    if (mixer)
                        MIX_SetMixerGain(mixer, norm_vol);
                }
                else if (btn.id == ButtonId::RETRY)
                {
                    reset_game();
                    state = GameState::PLAYING;
                    if (mixer)
                        MIX_SetMixerGain(mixer, norm_vol);
                }
                else if (btn.id == ButtonId::MENU)
                {
                    state = GameState::START_SCREEN;
                    if (mixer)
                        MIX_SetMixerGain(mixer, norm_vol);
                }
            }
        }
        return;
    }

    if (state == GameState::GAME_OVER)
    {
        for (auto &btn : game_over_buttons)
        {
            if (btn.handle_touch_up(finger, x, y))
            {
                if (btn.id == ButtonId::RETRY)
                {
                    reset_game();
                    state = GameState::PLAYING;
                }
                else if (btn.id == ButtonId::MENU)
                {
                    state = GameState::START_SCREEN;
                }
            }
        }
        return;
    }

    if (state == GameState::PLAYING)
    {
        bool was_btn = false;
        for (auto &btn : game_buttons)
        {
            if (btn.active_finger == finger)
            {
                was_btn = true;
                if (btn.id == ButtonId::LEFT)
                {
                    touch_left_pressed = false;
                    active_dir[LEFT] = false;
                    das_triggered[LEFT] = false;
                }
                else if (btn.id == ButtonId::RIGHT)
                {
                    touch_right_pressed = false;
                    active_dir[RIGHT] = false;
                    das_triggered[RIGHT] = false;
                }
                else if (btn.id == ButtonId::DOWN)
                {
                    touch_down_pressed = false;
                }
                btn.handle_touch_up(finger, x, y);
            }
        }

        if (!was_btn)
        {
            bool rot_cw = false;
            bool rot_ccw = false;
            bool h_drop = false;
            bool hold_trig = false;

            if (touch_controller.on_touch_up(finger, x, y, rot_cw, rot_ccw, h_drop, hold_trig))
            {
                if (rot_cw)
                {
                    rotate_current_piece(1);
                }
                else if (rot_ccw)
                {
                    rotate_current_piece(-1);
                }
                else if (h_drop)
                {
                    hard_drop();
                }
                else if (hold_trig)
                {
                    trigger_hold();
                }
            }
            if (!touch_controller.is_soft_drop())
            {
                touch_down_pressed = false;
            }
        }
    }
}

void Game::handle_touch_motion(SDL_FingerID finger, float x, float y)
{
    if (state == GameState::PLAYING)
    {
        for (auto &btn : game_buttons)
        {
            if (btn.active_finger == finger)
            {
                if (!btn.contains(x, y))
                {
                    if (btn.id == ButtonId::LEFT)
                    {
                        touch_left_pressed = false;
                        active_dir[LEFT] = false;
                    }
                    else if (btn.id == ButtonId::RIGHT)
                    {
                        touch_right_pressed = false;
                        active_dir[RIGHT] = false;
                    }
                    else if (btn.id == ButtonId::DOWN)
                    {
                        touch_down_pressed = false;
                    }
                    btn.cancel_touch();
                }
            }
        }

        int move_steps = 0;
        bool gesture_soft_drop = false;
        if (touch_controller.on_touch_motion(finger, x, y, move_steps, gesture_soft_drop))
        {
            if (move_steps != 0)
            {
                int dx = (move_steps > 0) ? 1 : -1;
                for (int i = 0; i < std::abs(move_steps); ++i)
                {
                    if (!move_current_piece(dx))
                        break;
                }
            }
            touch_down_pressed = gesture_soft_drop;
        }
    }
}

void Game::handle_inputs(const SDL_Event &event)
{
    if (event.type == SDL_EVENT_QUIT)
        running = false;

    if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED || event.type == SDL_EVENT_WINDOW_RESIZED)
    {
        update_layout(event.window.data1, event.window.data2);
    }

    // Touch events (normalized coordinates in SDL3)
    if (event.type == SDL_EVENT_FINGER_DOWN)
    {
        float px = event.tfinger.x * static_cast<float>(screen_w);
        float py = event.tfinger.y * static_cast<float>(screen_h);
        handle_touch_down(event.tfinger.fingerID, px, py);
    }
    else if (event.type == SDL_EVENT_FINGER_UP)
    {
        float px = event.tfinger.x * static_cast<float>(screen_w);
        float py = event.tfinger.y * static_cast<float>(screen_h);
        handle_touch_up(event.tfinger.fingerID, px, py);
    }
    else if (event.type == SDL_EVENT_FINGER_CANCELED)
    {
        float px = event.tfinger.x * static_cast<float>(screen_w);
        float py = event.tfinger.y * static_cast<float>(screen_h);
        handle_touch_up(event.tfinger.fingerID, px, py);
        touch_controller.on_touch_cancel(event.tfinger.fingerID);
    }
    else if (event.type == SDL_EVENT_FINGER_MOTION)
    {
        float px = event.tfinger.x * static_cast<float>(screen_w);
        float py = event.tfinger.y * static_cast<float>(screen_h);
        handle_touch_motion(event.tfinger.fingerID, px, py);
    }

    // Mouse events fallback for desktop testing
    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
    {
        if (event.button.which != SDL_TOUCH_MOUSEID)
        {
            handle_touch_down(0, event.button.x, event.button.y);
        }
    }
    else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP)
    {
        if (event.button.which != SDL_TOUCH_MOUSEID)
        {
            handle_touch_up(0, event.button.x, event.button.y);
        }
    }
    else if (event.type == SDL_EVENT_MOUSE_MOTION)
    {
        if (event.motion.which != SDL_TOUCH_MOUSEID)
        {
            handle_touch_motion(0, event.motion.x, event.motion.y);
        }
    }

    // Keyboard controls
    if (event.type == SDL_EVENT_KEY_DOWN)
    {
        if (event.key.key == SDLK_ESCAPE)
        {
            if (state == GameState::PLAYING)
            {
                state = GameState::PAUSED;
                if (mixer)
                    MIX_SetMixerGain(mixer, volume * 0.4f);
            }
            else if (state == GameState::PAUSED)
            {
                state = GameState::PLAYING;
                if (mixer)
                    MIX_SetMixerGain(mixer, norm_vol);
            }
        }

        if (state == GameState::PLAYING)
        {
            if (event.key.key == SDLK_UP || event.key.key == SDLK_X)
            {
                rotate_current_piece(1);
            }
            else if (event.key.key == SDLK_Z)
            {
                rotate_current_piece(-1);
            }
            else if (event.key.key == SDLK_SPACE)
            {
                hard_drop();
            }
            else if (event.key.key == SDLK_C || event.key.key == SDLK_LSHIFT)
            {
                trigger_hold();
            }
        }
        else if (state == GameState::START_SCREEN)
        {
            if (event.key.key == SDLK_RETURN || event.key.key == SDLK_SPACE)
            {
                reset_game();
                state = GameState::PLAYING;
            }
        }
        else if (state == GameState::GAME_OVER)
        {
            if (event.key.key == SDLK_RETURN || event.key.key == SDLK_SPACE)
            {
                reset_game();
                state = GameState::PLAYING;
            }
        }
    }
}

void Game::update_movement(Uint64 cur_time, const bool *key_state)
{
    if (state != GameState::PLAYING || clear_anim.active)
    {
        active_dir = {false, false};
        das_triggered = {false, false};
        return;
    }

    bool left_active = touch_left_pressed || (key_state && (key_state[SDL_SCANCODE_LEFT] || key_state[SDL_SCANCODE_A]));
    bool right_active = touch_right_pressed || (key_state && (key_state[SDL_SCANCODE_RIGHT] || key_state[SDL_SCANCODE_D]));

    std::vector<bool> input_state = {left_active, right_active};
    for (int dir : {LEFT, RIGHT})
    {
        int dx = (dir == LEFT) ? -1 : 1;
        if (input_state[dir])
        {
            if (!active_dir[dir])
            {
                active_dir[dir] = true;
                das_triggered[dir] = false;
                last_move_time[dir] = cur_time;
                move_current_piece(dx);
            }
            else
            {
                Uint64 held = cur_time - last_move_time[dir];
                if (!das_triggered[dir] && held >= static_cast<Uint64>(DAS))
                {
                    das_triggered[dir] = true;
                    last_move_time[dir] = cur_time;
                    held = 0;
                }
                if (das_triggered[dir])
                {
                    int moves = (ARR == 0) ? play_grid_x : static_cast<int>(held / ARR);
                    for (int i = 0; i < moves; ++i)
                    {
                        if (!move_current_piece(dx))
                            break;
                    }
                    if (ARR > 0 && moves > 0)
                    {
                        last_move_time[dir] += moves * ARR;
                    }
                }
            }
        }
        else
        {
            active_dir[dir] = false;
            das_triggered[dir] = false;
        }
    }
}

void Game::update_fall(Uint64 cur_time, int current_fall_delay)
{
    if (state != GameState::PLAYING || clear_anim.active)
    {
        last_fall_time = cur_time;
        return;
    }

    bool on_ground = collide(cur_tile, 0, 1, play_grid_x, play_grid_y, board);
    if (on_ground)
    {
        if (!is_grounded)
        {
            is_grounded = true;
            ground_start_time = cur_time;
        }
        else
        {
            if (cur_time - ground_start_time >= static_cast<Uint64>(lock_delay_ms))
            {
                place_piece();
                is_grounded = false;
                last_fall_time = cur_time;
            }
        }
    }
    else
    {
        is_grounded = false;
        if (cur_time - last_fall_time >= static_cast<Uint64>(current_fall_delay))
        {
            cur_tile.y += 1;
            if (is_soft_dropping)
            {
                score += 1;
            }
            last_fall_time = cur_time;
        }
    }
}

void Game::run()
{
    SDL_Event event;
    while (running)
    {
        Uint64 cur_time = SDL_GetTicks();
        const bool *keys = SDL_GetKeyboardState(nullptr);

        while (SDL_PollEvent(&event))
        {
            handle_inputs(event);
        }

        update_music();

        if (state == GameState::PLAYING)
        {
            if (clear_anim.active)
            {
                if (cur_time - clear_anim.start_time >= clear_anim.duration)
                {
                    remove_lines(board, clear_anim.rows, play_grid_x, play_grid_y);
                    clear_anim.active = false;
                    clear_anim.rows.clear();
                    spawn_new_piece();
                }
            }
            else
            {
                is_soft_dropping = touch_down_pressed ||
                                   (keys && (keys[SDL_SCANCODE_DOWN] || keys[SDL_SCANCODE_S]));
                int cur_fall_delay = is_soft_dropping ? std::max(1, fall_delay / SDF) : fall_delay;
                update_movement(cur_time, keys);
                update_fall(cur_time, cur_fall_delay);
            }
        }

        render();
        SDL_Delay(1000 / FPS);
    }
}

void Game::render()
{
    if (!renderer) return;

    // Background
    SDL_SetRenderDrawColor(renderer, 10, 14, 26, 255);
    SDL_RenderClear(renderer);

    draw_grid();

    float shrink_scale = 1.0f;
    if (clear_anim.active)
    {
        Uint64 elapsed = SDL_GetTicks() - clear_anim.start_time;
        shrink_scale = std::clamp(1.0f - (static_cast<float>(elapsed) / clear_anim.duration), 0.0f, 1.0f);
    }

    // Locked blocks
    if (!board.empty() && static_cast<int>(board.size()) >= play_grid_y)
    {
        for (int r = 0; r < play_grid_y; ++r)
        {
            bool is_clearing = clear_anim.active &&
                               (std::find(clear_anim.rows.begin(), clear_anim.rows.end(), r) != clear_anim.rows.end());

            for (int c = 0; c < play_grid_x; ++c)
            {
                if (board[r][c] != 0)
                {
                    auto it = textures.find(board[r][c]);
                    if (it != textures.end() && it->second)
                    {
                        SDL_FRect dest;
                        if (is_clearing)
                        {
                            float cell_size = static_cast<float>(g_size) * shrink_scale;
                            float offset = (static_cast<float>(g_size) - cell_size) * 0.5f;
                            dest = {start_x + static_cast<float>(c * g_size) + offset,
                                    start_y + static_cast<float>(r * g_size) + offset, cell_size, cell_size};
                        }
                        else
                        {
                            dest = {start_x + static_cast<float>(c * g_size), start_y + static_cast<float>(r * g_size),
                                    static_cast<float>(g_size), static_cast<float>(g_size)};
                        }
                        SDL_RenderTexture(renderer, it->second, nullptr, &dest);
                    }
                }
            }
        }
    }

    // Active falling piece & ghost
    if (state == GameState::PLAYING && !clear_anim.active)
    {
        int ghost_y = get_ghost_y(cur_tile, play_grid_x, play_grid_y, board);
        draw_matrix(cur_tile, cur_tile.x, cur_tile.y, ghost_y, cur_color);
    }

    draw_hud();
    draw_touch_controls();

    if (state == GameState::START_SCREEN)
    {
        draw_start_screen();
    }
    else if (state == GameState::PAUSED)
    {
        draw_pause_menu();
    }
    else if (state == GameState::GAME_OVER)
    {
        draw_game_over_screen();
    }

    SDL_RenderPresent(renderer);
}

void Game::draw_grid()
{
    // Frame background with anti-aliasing (scaled via ui_scaling)
    SDL_FRect frame_rect = {grid_rect.x - 3.0f * ui_scaling, grid_rect.y - 3.0f * ui_scaling,
                            grid_rect.w + 6.0f * ui_scaling, grid_rect.h + 6.0f * ui_scaling};
    draw_aa_panel(renderer, frame_rect, 8.0f, {12, 17, 30, 240}, {55, 95, 175, 220}, 2.0f);

    // Subtle internal grid lines
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 28, 40, 65, 140);
    for (int i = 1; i < play_grid_x; ++i)
    {
        float x = start_x + static_cast<float>(i * g_size);
        SDL_RenderLine(renderer, x, start_y, x, start_y + grid_rect.h);
    }
    for (int j = 1; j < play_grid_y; ++j)
    {
        float y = start_y + static_cast<float>(j * g_size);
        SDL_RenderLine(renderer, start_x, y, start_x + grid_rect.w, y);
    }
}

void Game::draw_matrix(const Tetro &block, int tetro_x, int tetro_y, int ghost_y, char color_key)
{
    auto it = textures.find(color_key);
    if (it == textures.end() || !it->second)
        return;

    const auto &matrix = block.get_shape();
    SDL_Texture *tex = it->second;

    // Ghost piece
    if (ghost_y >= 0 && ghost_y != tetro_y)
    {
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
        SDL_SetTextureAlphaModFloat(tex, GHOST_ALPHA);
        for (size_t r = 0; r < matrix.size(); ++r)
        {
            for (size_t c = 0; c < matrix[r].size(); ++c)
            {
                if (matrix[r][c] != 0)
                {
                    SDL_FRect dest = {start_x + static_cast<float>((tetro_x + static_cast<int>(c)) * g_size),
                                      start_y + static_cast<float>((ghost_y + static_cast<int>(r)) * g_size),
                                      static_cast<float>(g_size), static_cast<float>(g_size)};
                    SDL_RenderTexture(renderer, tex, nullptr, &dest);
                }
            }
        }
        SDL_SetTextureAlphaModFloat(tex, 1.0f);
    }

    // Current piece
    for (size_t r = 0; r < matrix.size(); ++r)
    {
        for (size_t c = 0; c < matrix[r].size(); ++c)
        {
            if (matrix[r][c] != 0)
            {
                SDL_FRect dest = {start_x + static_cast<float>((tetro_x + static_cast<int>(c)) * g_size),
                                  start_y + static_cast<float>((tetro_y + static_cast<int>(r)) * g_size),
                                  static_cast<float>(g_size), static_cast<float>(g_size)};
                SDL_RenderTexture(renderer, tex, nullptr, &dest);
            }
        }
    }
}

void Game::draw_preview_piece(const PieceInfo &piece, float slot_x, float slot_y, float slot_w, float slot_h,
                              float cell_sz, float alpha)
{
    auto it = tetro.find(piece.type);
    if (it == tetro.end())
        return;
    const Matrix &mat = it->second;

    auto tex_it = textures.find(piece.color);
    if (tex_it == textures.end() || !tex_it->second)
        return;
    SDL_Texture *tex = tex_it->second;

    int min_r = 99, max_r = -1, min_c = 99, max_c = -1;
    for (size_t r = 0; r < mat.size(); ++r)
    {
        for (size_t c = 0; c < mat[r].size(); ++c)
        {
            if (mat[r][c] != 0)
            {
                min_r = std::min(min_r, static_cast<int>(r));
                max_r = std::max(max_r, static_cast<int>(r));
                min_c = std::min(min_c, static_cast<int>(c));
                max_c = std::max(max_c, static_cast<int>(c));
            }
        }
    }
    if (max_r < 0)
        return;

    float piece_w = static_cast<float>(max_c - min_c + 1) * cell_sz;
    float piece_h = static_cast<float>(max_r - min_r + 1) * cell_sz;
    float ox = slot_x + (slot_w - piece_w) * 0.5f - static_cast<float>(min_c) * cell_sz;
    float oy = slot_y + (slot_h - piece_h) * 0.5f - static_cast<float>(min_r) * cell_sz;

    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    SDL_SetTextureAlphaModFloat(tex, alpha);

    for (size_t r = 0; r < mat.size(); ++r)
    {
        for (size_t c = 0; c < mat[r].size(); ++c)
        {
            if (mat[r][c] != 0)
            {
                SDL_FRect dest = {ox + static_cast<float>(c) * cell_sz, oy + static_cast<float>(r) * cell_sz,
                                  cell_sz, cell_sz};
                SDL_RenderTexture(renderer, tex, nullptr, &dest);
            }
        }
    }
    SDL_SetTextureAlphaModFloat(tex, 1.0f);
}

void Game::draw_hud()
{
    if (is_portrait)
    {
        // 1. Top Bar Main HUD Card with Anti-Aliased Rounded Corners
        draw_aa_panel(renderer, top_bar_rect, 16.0f, {15, 20, 36, 215}, {45, 80, 155, 180}, 2.0f);

        // A. HOLD Box (Left)
        draw_aa_panel(renderer, hold_box_rect, 10.0f, {10, 14, 26, 200}, {38, 65, 125, 160}, 1.5f);
        if (text_high_score)
        {
            SDL_FRect title_r = {hold_box_rect.x, hold_box_rect.y + 4.0f * ui_scaling, hold_box_rect.w, 16.0f * ui_scaling};
            text_high_score->render_centered(renderer, "HOLD", title_r);
        }
        if (has_hold)
        {
            float preview_cell = std::clamp(hold_box_rect.h * 0.18f, 13.0f * ui_scaling, 22.0f * ui_scaling);
            float slot_y = hold_box_rect.y + 20.0f * ui_scaling;
            float slot_h = hold_box_rect.h - 24.0f * ui_scaling;
            draw_preview_piece(hold_piece, hold_box_rect.x + 4.0f * ui_scaling, slot_y, hold_box_rect.w - 8.0f * ui_scaling, slot_h,
                               preview_cell, can_hold ? 1.0f : 0.35f);
        }

        // B. STATS Box (Center/Right in Top Bar - SCORE, BEST, LEVEL, LINES)
        draw_aa_panel(renderer, info_box_rect, 10.0f, {10, 14, 26, 200}, {38, 65, 125, 160}, 1.5f);
        if (text_score && text_high_score)
        {
            float col_w = (info_box_rect.w - 16.0f * ui_scaling) * 0.5f;
            float col1_x = info_box_rect.x + 10.0f * ui_scaling;
            float col2_x = col1_x + col_w + 4.0f * ui_scaling;
            float row1_y = info_box_rect.y + 8.0f * ui_scaling;
            float row2_y = info_box_rect.y + info_box_rect.h * 0.52f;

            text_score->render(renderer, "SCORE " + std::to_string(score), col1_x, row1_y);
            text_high_score->render(renderer, "BEST  " + std::to_string(high_score), col1_x, row2_y);

            text_high_score->render(renderer, "LEVEL " + std::to_string(level), col2_x, row1_y);
            text_high_score->render(renderer, "LINES " + std::to_string(lines_cleared_total), col2_x, row2_y);
        }

        // C. NEXT Queue (Vertical Column on Right of Main Grid)
        draw_aa_panel(renderer, next_box_rect, 10.0f, {10, 14, 26, 200}, {38, 65, 125, 160}, 1.5f);
        if (text_high_score)
        {
            SDL_FRect title_r = {next_box_rect.x, next_box_rect.y + 6.0f * ui_scaling, next_box_rect.w, 18.0f * ui_scaling};
            text_high_score->render_centered(renderer, "NEXT", title_r);
        }
        float title_space = 28.0f * ui_scaling;
        float queue_start_y = next_box_rect.y + title_space;
        float slot_h = (next_box_rect.h - title_space - 10.0f * ui_scaling) / 3.0f;
        float preview_cell = std::clamp(next_box_rect.w * 0.18f, 12.0f * ui_scaling, 20.0f * ui_scaling);
        for (int i = 0; i < 3 && i < static_cast<int>(next_queue.size()); ++i)
        {
            float sy = queue_start_y + static_cast<float>(i) * slot_h;
            float alpha = (i == 0) ? 1.0f : (i == 1 ? 0.85f : 0.65f);
            draw_preview_piece(next_queue[i], next_box_rect.x + 4.0f * ui_scaling, sy,
                               next_box_rect.w - 8.0f * ui_scaling, slot_h, preview_cell, alpha);

            if (i < 2)
            {
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(renderer, 32, 50, 85, 100);
                float div_y = sy + slot_h;
                SDL_RenderLine(renderer, next_box_rect.x + 8.0f * ui_scaling, div_y,
                               next_box_rect.x + next_box_rect.w - 8.0f * ui_scaling, div_y);
            }
        }
    }
    else
    {
        // Landscape HUD
        float preview_cell = std::clamp(static_cast<float>(g_size) * 0.68f, 12.0f, 26.0f);

        draw_aa_panel(renderer, hold_box_rect, 12.0f, {15, 20, 36, 215}, {45, 80, 155, 180}, 2.0f);
        if (text_high_score)
        {
            SDL_FRect title_r = {hold_box_rect.x, hold_box_rect.y + 4.0f, hold_box_rect.w, 18.0f};
            text_high_score->render_centered(renderer, "HOLD", title_r);
        }
        if (has_hold)
        {
            float slot_y = hold_box_rect.y + 22.0f;
            float slot_h = hold_box_rect.h - 26.0f;
            draw_preview_piece(hold_piece, hold_box_rect.x + 4.0f, slot_y, hold_box_rect.w - 8.0f, slot_h,
                               preview_cell, can_hold ? 1.0f : 0.35f);
        }

        draw_aa_panel(renderer, next_box_rect, 12.0f, {15, 20, 36, 215}, {45, 80, 155, 180}, 2.0f);
        if (text_high_score)
        {
            SDL_FRect title_r = {next_box_rect.x, next_box_rect.y + 4.0f, next_box_rect.w, 18.0f};
            text_high_score->render_centered(renderer, "NEXT", title_r);
        }
        float queue_start_y = next_box_rect.y + 24.0f;
        float slot_h = (next_box_rect.h - 28.0f) / 3.0f;
        for (int i = 0; i < 3 && i < static_cast<int>(next_queue.size()); ++i)
        {
            float sy = queue_start_y + static_cast<float>(i) * slot_h;
            draw_preview_piece(next_queue[i], next_box_rect.x + 4.0f, sy, next_box_rect.w - 8.0f, slot_h,
                               preview_cell, (i == 0) ? 1.0f : (i == 1 ? 0.85f : 0.70f));
        }

        draw_aa_panel(renderer, info_box_rect, 12.0f, {15, 20, 36, 215}, {45, 80, 155, 180}, 2.0f);
        if (text_high_score)
        {
            float line_step = (info_box_rect.h - 8.0f) / 4.0f;
            float tx = info_box_rect.x + 8.0f;

            text_high_score->render(renderer, "SCORE " + std::to_string(score), tx, info_box_rect.y + 4.0f);
            text_high_score->render(renderer, "BEST  " + std::to_string(high_score), tx, info_box_rect.y + 4.0f + line_step);
            text_high_score->render(renderer, "LEVEL " + std::to_string(level), tx, info_box_rect.y + 4.0f + line_step * 2.0f);
            text_high_score->render(renderer, "LINES " + std::to_string(lines_cleared_total), tx, info_box_rect.y + 4.0f + line_step * 3.0f);
        }
    }
}

void Game::draw_touch_controls()
{
    for (auto &btn : game_buttons)
    {
        btn.render(renderer, font_medium ? font_medium : font_small, font_small);
    }
}

void Game::draw_start_screen()
{
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 5, 10, 25, 210);
    SDL_FRect screen_r = {0, 0, static_cast<float>(screen_w), static_cast<float>(screen_h)};
    SDL_RenderFillRect(renderer, &screen_r);

    if (text_banner)
    {
        SDL_FRect title_r = {0, static_cast<float>(screen_h) * 0.28f, static_cast<float>(screen_w), 50.0f * ui_scaling};
        text_banner->render_centered(renderer, "TETRIS", title_r);
    }

    if (text_score)
    {
        SDL_FRect sub_r = {0, static_cast<float>(screen_h) * 0.38f, static_cast<float>(screen_w), 30.0f * ui_scaling};
        text_score->render_centered(renderer, "BEST SCORE: " + std::to_string(high_score), sub_r);
    }

    for (auto &btn : start_buttons)
    {
        btn.render(renderer, font_medium ? font_medium : font_small, font_small);
    }
}

void Game::draw_pause_menu()
{
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 5, 8, 20, 200);
    SDL_FRect screen_r = {0, 0, static_cast<float>(screen_w), static_cast<float>(screen_h)};
    SDL_RenderFillRect(renderer, &screen_r);

    float dlg_w = std::min(static_cast<float>(screen_w) * 0.85f, 380.0f * ui_scaling);
    float dlg_h = 290.0f * ui_scaling;
    float dlg_x = (static_cast<float>(screen_w) - dlg_w) * 0.5f;
    float dlg_y = (static_cast<float>(screen_h) - dlg_h) * 0.5f;
    SDL_FRect dlg_rect = {dlg_x, dlg_y, dlg_w, dlg_h};

    draw_aa_panel(renderer, dlg_rect, 16.0f, {20, 30, 55, 245}, {75, 130, 220, 255}, 2.5f);

    if (text_score)
    {
        SDL_FRect title_r = {dlg_x, dlg_y + 24.0f * ui_scaling, dlg_w, 32.0f * ui_scaling};
        text_score->render_centered(renderer, "GAME PAUSED", title_r);
    }

    for (auto &btn : pause_buttons)
    {
        btn.render(renderer, font_medium ? font_medium : font_small, font_small);
    }
}

void Game::draw_game_over_screen()
{
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 25, 8, 15, 215);
    SDL_FRect screen_r = {0, 0, static_cast<float>(screen_w), static_cast<float>(screen_h)};
    SDL_RenderFillRect(renderer, &screen_r);

    float go_dlg_w = std::min(static_cast<float>(screen_w) * 0.88f, 400.0f * ui_scaling);
    float go_dlg_h = 350.0f * ui_scaling;
    float go_dlg_x = (static_cast<float>(screen_w) - go_dlg_w) * 0.5f;
    float go_dlg_y = (static_cast<float>(screen_h) - go_dlg_h) * 0.5f;
    SDL_FRect dlg_rect = {go_dlg_x, go_dlg_y, go_dlg_w, go_dlg_h};

    draw_aa_panel(renderer, dlg_rect, 16.0f, {35, 18, 28, 245}, {220, 70, 80, 255}, 2.5f);

    if (text_banner)
    {
        SDL_FRect title_r = {go_dlg_x, go_dlg_y + 20.0f * ui_scaling, go_dlg_w, 36.0f * ui_scaling};
        text_banner->render_centered(renderer, "GAME OVER", title_r);
    }

    if (text_high_score)
    {
        float sy = go_dlg_y + 68.0f * ui_scaling;
        if (is_new_high_score)
        {
            SDL_FRect rec_r = {go_dlg_x, sy, go_dlg_w, 20.0f * ui_scaling};
            text_high_score->render_centered(renderer, "* NEW RECORD! *", rec_r);
            sy += 24.0f * ui_scaling;
        }

        SDL_FRect sc_r = {go_dlg_x, sy, go_dlg_w, 20.0f * ui_scaling};
        text_high_score->render_centered(renderer, "FINAL SCORE: " + std::to_string(score), sc_r);
        sy += 24.0f * ui_scaling;

        SDL_FRect ln_r = {go_dlg_x, sy, go_dlg_w, 20.0f * ui_scaling};
        text_high_score->render_centered(renderer, "LINES: " + std::to_string(lines_cleared_total) + "   LEVEL: " + std::to_string(level), ln_r);
    }

    for (auto &btn : game_over_buttons)
    {
        btn.render(renderer, font_medium ? font_medium : font_small, font_small);
    }
}

std::vector<std::string> Game::load_music()
{
    std::vector<std::string> tracks = {
        "music/hyperspace.mp3",
        "music/kora.mp3",
        "music/liquid glass.mp3",
        "music/phrase break.mp3",
        "music/shipwrecked.mp3"
    };

    static std::mt19937 g(std::random_device{}());
    std::ranges::shuffle(tracks, g);
    return tracks;
}

std::map<char, SDL_Texture *> Game::load_textures()
{
    std::map<char, SDL_Texture *> texs;
    if (!renderer) return texs;

    for (const auto &[key, path] : tiles_path)
    {
        SDL_Texture *tex = IMG_LoadTexture(renderer, path.c_str());
        if (!tex)
        {
            std::string sub_path = path.substr(path.find('/') + 1);
            tex = IMG_LoadTexture(renderer, sub_path.c_str());
        }
        if (tex)
        {
            texs[key] = tex;
        }
        else
        {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Failed to load tile texture '%s': %s",
                        path.c_str(), SDL_GetError());
        }
    }
    return texs;
}
