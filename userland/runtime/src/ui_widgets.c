#include <reliefos/ui.h>

#include "ui_internal.h"

void reliefos_ui_bevel(struct reliefos_ui_surface *surface, uint32_t x, uint32_t y,
                     uint32_t w, uint32_t h, uint32_t fill, uint32_t flags)
{
    if (ui_theme_is_metro()) {
        uint32_t border = (flags & RELIEFOS_UI_BUTTON_PRESSED)
                              ? RELIEFOS_UI_ACTIVE_TITLE
                              : reliefos_ui_color(RELIEFOS_UI_COLOR_BORDER);
        reliefos_ui_rect(surface, x, y, w, h, fill);
        reliefos_ui_rect(surface, x, y, w, 1, border);
        reliefos_ui_rect(surface, x, y, 1, h, border);
        reliefos_ui_rect(surface, x + w - 1, y, 1, h, border);
        reliefos_ui_rect(surface, x, y + h - 1, w, 1, border);
        return;
    }
    int pressed = (flags & RELIEFOS_UI_BUTTON_PRESSED) != 0;
    uint32_t tl = pressed ? RELIEFOS_UI_DARK : RELIEFOS_UI_WHITE;
    uint32_t br = pressed ? RELIEFOS_UI_WHITE : RELIEFOS_UI_BLACK;

    reliefos_ui_rect(surface, x, y, w, h, fill);
    reliefos_ui_rect(surface, x, y, w, 1, tl);
    reliefos_ui_rect(surface, x, y, 1, h, tl);
    reliefos_ui_rect(surface, x + w - 1, y, 1, h, br);
    reliefos_ui_rect(surface, x, y + h - 1, w, 1, br);
    if (w > 3 && h > 3) {
        reliefos_ui_rect(surface, x + 1, y + 1, w - 2, 1, pressed ? RELIEFOS_UI_BLACK : RELIEFOS_UI_LIGHT);
        reliefos_ui_rect(surface, x + 1, y + 1, 1, h - 2, pressed ? RELIEFOS_UI_BLACK : RELIEFOS_UI_LIGHT);
        reliefos_ui_rect(surface, x + w - 2, y + 1, 1, h - 2, pressed ? RELIEFOS_UI_LIGHT : RELIEFOS_UI_DARK);
        reliefos_ui_rect(surface, x + 1, y + h - 2, w - 2, 1, pressed ? RELIEFOS_UI_LIGHT : RELIEFOS_UI_DARK);
    }
}

void reliefos_ui_inset(struct reliefos_ui_surface *surface, uint32_t x, uint32_t y,
                     uint32_t w, uint32_t h, uint32_t fill)
{
    if (ui_theme_is_metro()) {
        uint32_t border = reliefos_ui_color(RELIEFOS_UI_COLOR_BORDER);
        reliefos_ui_rect(surface, x, y, w, h, fill);
        reliefos_ui_rect(surface, x, y, w, 1, border);
        reliefos_ui_rect(surface, x, y, 1, h, border);
        reliefos_ui_rect(surface, x + w - 1, y, 1, h, border);
        reliefos_ui_rect(surface, x, y + h - 1, w, 1, border);
        return;
    }
    reliefos_ui_rect(surface, x, y, w, h, fill);
    reliefos_ui_rect(surface, x, y, w, 1, RELIEFOS_UI_DARK);
    reliefos_ui_rect(surface, x, y, 1, h, RELIEFOS_UI_DARK);
    reliefos_ui_rect(surface, x + w - 1, y, 1, h, RELIEFOS_UI_WHITE);
    reliefos_ui_rect(surface, x, y + h - 1, w, 1, RELIEFOS_UI_WHITE);
}

