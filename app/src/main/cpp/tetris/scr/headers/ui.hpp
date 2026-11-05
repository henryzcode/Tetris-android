#pragma once

#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <string>
#include <vector>

enum class ButtonId {
    NONE = 0,
    LEFT,
    RIGHT,
    DOWN,
    UP,          // Hard drop
    ROTATE_CW,   // Clockwise
    ROTATE_CCW,  // Counter-Clockwise
    HOLD,
    PAUSE,
    START,
    RESUME,
    RETRY,
    MENU
};

// Global UI scaling factor (default 1.0f, intelligently updated based on screen resolution)
extern float ui_scaling;

// Intelligent scaling helpers
float calculate_smart_ui_scaling(int screen_w, int screen_h);
void update_ui_scaling(int screen_w, int screen_h);
void set_ui_scaling(float scale);
float get_ui_scaling();

inline float ui_scale(float value) {
    return value * ui_scaling;
}

// Anti-aliased rounded rectangle rendering via SDL_RenderGeometry
void draw_aa_filled_rounded_rect(SDL_Renderer* ren, const SDL_FRect& rect, float radius, SDL_Color color);
void draw_aa_rounded_rect_outline(SDL_Renderer* ren, const SDL_FRect& rect, float radius, SDL_Color color, float thickness = 2.0f);
void draw_aa_panel(SDL_Renderer* ren, const SDL_FRect& rect, float radius, SDL_Color bg, SDL_Color border, float border_thickness = 2.0f);

class TouchButton {
public:
    ButtonId id = ButtonId::NONE;
    SDL_FRect rect{0.0f, 0.0f, 0.0f, 0.0f};      // Visible boundary
    SDL_FRect hit_box{0.0f, 0.0f, 0.0f, 0.0f};   // Generous touch hitbox
    float corner_radius = 16.0f;
    std::string label;
    std::string sub_label;
    bool is_pressed = false;
    bool was_clicked = false;
    SDL_FingerID active_finger = -1;
    Uint64 press_time = 0;

    TouchButton() = default;
    TouchButton(ButtonId btn_id, float x, float y, float w, float h,
                const std::string& lbl, const std::string& sub = "",
                float radius = 16.0f, float hit_padding = 8.0f);

    bool contains(float x, float y) const;
    bool handle_touch_down(SDL_FingerID finger, float x, float y);
    bool handle_touch_up(SDL_FingerID finger, float x, float y);
    void handle_touch_motion(SDL_FingerID finger, float x, float y);
    void cancel_touch();
    bool consume_click();

    void render(SDL_Renderer* ren, TTF_Font* font, TTF_Font* font_small = nullptr);
};

class TextCache {
private:
    SDL_Texture* texture = nullptr;
    std::string current_text = "";
    float width = 0.0f;
    float height = 0.0f;
    TTF_Font* font = nullptr;
    SDL_Color color{255, 255, 255, 255};

public:
    TextCache(TTF_Font* f, SDL_Color c);
    ~TextCache();

    TextCache(const TextCache&) = delete;
    TextCache& operator=(const TextCache&) = delete;

    void set_color(SDL_Color c);
    void render(SDL_Renderer* ren, const std::string& new_text, float x, float y);
    void render_centered(SDL_Renderer* ren, const std::string& new_text, const SDL_FRect& target_rect);
    float get_width() const { return width * ui_scaling; }
    float get_height() const { return height * ui_scaling; }
};
