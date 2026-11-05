#include "headers/ui.hpp"
#include <algorithm>
#include <cmath>

// Global UI scaling factor
float ui_scaling = 3.5f;

float calculate_smart_ui_scaling(int screen_w, int screen_h) {
    if (screen_w <= 0 || screen_h <= 0) return 1.0f;
    // Standard mobile baseline resolution: 1080px short edge (e.g. 1080x1920 / 1080x2400)
    float min_dim = static_cast<float>(std::min(screen_w, screen_h));
    float base_dim = 1080.0f;
    float raw_scale = min_dim / base_dim;
    // Intelligently clamp scale so elements remain legible on small screens and balanced on high-DPI displays
    return std::clamp(raw_scale, 0.65f, 2.5f);
}

void update_ui_scaling(int screen_w, int screen_h) {
}

void set_ui_scaling(float scale) {
    ui_scaling = std::clamp(scale, 0.5f, 3.5f);
}

float get_ui_scaling() {
    return ui_scaling;
}

static inline SDL_FColor to_fcolor(SDL_Color c) {
    return SDL_FColor{c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, c.a / 255.0f};
}

void draw_aa_filled_rounded_rect(SDL_Renderer* ren, const SDL_FRect& rect, float radius, SDL_Color color) {
    if (!ren || rect.w <= 0.0f || rect.h <= 0.0f) return;

    radius = std::clamp(radius * ui_scaling, 0.0f, std::min(rect.w, rect.h) * 0.5f);
    if (radius <= 0.5f) {
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(ren, color.r, color.g, color.b, color.a);
        SDL_RenderFillRect(ren, &rect);
        return;
    }

    const int corner_segments = 8;
    std::vector<SDL_Vertex> vertices;
    std::vector<int> indices;

    SDL_FColor fcol = to_fcolor(color);

    // Center point
    float cx = rect.x + rect.w * 0.5f;
    float cy = rect.y + rect.h * 0.5f;
    vertices.push_back(SDL_Vertex{{cx, cy}, fcol, {0.0f, 0.0f}});

    float left = rect.x + radius;
    float right = rect.x + rect.w - radius;
    float top = rect.y + radius;
    float bottom = rect.y + rect.h - radius;

    // 4 Corner Centers and angle ranges
    struct Corner { float ox, oy, start_ang; };
    Corner corners[4] = {
        {right, top, -M_PI_2},       // Top-Right: -90° to 0°
        {right, bottom, 0.0f},       // Bottom-Right: 0° to 90°
        {left, bottom, M_PI_2},      // Bottom-Left: 90° to 180°
        {left, top, M_PI}            // Top-Left: 180° to 270°
    };

    for (int c = 0; c < 4; ++c) {
        for (int i = 0; i <= corner_segments; ++i) {
            float angle = corners[c].start_ang + (M_PI_2 / corner_segments) * i;
            float vx = corners[c].ox + std::cos(angle) * radius;
            float vy = corners[c].oy + std::sin(angle) * radius;
            vertices.push_back(SDL_Vertex{{vx, vy}, fcol, {0.0f, 0.0f}});
        }
    }

    int num_outer = static_cast<int>(vertices.size()) - 1;
    for (int i = 1; i <= num_outer; ++i) {
        int next_i = (i == num_outer) ? 1 : (i + 1);
        indices.push_back(0);
        indices.push_back(i);
        indices.push_back(next_i);
    }

    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_RenderGeometry(ren, nullptr, vertices.data(), static_cast<int>(vertices.size()),
                       indices.data(), static_cast<int>(indices.size()));
}