void reliefos_ui_button(struct reliefos_ui_surface *surface, uint32_t x, uint32_t y,
                      uint32_t w, uint32_t h, const char *label, uint32_t flags)
{
    reliefos_ui_cursor_region(surface, (int32_t)x, (int32_t)y, w, h,
                            (flags & RELIEFOS_UI_BUTTON_DISABLED)
                                ? RELIEFOS_GUI_CURSOR_NO : RELIEFOS_GUI_CURSOR_HAND,
                            (flags & RELIEFOS_UI_BUTTON_DISABLED)
                                ? RELIEFOS_GUI_CURSOR_REGION_DISABLED : 0);
    uint32_t fill = (flags & RELIEFOS_UI_BUTTON_DISABLED) ? RELIEFOS_UI_LIGHT : RELIEFOS_UI_GRAY;
    uint32_t pressed = flags & RELIEFOS_UI_BUTTON_PRESSED;
    reliefos_ui_bevel(surface, x, y, w, h, fill, pressed);
    if (!label) {
        return;
    }
    uint32_t text_w = reliefos_ui_text_width(label);
    uint32_t tx = text_w < w ? x + (w - text_w) / 2 : x + 4;
    uint32_t ty = RELIEFOS_FONT_H < h ? y + (h - RELIEFOS_FONT_H) / 2 : y + 2;
    if (pressed) {
        ++tx;
        ++ty;
    }
    reliefos_ui_text_transparent_clipped(surface, tx, ty, w > 8 ? w - 8 : w, label,
                                       (flags & RELIEFOS_UI_BUTTON_DISABLED) ? RELIEFOS_UI_DARK : RELIEFOS_UI_BLACK);
}

void reliefos_ui_window_button(struct reliefos_ui_surface *surface, uint32_t x, uint32_t y,
                             char label, uint32_t flags)
{
    ui_window_button_draw(surface, x, y, label, flags);
}

void reliefos_ui_window(struct reliefos_ui_surface *surface, uint32_t x, uint32_t y,
                      uint32_t w, uint32_t h, const char *title, uint32_t flags,
                      struct reliefos_ui_window_parts *parts)
{
    reliefos_ui_window_ex(surface, x, y, w, h, title, 'M', flags, parts);
}

void reliefos_ui_window_ex(struct reliefos_ui_surface *surface, uint32_t x, uint32_t y,
                         uint32_t w, uint32_t h, const char *title, char maximize_label,
                         uint32_t flags, struct reliefos_ui_window_parts *parts)
{
    uint32_t active = flags & RELIEFOS_UI_WINDOW_ACTIVE;
    uint32_t no_resize = flags & RELIEFOS_UI_WINDOW_NO_RESIZE;
    uint32_t title_color = active ? RELIEFOS_UI_ACTIVE_TITLE : RELIEFOS_UI_INACTIVE_TITLE;
    reliefos_ui_bevel(surface, x, y, w, h, RELIEFOS_UI_GRAY, 0);
    reliefos_ui_rect(surface, x + 4, y + 4, w > 8 ? w - 8 : 0, RELIEFOS_UI_TITLEBAR_H, title_color);
    reliefos_ui_text(surface, x + 10, y + 9, title, RELIEFOS_UI_WHITE, title_color);

    uint32_t bx = x + w - 64;
    uint32_t by = y + 6;
    uint32_t minimize_bx = no_resize ? bx + 20 : bx;
    reliefos_ui_window_button(surface, minimize_bx, by, '_', 0);
    if (!no_resize) {
        reliefos_ui_window_button(surface, bx + 20, by, maximize_label, 0);
    }
    reliefos_ui_window_button(surface, bx + 40, by, 'X', 0);

    uint32_t body_x = x + 8;
    uint32_t body_y = y + RELIEFOS_UI_TITLEBAR_H + 10;
    uint32_t body_w = w > 16 ? w - 16 : 0;
    uint32_t body_h = h > RELIEFOS_UI_TITLEBAR_H + 18 ? h - RELIEFOS_UI_TITLEBAR_H - 18 : 0;
    reliefos_ui_inset(surface, body_x, body_y, body_w, body_h, RELIEFOS_UI_WHITE);

    if (parts) {
        parts->titlebar = (struct reliefos_ui_rect){(int32_t)x + 4, (int32_t)y + 4, w > 8 ? w - 8 : 0, RELIEFOS_UI_TITLEBAR_H};
        parts->body = (struct reliefos_ui_rect){(int32_t)body_x, (int32_t)body_y, body_w, body_h};
        parts->minimize = (struct reliefos_ui_rect){(int32_t)minimize_bx, (int32_t)by, RELIEFOS_UI_WINDOW_BUTTON_W, RELIEFOS_UI_WINDOW_BUTTON_H};
        parts->maximize = (struct reliefos_ui_rect){0};
        if (!no_resize) {
            parts->maximize = (struct reliefos_ui_rect){(int32_t)bx + 20, (int32_t)by, RELIEFOS_UI_WINDOW_BUTTON_W, RELIEFOS_UI_WINDOW_BUTTON_H};
        }
        parts->close = (struct reliefos_ui_rect){(int32_t)bx + 40, (int32_t)by, RELIEFOS_UI_WINDOW_BUTTON_W, RELIEFOS_UI_WINDOW_BUTTON_H};
    }
}

