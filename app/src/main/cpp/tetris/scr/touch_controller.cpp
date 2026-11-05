#include "headers/touch_controller.hpp"

void TouchController::update_config(float block_size, float ui_scale)
{
    block_sz = std::max(12.0f, block_size);
    scale = std::max(0.5f, ui_scale);
}

bool TouchController::on_touch_down(SDL_FingerID finger, float x, float y, const SDL_FRect &touch_zone)
{
    active_touch_count++;
    if (active_touch_count > 1)
    {
        multi_touch_detected = true;
    }

    // Check if the touch falls inside the primary active touch zone (main board/screen)
    if (x < touch_zone.x || x > touch_zone.x + touch_zone.w ||
        y < touch_zone.y || y > touch_zone.y + touch_zone.h)
    {
        return false;
    }

    if (tracking_finger == -1)
    {
        tracking_finger = finger;
        gesture.finger_id = finger;
        gesture.start_x = x;
        gesture.start_y = y;
        gesture.last_x = x;
        gesture.last_y = y;
        gesture.last_drag_x = x;
        gesture.start_time = SDL_GetTicks();
        gesture.axis = GestureAxis::NONE;
        gesture.is_soft_dropping = false;
        gesture.moved_steps = 0;
        return true;
    }
    return false;
}

bool TouchController::on_touch_motion(SDL_FingerID finger, float x, float y, int &out_move_steps, bool &out_soft_drop)
{
    if (finger != tracking_finger)
    {
        return false;
    }

    float dx = x - gesture.start_x;
    float dy = y - gesture.start_y;
    float abs_dx = std::abs(dx);
    float abs_dy = std::abs(dy);

    float lock_thresh = 12.0f * scale;

    // 5. Axis Locking
    if (gesture.axis == GestureAxis::NONE)
    {
        if (abs_dx > lock_thresh && abs_dx > abs_dy * 1.25f)
        {
            gesture.axis = GestureAxis::HORIZONTAL;
        }
        else if (abs_dy > lock_thresh && abs_dy > abs_dx * 1.25f)
        {
            gesture.axis = GestureAxis::VERTICAL;
        }
    }

    // 1. Horizontal Step Movement
    if (gesture.axis == GestureAxis::HORIZONTAL)
    {
        float step_sz = block_sz * 0.92f;
        float drag_delta = x - gesture.last_drag_x;
        if (drag_delta >= step_sz)
        {
            int steps = static_cast<int>(drag_delta / step_sz);
            out_move_steps += steps;
            gesture.last_drag_x += static_cast<float>(steps) * step_sz;
            gesture.moved_steps += steps;
        }
        else if (drag_delta <= -step_sz)
        {
            int steps = static_cast<int>(-drag_delta / step_sz);
            out_move_steps -= steps;
            gesture.last_drag_x -= static_cast<float>(steps) * step_sz;
            gesture.moved_steps -= steps;
        }
    }

    // 3. Vertical Soft Drop (Slow Drag Down)
    if (gesture.axis == GestureAxis::VERTICAL)
    {
        float soft_thresh = 22.0f * scale;
        if (dy > soft_thresh && (y - gesture.last_y) >= -6.0f)
        {
            soft_dropping = true;
            gesture.is_soft_dropping = true;
            out_soft_drop = true;
        }
        else if (dy < soft_thresh * 0.6f || (gesture.last_y - y) > 15.0f * scale)
        {
            soft_dropping = false;
            gesture.is_soft_dropping = false;
            out_soft_drop = false;
        }
    }

    gesture.last_x = x;
    gesture.last_y = y;
    return true;
}

bool TouchController::on_touch_up(SDL_FingerID finger, float x, float y,
                                  bool &out_rotate_cw, bool &out_rotate_ccw,
                                  bool &out_hard_drop, bool &out_hold)
{
    if (active_touch_count > 0)
    {
        active_touch_count--;
    }

    if (finger != tracking_finger)
    {
        if (active_touch_count == 0)
        {
            multi_touch_detected = false;
        }
        return false;
    }

    Uint64 now = SDL_GetTicks();
    Uint64 duration = now - gesture.start_time;
    float dx = x - gesture.start_x;
    float dy = y - gesture.start_y;
    float abs_dx = std::abs(dx);
    float abs_dy = std::abs(dy);
    float dist = std::sqrt(dx * dx + dy * dy);
    float dt_f = static_cast<float>(std::max<Uint64>(1, duration));
    float vy = dy / dt_f;

    if (soft_dropping)
    {
        soft_dropping = false;
    }

    bool handled = false;

    // 4. Hard Drop (Flick / Swipe Down)
    if (dy > 45.0f * scale && vy > 0.55f && dy > abs_dx * 1.15f)
    {
        out_hard_drop = true;
        handled = true;
    }
    // 5. Swipe Up (HOLD)
    else if (dy < -35.0f * scale && vy < -0.40f && -dy > abs_dx * 1.15f)
    {
        out_hold = true;
        handled = true;
    }
    // 2. Click / Tap (Rotate)
    else if (duration < 220 && dist < 18.0f * scale && gesture.moved_steps == 0)
    {
        if (multi_touch_detected)
        {
            out_rotate_ccw = true;
        }
        else
        {
            out_rotate_cw = true;
        }
        handled = true;
    }

    tracking_finger = -1;
    gesture.finger_id = -1;
    if (active_touch_count == 0)
    {
        multi_touch_detected = false;
    }
    return handled;
}

void TouchController::on_touch_cancel(SDL_FingerID finger)
{
    if (active_touch_count > 0)
    {
        active_touch_count--;
    }
    if (finger == tracking_finger)
    {
        tracking_finger = -1;
        gesture.finger_id = -1;
        soft_dropping = false;
    }
    if (active_touch_count == 0)
    {
        multi_touch_detected = false;
    }
}

void TouchController::reset()
{
    tracking_finger = -1;
    gesture.finger_id = -1;
    soft_dropping = false;
    active_touch_count = 0;
    multi_touch_detected = false;
}