void draw_aa_rounded_rect_outline(SDL_Renderer* ren, const SDL_FRect& rect, float radius, SDL_Color color, float thickness) {
    if (!ren || rect.w <= 0.0f || rect.h <= 0.0f || thickness <= 0.0f) return;

    radius = std::clamp(radius * ui_scaling, 0.0f, std::min(rect.w, rect.h) * 0.5f);
    thickness = std::max(1.0f, thickness * ui_scaling);
    const int corner_segments = 8;

    std::vector<SDL_Vertex> vertices;
    std::vector<int> indices;

    SDL_FColor fcol = to_fcolor(color);

    float left = rect.x + radius;
    float right = rect.x + rect.w - radius;
    float top = rect.y + radius;
    float bottom = rect.y + rect.h - radius;

    float r_out = radius;
    float r_in = std::max(0.0f, radius - thickness);

    struct Corner { float ox, oy, start_ang; };
    Corner corners[4] = {
        {right, top, -M_PI_2},
        {right, bottom, 0.0f},
        {left, bottom, M_PI_2},
        {left, top, M_PI}
    };

    int vertex_count = 0;
    for (int c = 0; c < 4; ++c) {
        for (int i = 0; i <= corner_segments; ++i) {
            float angle = corners[c].start_ang + (M_PI_2 / corner_segments) * i;
            float cos_a = std::cos(angle);
            float sin_a = std::sin(angle);

            float vx_out = corners[c].ox + cos_a * r_out;
            float vy_out = corners[c].oy + sin_a * r_out;
            float vx_in = corners[c].ox + cos_a * r_in;
            float vy_in = corners[c].oy + sin_a * r_in;

            vertices.push_back(SDL_Vertex{{vx_out, vy_out}, fcol, {0.0f, 0.0f}});
            vertices.push_back(SDL_Vertex{{vx_in, vy_in}, fcol, {0.0f, 0.0f}});
            vertex_count += 2;
        }
    }

    int total_pairs = vertex_count / 2;
    for (int p = 0; p < total_pairs; ++p) {
        int next_p = (p + 1) % total_pairs;

        int o1 = p * 2;
        int i1 = p * 2 + 1;
        int o2 = next_p * 2;
        int i2 = next_p * 2 + 1;

        indices.push_back(o1);
        indices.push_back(i1);
        indices.push_back(o2);

        indices.push_back(o2);
        indices.push_back(i1);
        indices.push_back(i2);
    }

    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_RenderGeometry(ren, nullptr, vertices.data(), static_cast<int>(vertices.size()),
                       indices.data(), static_cast<int>(indices.size()));
}

void draw_aa_panel(SDL_Renderer* ren, const SDL_FRect& rect, float radius, SDL_Color bg, SDL_Color border, float border_thickness) {
    if (bg.a > 0) {
        draw_aa_filled_rounded_rect(ren, rect, radius, bg);
    }
    if (border.a > 0 && border_thickness > 0.0f) {
        draw_aa_rounded_rect_outline(ren, rect, radius, border, border_thickness);
    }
}

TouchButton::TouchButton(ButtonId btn_id, float x, float y, float w, float h,
                         const std::string& lbl, const std::string& sub,
                         float radius, float hit_padding)
    : id(btn_id), rect{x, y, w, h}, corner_radius(radius), label(lbl), sub_label(sub) {
    float pad = hit_padding * ui_scaling;
    hit_box = {x - pad, y - pad, w + pad * 2.0f, h + pad * 2.0f};
}

bool TouchButton::contains(float x, float y) const {
    return (x >= hit_box.x && x <= hit_box.x + hit_box.w &&
            y >= hit_box.y && y <= hit_box.y + hit_box.h);
}

bool TouchButton::handle_touch_down(SDL_FingerID finger, float x, float y) {
    if (contains(x, y)) {
        active_finger = finger;
        is_pressed = true;
        was_clicked = true;
        press_time = SDL_GetTicks();
        return true;
    }
    return false;
}

bool TouchButton::handle_touch_up(SDL_FingerID finger, float x, float y) {
    (void)x;
    (void)y;
    if (active_finger == finger) {
        active_finger = -1;
        is_pressed = false;
        return true;
    }
    return false;
}

void TouchButton::handle_touch_motion(SDL_FingerID finger, float x, float y) {
    if (active_finger == finger) {
        if (!contains(x, y)) {
            active_finger = -1;
            is_pressed = false;
        }
    }
}

void TouchButton::cancel_touch() {
    active_finger = -1;
    is_pressed = false;
    was_clicked = false;
}

bool TouchButton::consume_click() {
    if (was_clicked) {
        was_clicked = false;
        return true;
    }
    return false;
}