void reliefos_ui_taskbar(struct reliefos_ui_surface *surface, uint32_t y, uint32_t h)
{
    reliefos_ui_rect(surface, 0, y, surface ? surface->width : 0, h, RELIEFOS_UI_GRAY);
    if (ui_theme_is_metro()) {
        reliefos_ui_rect(surface, 0, y, surface ? surface->width : 0, 1,
                       reliefos_ui_color(RELIEFOS_UI_COLOR_BORDER));
        return;
    }
    reliefos_ui_rect(surface, 0, y, surface ? surface->width : 0, 2, RELIEFOS_UI_WHITE);
    reliefos_ui_rect(surface, 0, y + 2, surface ? surface->width : 0, 1, RELIEFOS_UI_DARK);
}

void reliefos_ui_taskbar_button(struct reliefos_ui_surface *surface, uint32_t x, uint32_t y,
                              uint32_t w, const char *label, uint32_t flags)
{
    reliefos_ui_button(surface, x, y, w, RELIEFOS_UI_BUTTON_H, label,
                     (flags & RELIEFOS_UI_BUTTON_ACTIVE) ? RELIEFOS_UI_BUTTON_PRESSED : 0);
}

void reliefos_ui_menu(struct reliefos_ui_surface *surface, uint32_t x, uint32_t y,
                    uint32_t w, uint32_t h)
{
    reliefos_ui_bevel(surface, x, y, w, h, RELIEFOS_UI_GRAY, 0);
    if (!ui_theme_is_metro() && h > 8) {
        reliefos_ui_rect(surface, x + 4, y + 4, 26, h - 8, RELIEFOS_UI_ACTIVE_TITLE);
    }
}

void reliefos_ui_menu_item(struct reliefos_ui_surface *surface, uint32_t x, uint32_t y,
                         uint32_t w, const char *label, uint32_t flags)
{
    uint32_t disabled = flags & RELIEFOS_UI_MENU_DISABLED;
    if (flags & RELIEFOS_UI_MENU_SEPARATOR) {
        reliefos_ui_rect(surface, x, y + 9, w, 1, RELIEFOS_UI_DARK);
        reliefos_ui_rect(surface, x, y + 10, w, 1, RELIEFOS_UI_WHITE);
        return;
    }
    reliefos_ui_cursor_region(surface, (int32_t)x, (int32_t)y, w, RELIEFOS_FONT_H + 8,
                            disabled ? RELIEFOS_GUI_CURSOR_NO : RELIEFOS_GUI_CURSOR_HAND,
                            disabled ? RELIEFOS_GUI_CURSOR_REGION_DISABLED : 0);
    if ((flags & RELIEFOS_UI_MENU_SELECTED) && !disabled) {
        reliefos_ui_rect(surface, x, y, w, RELIEFOS_FONT_H + 8, RELIEFOS_UI_ACTIVE_TITLE);
        reliefos_ui_text_transparent(surface, x + 4, y + 4, label, RELIEFOS_UI_WHITE);
    } else {
        reliefos_ui_text_transparent(surface, x + 4, y + 4, label,
                                   disabled ? RELIEFOS_UI_DARK : RELIEFOS_UI_BLACK);
    }
}

uint32_t reliefos_ui_context_menu_height(uint32_t count)
{
    return 8 + count * (RELIEFOS_FONT_H + 8);
}

void reliefos_ui_context_menu(struct reliefos_ui_surface *surface, uint32_t x, uint32_t y,
                            uint32_t w, const struct reliefos_ui_context_menu_item *items,
                            uint32_t count)
{
    reliefos_ui_context_menu_animated(surface, x, y, w, items, count, 1000);
}

