#pragma once

#include <SDL3/SDL.h>
#include <cmath>
#include <algorithm>

enum class GestureAxis {
    NONE = 0,
    HORIZONTAL,
    VERTICAL
};

struct TouchGestureData {
    SDL_FingerID finger_id = -1;
    float start_x = 0.0f;
    float start_y = 0.0f;
    float last_x = 0.0f;
    float last_y = 0.0f;
    float last_drag_x = 0.0f;
    Uint64 start_time = 0;
    GestureAxis axis = GestureAxis::NONE;
    bool is_soft_dropping = false;
    int moved_steps = 0;
};

class TouchController {
public:
    TouchController() = default;

    void update_config(float block_size, float ui_scale);

    // Call on touch down. Returns true if touch was within touch_zone and captured.
    bool on_touch_down(SDL_FingerID finger, float x, float y, const SDL_FRect& touch_zone);

    // Call on motion. Outputs discrete step moves (>0 right, <0 left) and soft drop state.
    bool on_touch_motion(SDL_FingerID finger, float x, float y, int& out_move_steps, bool& out_soft_drop);

    // Call on touch up. Outputs gestures: rotate_cw, rotate_ccw, hard_drop, hold.
    bool on_touch_up(SDL_FingerID finger, float x, float y,
                     bool& out_rotate_cw, bool& out_rotate_ccw,
                     bool& out_hard_drop, bool& out_hold);

    // Call when touch is cancelled
    void on_touch_cancel(SDL_FingerID finger);

    void reset();

    bool is_active() const { return tracking_finger != -1; }
    bool is_soft_drop() const { return soft_dropping; }

private:
    SDL_FingerID tracking_finger = -1;
    TouchGestureData gesture;
    float block_sz = 32.0f;
    float scale = 1.0f;
    bool soft_dropping = false;
    int active_touch_count = 0;
    bool multi_touch_detected = false;
};