void TouchButton::render(SDL_Renderer* ren, TTF_Font* font, TTF_Font* font_small) {
    if (!ren) return;

    SDL_FRect draw_box = rect;
    if (is_pressed) {
        draw_box.y += 2.0f * ui_scaling;
    }

    SDL_Color bg;
    SDL_Color border;

    if (id == ButtonId::UP) {
        // Hard Drop accent color
        bg = is_pressed ? SDL_Color{180, 50, 60, 240} : SDL_Color{75, 24, 32, 205};
        border = is_pressed ? SDL_Color{255, 140, 150, 255} : SDL_Color{180, 65, 80, 210};
    } else if (id == ButtonId::ROTATE_CW || id == ButtonId::ROTATE_CCW) {
        // Rotation accent color
        bg = is_pressed ? SDL_Color{25, 130, 190, 240} : SDL_Color{16, 45, 85, 210};
        border = is_pressed ? SDL_Color{100, 210, 255, 255} : SDL_Color{45, 120, 195, 210};
    } else if (id == ButtonId::HOLD) {
        // Hold accent color
        bg = is_pressed ? SDL_Color{140, 95, 30, 240} : SDL_Color{55, 38, 16, 210};
        border = is_pressed ? SDL_Color{250, 195, 80, 255} : SDL_Color{165, 115, 35, 210};
    } else {
        // Standard Directional / Action buttons
        bg = is_pressed ? SDL_Color{35, 115, 215, 235} : SDL_Color{18, 28, 52, 205};
        border = is_pressed ? SDL_Color{140, 220, 255, 255} : SDL_Color{70, 110, 175, 210};
    }

    draw_aa_panel(ren, draw_box, corner_radius, bg, border, 2.0f);

    float cx = draw_box.x + draw_box.w * 0.5f;
    float cy = draw_box.y + draw_box.h * 0.5f;

    SDL_Color icon_color = is_pressed ? SDL_Color{255, 255, 255, 255} : SDL_Color{225, 238, 255, 240};
    SDL_SetRenderDrawColor(ren, icon_color.r, icon_color.g, icon_color.b, icon_color.a);

    // Vector shapes for Pause, Left, Right, Down, Up
    if (id == ButtonId::PAUSE) {
        float bar_w = std::max(3.0f * ui_scaling, draw_box.w * 0.12f);
        float bar_h = std::max(10.0f * ui_scaling, draw_box.h * 0.45f);
        float gap = std::max(3.0f * ui_scaling, draw_box.w * 0.12f);
        SDL_FRect b1 = {cx - gap * 0.5f - bar_w, cy - bar_h * 0.5f, bar_w, bar_h};
        SDL_FRect b2 = {cx + gap * 0.5f, cy - bar_h * 0.5f, bar_w, bar_h};
        SDL_RenderFillRect(ren, &b1);
        SDL_RenderFillRect(ren, &b2);
        return;
    }

    float stroke_step = std::max(1.0f, 1.0f * ui_scaling);

    if (id == ButtonId::LEFT) {
        float s = std::min(draw_box.w, draw_box.h) * 0.32f;
        for (float i = 0; i <= s; i += stroke_step) {
            float h_len = (i / s) * s;
            SDL_RenderLine(ren, cx - s * 0.5f + i, cy - h_len, cx - s * 0.5f + i, cy + h_len);
        }
        return;
    }

    if (id == ButtonId::RIGHT) {
        float s = std::min(draw_box.w, draw_box.h) * 0.32f;
        for (float i = 0; i <= s; i += stroke_step) {
            float h_len = (1.0f - i / s) * s;
            SDL_RenderLine(ren, cx - s * 0.5f + i, cy - h_len, cx - s * 0.5f + i, cy + h_len);
        }
        return;
    }

    if (id == ButtonId::DOWN) {
        float s = std::min(draw_box.w, draw_box.h) * 0.32f;
        for (float i = 0; i <= s; i += stroke_step) {
            float w_len = (1.0f - i / s) * s;
            SDL_RenderLine(ren, cx - w_len, cy - s * 0.5f + i, cx + w_len, cy - s * 0.5f + i);
        }
        return;
    }

    if (id == ButtonId::UP) {
        // Double chevron for Hard Drop
        float s = std::min(draw_box.w, draw_box.h) * 0.28f;
        for (float i = 0; i <= s; i += stroke_step) {
            float w_len = (i / s) * s;
            SDL_RenderLine(ren, cx - w_len, cy - s * 0.3f + i, cx + w_len, cy - s * 0.3f + i);
            SDL_RenderLine(ren, cx - w_len, cy - s * 0.9f + i, cx + w_len, cy - s * 0.9f + i);
        }
        return;
    }

    // Text Label for Rotate CW, CCW, HOLD, START, RESUME, RETRY, etc.
    if (!label.empty() && font) {
        SDL_Surface* surf = TTF_RenderText_Solid(font, label.c_str(), 0, icon_color);
        if (surf) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(ren, surf);
            if (tex) {
                float tw = static_cast<float>(surf->w) * ui_scaling;
                float th = static_cast<float>(surf->h) * ui_scaling;
                float tx = cx - tw * 0.5f;
                float ty = sub_label.empty() ? (cy - th * 0.5f) : (cy - th * 0.65f);
                SDL_FRect dest = {tx, ty, tw, th};
                SDL_RenderTexture(ren, tex, nullptr, &dest);
                SDL_DestroyTexture(tex);
            }
            SDL_DestroySurface(surf);
        }
    }

    if (!sub_label.empty() && font_small) {
        SDL_Color sub_color = is_pressed ? SDL_Color{220, 240, 255, 240} : SDL_Color{160, 195, 235, 210};
        SDL_Surface* surf_sub = TTF_RenderText_Solid(font_small, sub_label.c_str(), 0, sub_color);
        if (surf_sub) {
            SDL_Texture* tex_sub = SDL_CreateTextureFromSurface(ren, surf_sub);
            if (tex_sub) {
                float stw = static_cast<float>(surf_sub->w) * ui_scaling;
                float sth = static_cast<float>(surf_sub->h) * ui_scaling;
                float stx = cx - stw * 0.5f;
                float sty = cy + sth * 0.25f;
                SDL_FRect dest_sub = {stx, sty, stw, sth};
                SDL_RenderTexture(ren, tex_sub, nullptr, &dest_sub);
                SDL_DestroyTexture(tex_sub);
            }
            SDL_DestroySurface(surf_sub);
        }
    }
}