void reliefos_ui_context_menu_animated(struct reliefos_ui_surface *surface, uint32_t x, uint32_t y,
                                     uint32_t w, const struct reliefos_ui_context_menu_item *items,
                                     uint32_t count, uint32_t progress)
{
    uint32_t row_h = RELIEFOS_FONT_H + 8;
    uint32_t h = reliefos_ui_context_menu_height(count);
    uint32_t visible_h;
    if (progress > 1000) {
        progress = 1000;
    }
    progress = (progress * (2000 - progress)) / 1000;
    visible_h = (h * progress + 999) / 1000;
    if (!visible_h) {
        return;
    }
    reliefos_ui_bevel(surface, x, y, w, visible_h, RELIEFOS_UI_GRAY, 0);
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t row_y = y + 4 + i * row_h;
        if (row_y + row_h > y + visible_h - 2) {
            break;
        }
        uint32_t flags = items ? items[i].flags : RELIEFOS_UI_MENU_DISABLED;
        const char *label = items ? items[i].label : "";
        if (flags & RELIEFOS_UI_MENU_SEPARATOR) {
            reliefos_ui_rect(surface, x + 4, row_y + row_h / 2, w > 8 ? w - 8 : w,
                           1, RELIEFOS_UI_DARK);
            reliefos_ui_rect(surface, x + 4, row_y + row_h / 2 + 1,
                           w > 8 ? w - 8 : w, 1, RELIEFOS_UI_WHITE);
            continue;
        }
        reliefos_ui_cursor_region(surface, (int32_t)x + 4, (int32_t)row_y,
                                w > 8 ? w - 8 : w, row_h,
                                (flags & RELIEFOS_UI_MENU_DISABLED)
                                    ? RELIEFOS_GUI_CURSOR_NO : RELIEFOS_GUI_CURSOR_HAND,
                                (flags & RELIEFOS_UI_MENU_DISABLED)
                                    ? RELIEFOS_GUI_CURSOR_REGION_DISABLED : 0);
        reliefos_ui_text_transparent_clipped(surface, x + 8, row_y + 4,
                                           w > 16 ? w - 16 : w,
                                           label ? label : "",
                                           (flags & RELIEFOS_UI_MENU_DISABLED)
                                               ? RELIEFOS_UI_DARK
                                               : RELIEFOS_UI_BLACK);
    }
}

int reliefos_ui_context_menu_hit(int32_t px, int32_t py, uint32_t x, uint32_t y,
                               uint32_t w, const struct reliefos_ui_context_menu_item *items,
                               uint32_t count, uint32_t *out_id)
{
    uint32_t row_h = RELIEFOS_FONT_H + 8;
    uint32_t h = reliefos_ui_context_menu_height(count);
    uint32_t index;
    if (out_id) {
        *out_id = 0;
    }
    if (!reliefos_ui_hit((uint32_t)px, (uint32_t)py, (int32_t)x, (int32_t)y, w, h)) {
        return 0;
    }
    if (py < (int32_t)y + 4) {
        return 1;
    }
    index = ((uint32_t)py - y - 4) / row_h;
    if (!items || index >= count ||
        (items[index].flags & (RELIEFOS_UI_MENU_SEPARATOR | RELIEFOS_UI_MENU_DISABLED))) {
        return 1;
    }
    if (out_id) {
        *out_id = items[index].id;
    }
    return 1;
}

void reliefos_ui_layout_begin(struct reliefos_ui_layout *layout, uint32_t x, uint32_t y,
                            uint32_t w, uint32_t h, uint32_t gap)
{
    if (!layout) {
        return;
    }
    layout->x = x;
    layout->y = y;
    layout->w = w;
    layout->h = h;
    layout->gap = gap;
    layout->cursor_x = x;
    layout->cursor_y = y;
    layout->row_h = 0;
}

struct reliefos_ui_rect reliefos_ui_layout_next(struct reliefos_ui_layout *layout,
                                            uint32_t preferred_w, uint32_t preferred_h)
{
    struct reliefos_ui_rect rect = {0, 0, 0, 0};
    uint32_t right;
    if (!layout || layout->w == 0 || layout->h == 0) {
        return rect;
    }
    if (preferred_h == 0) {
        preferred_h = RELIEFOS_UI_BUTTON_H;
    }
    if (preferred_w == 0 || preferred_w > layout->w) {
        preferred_w = layout->w;
    }
    right = layout->x + layout->w;
    if (layout->cursor_x != layout->x &&
        layout->cursor_x + preferred_w > right) {
        layout->cursor_x = layout->x;
        layout->cursor_y += layout->row_h + layout->gap;
        layout->row_h = 0;
    }
    if (layout->cursor_y >= layout->y + layout->h) {
        return rect;
    }
    if (layout->cursor_y + preferred_h > layout->y + layout->h) {
        preferred_h = layout->y + layout->h - layout->cursor_y;
    }
    rect.x = (int32_t)layout->cursor_x;
    rect.y = (int32_t)layout->cursor_y;
    rect.w = preferred_w;
    rect.h = preferred_h;
    layout->cursor_x += preferred_w + layout->gap;
    if (preferred_h > layout->row_h) {
        layout->row_h = preferred_h;
    }
    return rect;
}

uint32_t reliefos_ui_anim_progress(unsigned long now, unsigned long start,
                                 unsigned long duration_ms)
{
    unsigned long elapsed;
    if (duration_ms == 0) {
        return 1000;
    }
    if (now <= start) {
        return 0;
    }
    elapsed = now - start;
    if (elapsed >= duration_ms) {
        return 1000;
    }
    return (uint32_t)((elapsed * 1000UL) / duration_ms);
}

uint32_t reliefos_ui_anim_ease_out(uint32_t progress)
{
    if (progress > 1000) {
        progress = 1000;
    }
    return (progress * (2000 - progress)) / 1000;
}

uint32_t reliefos_ui_anim_lerp(uint32_t from, uint32_t to, uint32_t progress)
{
    if (progress > 1000) {
        progress = 1000;
    }
    if (to >= from) {
        return from + ((to - from) * progress) / 1000;
    }
    return from - ((from - to) * progress) / 1000;
}

void reliefos_ui_activity_bar(struct reliefos_ui_surface *surface, uint32_t x, uint32_t y,
                            uint32_t w, uint32_t h, uint32_t progress)
{
    uint32_t inner_w;
    uint32_t inner_h;
    uint32_t block_w;
    uint32_t range;
    uint32_t eased = reliefos_ui_anim_ease_out(progress % 1000);
    reliefos_ui_inset(surface, x, y, w, h, RELIEFOS_UI_WHITE);
    if (w <= 4 || h <= 4) {
        return;
    }
    inner_w = w - 4;
    inner_h = h - 4;
    block_w = inner_w / 4;
    if (block_w < 12) {
        block_w = inner_w < 12 ? inner_w : 12;
    }
    range = inner_w > block_w ? inner_w - block_w : 0;
    reliefos_ui_rect(surface, x + 2 + (range * eased) / 1000, y + 2,
                   block_w, inner_h, RELIEFOS_UI_ACTIVE_TITLE);
}

void reliefos_ui_tree(struct reliefos_ui_surface *surface, uint32_t x, uint32_t y,
                    uint32_t w, const struct reliefos_ui_tree_item *items,
                    uint32_t count, uint32_t row_h)
{
    if (row_h < RELIEFOS_FONT_H + 4) {
        row_h = RELIEFOS_FONT_H + 4;
    }
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t row_y = y + i * row_h;
        uint32_t indent = items ? items[i].depth * 14 : 0;
        uint32_t bg = (items && (items[i].flags & RELIEFOS_UI_TREE_SELECTED))
                          ? RELIEFOS_UI_ACTIVE_TITLE
                          : RELIEFOS_UI_WHITE;
        uint32_t fg = bg == RELIEFOS_UI_ACTIVE_TITLE ? RELIEFOS_UI_WHITE : RELIEFOS_UI_BLACK;
        uint32_t icon_x = x + 4 + indent;
        uint32_t text_x = x + 20 + indent;
        reliefos_ui_cursor_region(surface, (int32_t)x, (int32_t)row_y, w, row_h,
                                RELIEFOS_GUI_CURSOR_HAND, 0);
        reliefos_ui_rect(surface, x, row_y, w, row_h, bg);
        if (items && !(items[i].flags & RELIEFOS_UI_TREE_LEAF)) {
            reliefos_ui_button(surface, icon_x, row_y + 3, 12, 12,
                             (items[i].flags & RELIEFOS_UI_TREE_EXPANDED) ? "-" : "+", 0);
        } else {
            reliefos_ui_rect(surface, icon_x + 4, row_y + 8, 4, 4, fg);
        }
        if (text_x < x + w) {
            reliefos_ui_text_transparent_clipped(surface, text_x, row_y + 4,
                                               x + w - text_x,
                                               items ? items[i].label : "",
                                               fg);
        }
    }
}