TextCache::TextCache(TTF_Font* f, SDL_Color c) : font(f), color(c) {}

TextCache::~TextCache() {
    if (texture) {
        SDL_DestroyTexture(texture);
        texture = nullptr;
    }
}

void TextCache::set_color(SDL_Color c) {
    if (c.r != color.r || c.g != color.g || c.b != color.b || c.a != color.a) {
        color = c;
        current_text = "";
    }
}

void TextCache::render(SDL_Renderer* ren, const std::string& new_text, float x, float y) {
    if (new_text.empty() || !font || !ren) return;

    if (new_text != current_text || !texture) {
        if (texture) {
            SDL_DestroyTexture(texture);
            texture = nullptr;
        }
        current_text = new_text;
        SDL_Surface* surface = TTF_RenderText_Solid(font, current_text.c_str(), 0, color);
        if (surface) {
            width = static_cast<float>(surface->w);
            height = static_cast<float>(surface->h);
            texture = SDL_CreateTextureFromSurface(ren, surface);
            SDL_DestroySurface(surface);
        }
    }
    if (texture) {
        SDL_FRect dest = {x, y, width * ui_scaling, height * ui_scaling};
        SDL_RenderTexture(ren, texture, nullptr, &dest);
    }
}

void TextCache::render_centered(SDL_Renderer* ren, const std::string& new_text, const SDL_FRect& target_rect) {
    if (new_text.empty() || !font || !ren) return;

    if (new_text != current_text || !texture) {
        if (texture) {
            SDL_DestroyTexture(texture);
            texture = nullptr;
        }
        current_text = new_text;
        SDL_Surface* surface = TTF_RenderText_Solid(font, current_text.c_str(), 0, color);
        if (surface) {
            width = static_cast<float>(surface->w);
            height = static_cast<float>(surface->h);
            texture = SDL_CreateTextureFromSurface(ren, surface);
            SDL_DestroySurface(surface);
        }
    }
    if (texture) {
        float scaled_w = width * ui_scaling;
        float scaled_h = height * ui_scaling;
        float cx = target_rect.x + (target_rect.w - scaled_w) * 0.5f;
        float cy = target_rect.y + (target_rect.h - scaled_h) * 0.5f;
        SDL_FRect dest = {cx, cy, scaled_w, scaled_h};
        SDL_RenderTexture(ren, texture, nullptr, &dest);
    }
}