int reliefos_ui_tree_hit(int32_t px, int32_t py, uint32_t x, uint32_t y,
                       uint32_t w, const struct reliefos_ui_tree_item *items,
                       uint32_t count, uint32_t row_h, uint32_t *out_id)
{
    uint32_t index;
    if (out_id) {
        *out_id = 0;
    }
    if (row_h < RELIEFOS_FONT_H + 4) {
        row_h = RELIEFOS_FONT_H + 4;
    }
    if (!reliefos_ui_hit((uint32_t)px, (uint32_t)py, (int32_t)x, (int32_t)y,
                       w, count * row_h)) {
        return 0;
    }
    index = ((uint32_t)py - y) / row_h;
    if (!items || index >= count) {
        return 1;
    }
    if (out_id) {
        *out_id = items[index].id;
    }
    return 1;
}

void reliefos_ui_panel(struct reliefos_ui_surface *surface, uint32_t x, uint32_t y,
                     uint32_t w, uint32_t h, uint32_t color)
{
    reliefos_ui_inset(surface, x, y, w, h, color);
}

void reliefos_ui_checkbox(struct reliefos_ui_surface *surface, uint32_t x, uint32_t y,
                        const char *label, int checked, uint32_t flags)
{
    uint32_t disabled = flags & RELIEFOS_UI_BUTTON_DISABLED;
    uint32_t label_w = reliefos_ui_text_width(label);
    reliefos_ui_cursor_region(surface, (int32_t)x, (int32_t)y,
                            22 + label_w, 18,
                            disabled ? RELIEFOS_GUI_CURSOR_NO : RELIEFOS_GUI_CURSOR_HAND,
                            disabled ? RELIEFOS_GUI_CURSOR_REGION_DISABLED : 0);
    uint32_t bg = disabled ? RELIEFOS_UI_LIGHT : RELIEFOS_UI_WHITE;
    uint32_t fg = disabled ? RELIEFOS_UI_DARK : RELIEFOS_UI_BLACK;
    reliefos_ui_inset(surface, x, y + 2, 14, 14, bg);
    if (checked) {
        reliefos_ui_rect(surface, x + 3, y + 8, 2, 2, fg);
        reliefos_ui_rect(surface, x + 5, y + 10, 2, 2, fg);
        reliefos_ui_rect(surface, x + 7, y + 8, 2, 2, fg);
        reliefos_ui_rect(surface, x + 9, y + 6, 2, 2, fg);
    }
    reliefos_ui_text_transparent(surface, x + 22, y, label, fg);
}

void reliefos_ui_progress(struct reliefos_ui_surface *surface, uint32_t x, uint32_t y,
                        uint32_t w, uint32_t h, uint32_t value, uint32_t max)
{
    reliefos_ui_inset(surface, x, y, w, h, RELIEFOS_UI_WHITE);
    if (max == 0) {
        return;
    }
    if (value > max) {
        value = max;
    }
    uint32_t fill_w = w > 4 ? ((w - 4) * value) / max : 0;
    reliefos_ui_rect(surface, x + 2, y + 2, fill_w, h > 4 ? h - 4 : 0, RELIEFOS_UI_ACTIVE_TITLE);
}

void reliefos_ui_text_field(struct reliefos_ui_surface *surface, uint32_t x, uint32_t y,
                          uint32_t w, const char *text, uint32_t flags)
{
    reliefos_ui_edit(surface, x, y, w, text, ui_strlen(text), 0, flags);
}

void reliefos_ui_list_header(struct reliefos_ui_surface *surface, uint32_t x, uint32_t y,
                           uint32_t w, const char *label)
{
    reliefos_ui_bevel(surface, x, y, w, RELIEFOS_FONT_H + 8, RELIEFOS_UI_GRAY, 0);
    reliefos_ui_text_transparent(surface, x + 6, y + 4, label, RELIEFOS_UI_BLACK);
}

void reliefos_ui_list_row(struct reliefos_ui_surface *surface, uint32_t x, uint32_t y,
                        uint32_t w, const char *text, uint32_t flags)
{
    reliefos_ui_cursor_region(surface, (int32_t)x, (int32_t)y, w, RELIEFOS_FONT_H + 4,
                            RELIEFOS_GUI_CURSOR_HAND, 0);
    uint32_t selected = flags & RELIEFOS_UI_MENU_SELECTED;
    uint32_t bg = selected ? RELIEFOS_UI_ACTIVE_TITLE : RELIEFOS_UI_WHITE;
    uint32_t fg = selected ? RELIEFOS_UI_WHITE : RELIEFOS_UI_BLACK;
    reliefos_ui_rect(surface, x, y, w, RELIEFOS_FONT_H + 4, bg);
    reliefos_ui_text_clipped(surface, x + 4, y + 2, w > 8 ? w - 8 : w, text, fg, bg);
}

static uint32_t scrollbar_thumb_pos(uint32_t track_len, uint32_t value, uint32_t max, uint32_t page,
                                    uint32_t *thumb_len)
{
    uint32_t len;
    uint32_t range;
    if (track_len < 8) {
        *thumb_len = track_len;
        return 0;
    }
    if (max <= page || max == 0) {
        *thumb_len = track_len;
        return 0;
    }
    len = (track_len * page) / max;
    if (len < 12) {
        len = 12;
    }
    if (len > track_len) {
        len = track_len;
    }
    range = track_len - len;
    if (value > max - page) {
        value = max - page;
    }
    *thumb_len = len;
    return range ? (range * value) / (max - page) : 0;
}

void reliefos_ui_vscrollbar(struct reliefos_ui_surface *surface, uint32_t x, uint32_t y,
                          uint32_t w, uint32_t h, uint32_t value, uint32_t max,
                          uint32_t page, uint32_t flags)
{
    uint32_t arrow_h = w < h / 2 ? w : h / 2;
    uint32_t track_y = y + arrow_h;
    uint32_t track_h = h > arrow_h * 2 ? h - arrow_h * 2 : 0;
    uint32_t thumb_h;
    uint32_t thumb_y;
    uint32_t disabled = flags & RELIEFOS_UI_SCROLLBAR_DISABLED;
    reliefos_ui_button(surface, x, y, w, arrow_h, "^", disabled ? RELIEFOS_UI_BUTTON_DISABLED : 0);
    reliefos_ui_button(surface, x, y + h - arrow_h, w, arrow_h, "v", disabled ? RELIEFOS_UI_BUTTON_DISABLED : 0);
    reliefos_ui_inset(surface, x, track_y, w, track_h, RELIEFOS_UI_LIGHT);
    if (disabled || track_h == 0) {
        return;
    }
    thumb_y = scrollbar_thumb_pos(track_h, value, max, page, &thumb_h);
    reliefos_ui_bevel(surface, x + 2, track_y + thumb_y, w > 4 ? w - 4 : w, thumb_h, RELIEFOS_UI_GRAY, 0);
}

void reliefos_ui_hscrollbar(struct reliefos_ui_surface *surface, uint32_t x, uint32_t y,
                          uint32_t w, uint32_t h, uint32_t value, uint32_t max,
                          uint32_t page, uint32_t flags)
{
    uint32_t arrow_w = h < w / 2 ? h : w / 2;
    uint32_t track_x = x + arrow_w;
    uint32_t track_w = w > arrow_w * 2 ? w - arrow_w * 2 : 0;
    uint32_t thumb_w;
    uint32_t thumb_x;
    uint32_t disabled = flags & RELIEFOS_UI_SCROLLBAR_DISABLED;
    reliefos_ui_button(surface, x, y, arrow_w, h, "<", disabled ? RELIEFOS_UI_BUTTON_DISABLED : 0);
    reliefos_ui_button(surface, x + w - arrow_w, y, arrow_w, h, ">", disabled ? RELIEFOS_UI_BUTTON_DISABLED : 0);
    reliefos_ui_inset(surface, track_x, y, track_w, h, RELIEFOS_UI_LIGHT);
    if (disabled || track_w == 0) {
        return;
    }
    thumb_x = scrollbar_thumb_pos(track_w, value, max, page, &thumb_w);
    reliefos_ui_bevel(surface, track_x + thumb_x, y + 2, thumb_w, h > 4 ? h - 4 : h, RELIEFOS_UI_GRAY, 0);
}

void reliefos_ui_scroll_view_frame(struct reliefos_ui_surface *surface, uint32_t x, uint32_t y,
                                 uint32_t w, uint32_t h)
{
    reliefos_ui_inset(surface, x, y, w, h, RELIEFOS_UI_WHITE);
}
/* Published libleonos.so.2 aliases; keep these in the defining translation unit. */
extern __typeof__(reliefos_ui_activity_bar) leonos_ui_activity_bar __attribute__((alias("reliefos_ui_activity_bar")));
extern __typeof__(reliefos_ui_anim_ease_out) leonos_ui_anim_ease_out __attribute__((alias("reliefos_ui_anim_ease_out")));
extern __typeof__(reliefos_ui_anim_lerp) leonos_ui_anim_lerp __attribute__((alias("reliefos_ui_anim_lerp")));
extern __typeof__(reliefos_ui_anim_progress) leonos_ui_anim_progress __attribute__((alias("reliefos_ui_anim_progress")));
extern __typeof__(reliefos_ui_bevel) leonos_ui_bevel __attribute__((alias("reliefos_ui_bevel")));
extern __typeof__(reliefos_ui_button) leonos_ui_button __attribute__((alias("reliefos_ui_button")));
extern __typeof__(reliefos_ui_checkbox) leonos_ui_checkbox __attribute__((alias("reliefos_ui_checkbox")));
extern __typeof__(reliefos_ui_context_menu) leonos_ui_context_menu __attribute__((alias("reliefos_ui_context_menu")));
extern __typeof__(reliefos_ui_context_menu_animated) leonos_ui_context_menu_animated __attribute__((alias("reliefos_ui_context_menu_animated")));
extern __typeof__(reliefos_ui_context_menu_height) leonos_ui_context_menu_height __attribute__((alias("reliefos_ui_context_menu_height")));
extern __typeof__(reliefos_ui_context_menu_hit) leonos_ui_context_menu_hit __attribute__((alias("reliefos_ui_context_menu_hit")));
extern __typeof__(reliefos_ui_hscrollbar) leonos_ui_hscrollbar __attribute__((alias("reliefos_ui_hscrollbar")));
extern __typeof__(reliefos_ui_inset) leonos_ui_inset __attribute__((alias("reliefos_ui_inset")));
extern __typeof__(reliefos_ui_layout_begin) leonos_ui_layout_begin __attribute__((alias("reliefos_ui_layout_begin")));
extern __typeof__(reliefos_ui_layout_next) leonos_ui_layout_next __attribute__((alias("reliefos_ui_layout_next")));
extern __typeof__(reliefos_ui_list_header) leonos_ui_list_header __attribute__((alias("reliefos_ui_list_header")));
extern __typeof__(reliefos_ui_list_row) leonos_ui_list_row __attribute__((alias("reliefos_ui_list_row")));
extern __typeof__(reliefos_ui_menu) leonos_ui_menu __attribute__((alias("reliefos_ui_menu")));
extern __typeof__(reliefos_ui_menu_item) leonos_ui_menu_item __attribute__((alias("reliefos_ui_menu_item")));
extern __typeof__(reliefos_ui_panel) leonos_ui_panel __attribute__((alias("reliefos_ui_panel")));
extern __typeof__(reliefos_ui_progress) leonos_ui_progress __attribute__((alias("reliefos_ui_progress")));
extern __typeof__(reliefos_ui_scroll_view_frame) leonos_ui_scroll_view_frame __attribute__((alias("reliefos_ui_scroll_view_frame")));
extern __typeof__(reliefos_ui_taskbar) leonos_ui_taskbar __attribute__((alias("reliefos_ui_taskbar")));
extern __typeof__(reliefos_ui_taskbar_button) leonos_ui_taskbar_button __attribute__((alias("reliefos_ui_taskbar_button")));
extern __typeof__(reliefos_ui_text_field) leonos_ui_text_field __attribute__((alias("reliefos_ui_text_field")));
extern __typeof__(reliefos_ui_tree) leonos_ui_tree __attribute__((alias("reliefos_ui_tree")));
extern __typeof__(reliefos_ui_tree_hit) leonos_ui_tree_hit __attribute__((alias("reliefos_ui_tree_hit")));
extern __typeof__(reliefos_ui_vscrollbar) leonos_ui_vscrollbar __attribute__((alias("reliefos_ui_vscrollbar")));
extern __typeof__(reliefos_ui_window) leonos_ui_window __attribute__((alias("reliefos_ui_window")));
extern __typeof__(reliefos_ui_window_button) leonos_ui_window_button __attribute__((alias("reliefos_ui_window_button")));
extern __typeof__(reliefos_ui_window_ex) leonos_ui_window_ex __attribute__((alias("reliefos_ui_window_ex")));
