#ifndef RELIEFOS_UI_H
#define RELIEFOS_UI_H

#include <stdint.h>

#define RELIEFOS_UI_THEME_WIN95 0u
#define RELIEFOS_UI_THEME_METRO 1u

#define RELIEFOS_UI_COLOR_SCHEME_BLUE 0u
#define RELIEFOS_UI_COLOR_SCHEME_TEAL 1u
#define RELIEFOS_UI_COLOR_SCHEME_GREEN 2u
#define RELIEFOS_UI_COLOR_SCHEME_PURPLE 3u
#define RELIEFOS_UI_COLOR_SCHEME_RED 4u
#define RELIEFOS_UI_COLOR_SCHEME_GRAPHITE 5u
#define RELIEFOS_UI_COLOR_SCHEME_PINK 6u
#define RELIEFOS_UI_COLOR_SCHEME_COUNT 7u

#define RELIEFOS_UI_COLOR_TEXT 0u
#define RELIEFOS_UI_COLOR_CONTENT 1u
#define RELIEFOS_UI_COLOR_SURFACE 2u
#define RELIEFOS_UI_COLOR_SUBTLE 3u
#define RELIEFOS_UI_COLOR_MUTED 4u
#define RELIEFOS_UI_COLOR_ACCENT 5u
#define RELIEFOS_UI_COLOR_TITLE_INACTIVE 6u
#define RELIEFOS_UI_COLOR_DESKTOP 7u
#define RELIEFOS_UI_COLOR_BORDER 8u
#define RELIEFOS_UI_COLOR_SELECTION 9u

#define RELIEFOS_UI_BLACK reliefos_ui_color(RELIEFOS_UI_COLOR_TEXT)
#define RELIEFOS_UI_WHITE reliefos_ui_color(RELIEFOS_UI_COLOR_CONTENT)
#define RELIEFOS_UI_GRAY reliefos_ui_color(RELIEFOS_UI_COLOR_SURFACE)
#define RELIEFOS_UI_LIGHT reliefos_ui_color(RELIEFOS_UI_COLOR_SUBTLE)
#define RELIEFOS_UI_DARK reliefos_ui_color(RELIEFOS_UI_COLOR_MUTED)
#define RELIEFOS_UI_ACTIVE_TITLE reliefos_ui_color(RELIEFOS_UI_COLOR_ACCENT)
#define RELIEFOS_UI_INACTIVE_TITLE reliefos_ui_color(RELIEFOS_UI_COLOR_TITLE_INACTIVE)
#define RELIEFOS_UI_DESKTOP reliefos_ui_color(RELIEFOS_UI_COLOR_DESKTOP)

#define RELIEFOS_UI_TITLEBAR_H 26u
#define RELIEFOS_UI_TASKBAR_H 34u
#define RELIEFOS_UI_BUTTON_H 24u
#define RELIEFOS_UI_WINDOW_BUTTON_W 18u
#define RELIEFOS_UI_WINDOW_BUTTON_H 20u

#define RELIEFOS_UI_BUTTON_PRESSED 0x01u
#define RELIEFOS_UI_BUTTON_ACTIVE 0x02u
#define RELIEFOS_UI_BUTTON_DISABLED 0x04u
#define RELIEFOS_UI_WINDOW_ACTIVE 0x01u
#define RELIEFOS_UI_WINDOW_NO_RESIZE 0x02u
#define RELIEFOS_UI_MENU_SEPARATOR 0x01u
#define RELIEFOS_UI_MENU_SELECTED 0x02u
#define RELIEFOS_UI_MENU_DISABLED 0x04u
#define RELIEFOS_UI_EDIT_FOCUSED 0x01u
#define RELIEFOS_UI_EDIT_READONLY 0x02u
#define RELIEFOS_UI_EDIT_DISABLED 0x04u
#define RELIEFOS_UI_EDIT_SECURE 0x08u
#define RELIEFOS_UI_SCROLLBAR_DISABLED 0x01u
#define RELIEFOS_UI_TAB_DISABLED 0x01u
#define RELIEFOS_UI_TAB_CLOSABLE 0x02u
#define RELIEFOS_UI_INPUT_DISABLED 0x01u
#define RELIEFOS_UI_TOOLBAR_BUTTON_ACTIVE RELIEFOS_UI_BUTTON_ACTIVE
#define RELIEFOS_UI_TOOLBAR_BUTTON_PRESSED RELIEFOS_UI_BUTTON_PRESSED
#define RELIEFOS_UI_TOOLBAR_BUTTON_DISABLED RELIEFOS_UI_BUTTON_DISABLED
#define RELIEFOS_UI_OPEN_WITH_SET_DEFAULT 0x01u
#define RELIEFOS_UI_TOAST_INFO 0u
#define RELIEFOS_UI_TOAST_SUCCESS 1u
#define RELIEFOS_UI_TOAST_WARNING 2u
#define RELIEFOS_UI_TOAST_ERROR 3u
#define RELIEFOS_UI_SPLIT_VERTICAL 1u
#define RELIEFOS_UI_SPLIT_HORIZONTAL 0u
#define RELIEFOS_UI_TREEVIEW_ITEM_DISABLED 0x01u
#define RELIEFOS_UI_TREEVIEW_MAX_ITEMS 128u
#define RELIEFOS_UI_FILE_DIALOG_INPUT_CHECKBOX 1u
#define RELIEFOS_UI_FILE_DIALOG_INPUT_DROPDOWN 2u
#define RELIEFOS_UI_FILE_DIALOG_MAX_INPUTS 4u
#define RELIEFOS_UI_CURSOR_REGION_MAX 64u
#define RELIEFOS_UI_CURSOR_MAGIC 0x4c554943UL

struct reliefos_ui_cursor_region_cache {
    uint32_t id;
    int32_t x;
    int32_t y;
    uint32_t width;
    uint32_t height;
    uint32_t style;
    uint32_t flags;
    uint8_t seen;
    uint8_t synced;
};

struct reliefos_ui_surface {
    uint32_t *pixels;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    uint32_t cursor_magic;
    uint32_t cursor_window_id;
    uint32_t cursor_region_count;
    uint32_t cursor_next_region_id;
    uint8_t cursor_tracking;
    uint8_t cursor_implicit;
    uint8_t clip_enabled;
    int32_t clip_x;
    int32_t clip_y;
    uint32_t clip_width;
    uint32_t clip_height;
    struct reliefos_ui_cursor_region_cache cursor_regions[RELIEFOS_UI_CURSOR_REGION_MAX];
};

struct reliefos_ui_rect {
    int32_t x;
    int32_t y;
    uint32_t w;
    uint32_t h;
};

struct reliefos_ui_window_parts {
    struct reliefos_ui_rect titlebar;
    struct reliefos_ui_rect body;
    struct reliefos_ui_rect minimize;
    struct reliefos_ui_rect maximize;
    struct reliefos_ui_rect close;
};

struct reliefos_ui_list_column {
    const char *label;
    uint32_t width;
};

struct reliefos_ui_edit_state {
    char *buffer;
    uint32_t capacity;
    uint32_t length;
    uint32_t cursor;
    uint32_t scroll;
    uint32_t selection_anchor;
    uint8_t focused;
    uint8_t readonly;
    uint8_t selecting;
};

struct reliefos_ui_text_area_state {
    char *buffer;
    uint32_t capacity;
    uint32_t length;
    uint32_t cursor;
    uint32_t selection_anchor;
    uint32_t scroll_line;
    uint32_t preferred_column;
    uint32_t line_count;
    uint8_t focused;
    uint8_t readonly;
    uint8_t selecting;
};

uint32_t reliefos_ui_theme(void);
int reliefos_ui_theme_set(uint32_t theme);
uint32_t reliefos_ui_theme_color_scheme(uint32_t theme);
uint32_t reliefos_ui_theme_active_color_scheme(void);
uint32_t reliefos_ui_theme_scheme_accent(uint32_t theme, uint32_t scheme);
int reliefos_ui_theme_set_color_scheme(uint32_t theme, uint32_t scheme);
int reliefos_ui_theme_set_appearance(uint32_t theme,
                                   uint32_t metro_color_scheme,
                                   uint32_t win95_color_scheme);
void reliefos_ui_theme_load_system(void);
uint32_t reliefos_ui_color(uint32_t role);

struct reliefos_ui_listview_state {
    uint32_t row_count;
    uint32_t visible_rows;
    uint32_t row_height;
    uint32_t scroll;
    int32_t selected;
    uint8_t focused;
};

struct reliefos_ui_treeview_item {
    uint32_t id;
    uint32_t parent_id;
    const char *const *cells;
    uint32_t flags;
};

struct reliefos_ui_treeview_state {
    uint32_t visible_rows;
    uint32_t row_height;
    uint32_t scroll;
    uint32_t selected_id;
    uint32_t visible_count;
    uint32_t collapsed_count;
    uint32_t visible_indices[RELIEFOS_UI_TREEVIEW_MAX_ITEMS];
    uint32_t collapsed_ids[RELIEFOS_UI_TREEVIEW_MAX_ITEMS];
    uint8_t visible_depths[RELIEFOS_UI_TREEVIEW_MAX_ITEMS];
    uint8_t focused;
    uint8_t has_selection;
};

struct reliefos_ui_context_menu_item {
    const char *label;
    uint32_t id;
    uint32_t flags;
};

struct reliefos_ui_menubar_item {
    const char *label;
    uint32_t id;
    uint32_t width;
    uint32_t flags;
};

struct reliefos_ui_dropdown_item {
    const char *label;
    uint32_t id;
    uint32_t flags;
};

struct reliefos_ui_file_dialog_input {
    uint32_t type;
    uint32_t id;
    const char *label;
    uint32_t flags;
    uint32_t *value;
    const struct reliefos_ui_dropdown_item *items;
    uint32_t item_count;
};

struct reliefos_ui_file_dialog_options {
    const struct reliefos_ui_file_dialog_input *inputs;
    uint32_t input_count;
};

struct reliefos_ui_tab_item {
    const char *label;
    uint32_t id;
    uint32_t flags;
};

struct reliefos_ui_tab_state {
    uint32_t selected_id;
    uint32_t hovered_id;
    uint8_t focused;
};

struct reliefos_ui_color_input_state {
    uint32_t color;
    uint8_t open;
    uint8_t focused;
    uint8_t channel;
};

struct reliefos_ui_date_input_state {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t open;
    uint8_t focused;
    uint8_t part;
};

struct reliefos_ui_layout {
    uint32_t x;
    uint32_t y;
    uint32_t w;
    uint32_t h;
    uint32_t gap;
    uint32_t cursor_x;
    uint32_t cursor_y;
    uint32_t row_h;
};

struct reliefos_ui_tree_item {
    const char *label;
    uint32_t id;
    uint32_t depth;
    uint32_t flags;
};

struct reliefos_ui_property_item {
    const char *label;
    const char *value;
    uint32_t flags;
};

struct reliefos_ui_split_pane_state {
    uint32_t vertical;
    uint32_t split;
    uint32_t min_first;
    uint32_t min_second;
    uint32_t splitter_size;
    uint8_t dragging;
    struct reliefos_ui_rect first;
    struct reliefos_ui_rect splitter;
    struct reliefos_ui_rect second;
};

struct reliefos_ui_toast_state {
    char message[160];
    unsigned long start_ms;
    uint32_t duration_ms;
    uint32_t kind;
    uint8_t active;
};

#define RELIEFOS_UI_TREE_EXPANDED 0x01u
#define RELIEFOS_UI_TREE_SELECTED 0x02u
#define RELIEFOS_UI_TREE_LEAF 0x04u

void reliefos_ui_bind(struct reliefos_ui_surface *surface, uint32_t *pixels,
                    uint32_t width, uint32_t height, uint32_t stride);
void reliefos_ui_set_clip(struct reliefos_ui_surface *surface, int32_t x, int32_t y,
                        uint32_t width, uint32_t height);
void reliefos_ui_clear_clip(struct reliefos_ui_surface *surface);
void reliefos_ui_cursor_begin(struct reliefos_ui_surface *surface, uint32_t window_id);
void reliefos_ui_cursor_end(struct reliefos_ui_surface *surface);
int reliefos_ui_cursor_region(struct reliefos_ui_surface *surface,
                            int32_t x, int32_t y, uint32_t width, uint32_t height,
                            uint32_t style, uint32_t flags);
void reliefos_ui_cursor_clear(struct reliefos_ui_surface *surface);
void reliefos_ui_present_for_pixels(const uint32_t *pixels, uint32_t window_id);
int reliefos_ui_set_font_path(const char *path);
int reliefos_ui_set_font_fallback_path(const char *path);
uint32_t reliefos_ui_text_width(const char *text);
uint32_t reliefos_ui_text_fit_chars(uint32_t pixel_width);
int reliefos_ui_hit(uint32_t px, uint32_t py, int32_t x, int32_t y, uint32_t w, uint32_t h);
int reliefos_ui_keycode_to_char(uint8_t keycode, char *out);
int reliefos_ui_keycode_to_char_shift(uint8_t keycode, uint8_t shifted, char *out);
/* Absolute state from the input event, applied by libwind before dispatch. */
void reliefos_ui_set_keyboard_modifiers(uint8_t modifiers);
uint8_t reliefos_ui_keyboard_modifiers(void);
void reliefos_ui_pixel(struct reliefos_ui_surface *surface, uint32_t x, uint32_t y, uint32_t color);
void reliefos_ui_rect(struct reliefos_ui_surface *surface, uint32_t x, uint32_t y,
                    uint32_t w, uint32_t h, uint32_t color);
void reliefos_ui_codepoint(struct reliefos_ui_surface *surface, uint32_t x, uint32_t y,
                         uint32_t codepoint, uint32_t cell_width,
                         uint32_t fg, uint32_t bg);
void reliefos_ui_text(struct reliefos_ui_surface *surface, uint32_t x, uint32_t y,
                    const char *text, uint32_t fg, uint32_t bg);
void reliefos_ui_text_clipped(struct reliefos_ui_surface *surface, uint32_t x, uint32_t y,
                            uint32_t w, const char *text, uint32_t fg, uint32_t bg);
void reliefos_ui_text_resized_clipped(struct reliefos_ui_surface *surface,
                                    uint32_t x, uint32_t y, uint32_t w,
                                    const char *text, uint32_t fg, uint32_t bg,
                                    uint32_t cell_w, uint32_t cell_h);
void reliefos_ui_text_transparent(struct reliefos_ui_surface *surface, uint32_t x, uint32_t y,
                                const char *text, uint32_t fg);
void reliefos_ui_text_transparent_clipped(struct reliefos_ui_surface *surface, uint32_t x, uint32_t y,
                                        uint32_t w, const char *text, uint32_t fg);
void reliefos_ui_bevel(struct reliefos_ui_surface *surface, uint32_t x, uint32_t y,
                     uint32_t w, uint32_t h, uint32_t fill, uint32_t flags);
void reliefos_ui_inset(struct reliefos_ui_surface *surface, uint32_t x, uint32_t y,
                     uint32_t w, uint32_t h, uint32_t fill);
void reliefos_ui_button(struct reliefos_ui_surface *surface, uint32_t x, uint32_t y,
                      uint32_t w, uint32_t h, const char *label, uint32_t flags);
void reliefos_ui_window_button(struct reliefos_ui_surface *surface, uint32_t x, uint32_t y,
                             char label, uint32_t flags);
void reliefos_ui_window(struct reliefos_ui_surface *surface, uint32_t x, uint32_t y,
                      uint32_t w, uint32_t h, const char *title, uint32_t flags,
                      struct reliefos_ui_window_parts *parts);
void reliefos_ui_window_ex(struct reliefos_ui_surface *surface, uint32_t x, uint32_t y,
                         uint32_t w, uint32_t h, const char *title, char maximize_label,
                         uint32_t flags, struct reliefos_ui_window_parts *parts);
void reliefos_ui_taskbar(struct reliefos_ui_surface *surface, uint32_t y, uint32_t h);
void reliefos_ui_taskbar_button(struct reliefos_ui_surface *surface, uint32_t x, uint32_t y,
                              uint32_t w, const char *label, uint32_t flags);
void reliefos_ui_menu(struct reliefos_ui_surface *surface, uint32_t x, uint32_t y,
                    uint32_t w, uint32_t h);
void reliefos_ui_menu_item(struct reliefos_ui_surface *surface, uint32_t x, uint32_t y,
                         uint32_t w, const char *label, uint32_t flags);
uint32_t reliefos_ui_context_menu_height(uint32_t count);
void reliefos_ui_context_menu(struct reliefos_ui_surface *surface, uint32_t x, uint32_t y,
                            uint32_t w, const struct reliefos_ui_context_menu_item *items,
                            uint32_t count);
void reliefos_ui_context_menu_animated(struct reliefos_ui_surface *surface, uint32_t x, uint32_t y,
                                     uint32_t w, const struct reliefos_ui_context_menu_item *items,
                                     uint32_t count, uint32_t progress);
int reliefos_ui_context_menu_hit(int32_t px, int32_t py, uint32_t x, uint32_t y,
                               uint32_t w, const struct reliefos_ui_context_menu_item *items,
                               uint32_t count, uint32_t *out_id);
void reliefos_ui_layout_begin(struct reliefos_ui_layout *layout, uint32_t x, uint32_t y,
                            uint32_t w, uint32_t h, uint32_t gap);
struct reliefos_ui_rect reliefos_ui_layout_next(struct reliefos_ui_layout *layout,
                                            uint32_t preferred_w, uint32_t preferred_h);
uint32_t reliefos_ui_anim_progress(unsigned long now, unsigned long start,
                                 unsigned long duration_ms);
uint32_t reliefos_ui_anim_ease_out(uint32_t progress);
uint32_t reliefos_ui_anim_lerp(uint32_t from, uint32_t to, uint32_t progress);
void reliefos_ui_activity_bar(struct reliefos_ui_surface *surface, uint32_t x, uint32_t y,
                            uint32_t w, uint32_t h, uint32_t progress);
void reliefos_ui_tree(struct reliefos_ui_surface *surface, uint32_t x, uint32_t y,
                    uint32_t w, const struct reliefos_ui_tree_item *items,
                    uint32_t count, uint32_t row_h);
int reliefos_ui_tree_hit(int32_t px, int32_t py, uint32_t x, uint32_t y,
                       uint32_t w, const struct reliefos_ui_tree_item *items,
                       uint32_t count, uint32_t row_h, uint32_t *out_id);
void reliefos_ui_panel(struct reliefos_ui_surface *surface, uint32_t x, uint32_t y,
                     uint32_t w, uint32_t h, uint32_t color);
void reliefos_ui_checkbox(struct reliefos_ui_surface *surface, uint32_t x, uint32_t y,
                        const char *label, int checked, uint32_t flags);
void reliefos_ui_progress(struct reliefos_ui_surface *surface, uint32_t x, uint32_t y,
                        uint32_t w, uint32_t h, uint32_t value, uint32_t max);
void reliefos_ui_text_field(struct reliefos_ui_surface *surface, uint32_t x, uint32_t y,
                          uint32_t w, const char *text, uint32_t flags);
void reliefos_ui_list_header(struct reliefos_ui_surface *surface, uint32_t x, uint32_t y,
                           uint32_t w, const char *label);
void reliefos_ui_list_row(struct reliefos_ui_surface *surface, uint32_t x, uint32_t y,
                        uint32_t w, const char *text, uint32_t flags);
void reliefos_ui_vscrollbar(struct reliefos_ui_surface *surface, uint32_t x, uint32_t y,
                          uint32_t w, uint32_t h, uint32_t value, uint32_t max,
                          uint32_t page, uint32_t flags);
void reliefos_ui_hscrollbar(struct reliefos_ui_surface *surface, uint32_t x, uint32_t y,
                          uint32_t w, uint32_t h, uint32_t value, uint32_t max,
                          uint32_t page, uint32_t flags);
void reliefos_ui_scroll_view_frame(struct reliefos_ui_surface *surface, uint32_t x, uint32_t y,
                                 uint32_t w, uint32_t h);
void reliefos_ui_edit(struct reliefos_ui_surface *surface, uint32_t x, uint32_t y,
                    uint32_t w, const char *text, uint32_t cursor, uint32_t scroll,
                    uint32_t flags);
void reliefos_ui_edit_state_init(struct reliefos_ui_edit_state *state, char *buffer,
                               uint32_t capacity);
void reliefos_ui_edit_state_sync(struct reliefos_ui_edit_state *state);
void reliefos_ui_edit_state_draw(struct reliefos_ui_surface *surface, uint32_t x,
                               uint32_t y, uint32_t w,
                               struct reliefos_ui_edit_state *state,
                               uint32_t flags);
int reliefos_ui_edit_state_handle_key(struct reliefos_ui_edit_state *state,
                                    uint8_t keycode, uint8_t pressed);
int reliefos_ui_edit_state_handle_mouse(struct reliefos_ui_edit_state *state,
                                      int32_t px, int32_t py, uint32_t x,
                                      uint32_t y, uint32_t w, uint32_t buttons);
void reliefos_ui_text_area(struct reliefos_ui_surface *surface, uint32_t x, uint32_t y,
                         uint32_t w, uint32_t h, const char *text, uint32_t cursor,
                         uint32_t scroll_line, uint32_t flags);
void reliefos_ui_text_area_state_init(struct reliefos_ui_text_area_state *state,
                                    char *buffer, uint32_t capacity);
void reliefos_ui_text_area_state_sync(struct reliefos_ui_text_area_state *state,
                                    uint32_t w);
void reliefos_ui_text_area_state_draw(struct reliefos_ui_surface *surface, uint32_t x,
                                    uint32_t y, uint32_t w, uint32_t h,
                                    struct reliefos_ui_text_area_state *state,
                                    uint32_t flags);
int reliefos_ui_text_area_state_handle_key(struct reliefos_ui_text_area_state *state,
                                         uint8_t keycode, uint8_t pressed,
                                         uint32_t w, uint32_t h);
int reliefos_ui_text_area_state_handle_mouse(struct reliefos_ui_text_area_state *state,
                                           int32_t px, int32_t py, uint32_t x,
                                           uint32_t y, uint32_t w, uint32_t h,
                                           uint32_t buttons);
uint32_t reliefos_ui_text_area_line_count(struct reliefos_ui_text_area_state *state,
                                        uint32_t w);
void reliefos_ui_listview_header(struct reliefos_ui_surface *surface, uint32_t x, uint32_t y,
                               uint32_t w, const struct reliefos_ui_list_column *cols,
                               uint32_t count);
void reliefos_ui_listview_row(struct reliefos_ui_surface *surface, uint32_t x, uint32_t y,
                            uint32_t w, const struct reliefos_ui_list_column *cols,
                            const char *const cells[], uint32_t count, uint32_t flags);
void reliefos_ui_listview_state_init(struct reliefos_ui_listview_state *state,
                                   uint32_t visible_rows, uint32_t row_height);
void reliefos_ui_listview_state_set_count(struct reliefos_ui_listview_state *state,
                                        uint32_t row_count);
int reliefos_ui_listview_state_handle_key(struct reliefos_ui_listview_state *state,
                                        uint8_t keycode, uint32_t *activated);
int reliefos_ui_listview_state_handle_mouse(struct reliefos_ui_listview_state *state,
                                          int32_t px, int32_t py, uint32_t x,
                                          uint32_t rows_y, uint32_t w,
                                          uint32_t *activated);
int reliefos_ui_listview_state_handle_wheel(struct reliefos_ui_listview_state *state,
                                          int32_t wheel_delta);
void reliefos_ui_treeview_state_init(struct reliefos_ui_treeview_state *state,
                                   uint32_t visible_rows, uint32_t row_height);
void reliefos_ui_treeview_state_set_viewport(struct reliefos_ui_treeview_state *state,
                                           uint32_t visible_rows);
void reliefos_ui_treeview_state_sync(struct reliefos_ui_treeview_state *state,
                                   const struct reliefos_ui_treeview_item *items,
                                   uint32_t count);
void reliefos_ui_treeview(struct reliefos_ui_surface *surface, uint32_t x, uint32_t y,
                        uint32_t w, const struct reliefos_ui_list_column *cols,
                        uint32_t col_count,
                        const struct reliefos_ui_treeview_item *items,
                        uint32_t count, struct reliefos_ui_treeview_state *state);
int reliefos_ui_treeview_state_handle_key(struct reliefos_ui_treeview_state *state,
                                        const struct reliefos_ui_treeview_item *items,
                                        uint32_t count, uint8_t keycode,
                                        uint32_t *activated);
int reliefos_ui_treeview_state_handle_mouse(struct reliefos_ui_treeview_state *state,
                                          const struct reliefos_ui_treeview_item *items,
                                          uint32_t count, int32_t px, int32_t py,
                                          uint32_t x, uint32_t rows_y, uint32_t w,
                                          uint32_t *activated);
int reliefos_ui_treeview_state_handle_wheel(struct reliefos_ui_treeview_state *state,
                                          int32_t wheel_delta);
int reliefos_ui_vscrollbar_handle_mouse(uint32_t *value, uint32_t max, uint32_t page,
                                      uint32_t x, uint32_t y, uint32_t w,
                                      uint32_t h, int32_t px, int32_t py);
int reliefos_ui_vscrollbar_handle_wheel(uint32_t *value, uint32_t max, uint32_t page,
                                      int32_t wheel_delta);
int reliefos_ui_hscrollbar_handle_mouse(uint32_t *value, uint32_t max, uint32_t page,
                                      uint32_t x, uint32_t y, uint32_t w,
                                      uint32_t h, int32_t px, int32_t py);
void reliefos_ui_dialog(struct reliefos_ui_surface *surface, uint32_t x, uint32_t y,
                      uint32_t w, uint32_t h, const char *title);
void reliefos_ui_message_box(struct reliefos_ui_surface *surface, uint32_t x, uint32_t y,
                           uint32_t w, uint32_t h, const char *title,
                           const char *message, const char *button);
void reliefos_ui_confirm_dialog(struct reliefos_ui_surface *surface, uint32_t x, uint32_t y,
                              uint32_t w, uint32_t h, const char *title,
                              const char *message, uint32_t default_yes);
void reliefos_ui_input_dialog(struct reliefos_ui_surface *surface, uint32_t x, uint32_t y,
                            uint32_t w, uint32_t h, const char *title,
                            const char *label, const char *value, uint32_t flags);
int reliefos_ui_show_message_box(const char *title, const char *message,
                               const char *button);
int reliefos_ui_show_confirm_dialog(const char *title, const char *message,
                                  uint32_t default_yes);
int reliefos_ui_show_input_dialog(const char *title, const char *label,
                                char *value, uint32_t capacity);
int reliefos_ui_show_password_dialog(const char *title, const char *label,
                                   char *value, uint32_t capacity);
int reliefos_ui_show_open_dialog(const char *title, char *path, uint32_t capacity,
                               const char *filter_label, const char *filter_ext);
int reliefos_ui_show_open_dialog_with_options(const char *title, char *path,
                                            uint32_t capacity,
                                            const char *filter_label,
                                            const char *filter_ext,
                                            const struct reliefos_ui_file_dialog_options *options);
int reliefos_ui_show_open_with_dialog(const char *title, const char *path,
                                    char *program_path, uint32_t capacity,
                                    uint32_t *remember, uint32_t flags);
int reliefos_ui_show_save_dialog_ex(const char *title, char *value, uint32_t capacity,
                                  const char *filter_label, const char *filter_ext);
int reliefos_ui_show_save_dialog_with_options(const char *title, char *value,
                                            uint32_t capacity,
                                            const char *filter_label,
                                            const char *filter_ext,
                                            const struct reliefos_ui_file_dialog_options *options);
int reliefos_ui_show_save_dialog(const char *title, char *value, uint32_t capacity);
void reliefos_ui_combobox(struct reliefos_ui_surface *surface, uint32_t x, uint32_t y,
                        uint32_t w, const char *text, uint32_t open, uint32_t flags);
uint32_t reliefos_ui_dropdown_height(uint32_t count, uint32_t row_h,
                                   uint32_t progress);
void reliefos_ui_dropdown(struct reliefos_ui_surface *surface, uint32_t x, uint32_t y,
                        uint32_t w, const struct reliefos_ui_dropdown_item *items,
                        uint32_t count, uint32_t selected_id, uint32_t row_h,
                        uint32_t progress);
int reliefos_ui_dropdown_hit(int32_t px, int32_t py, uint32_t x, uint32_t y,
                           uint32_t w, const struct reliefos_ui_dropdown_item *items,
                           uint32_t count, uint32_t row_h, uint32_t progress,
                           uint32_t *out_id);
void reliefos_ui_radio(struct reliefos_ui_surface *surface, uint32_t x, uint32_t y,
                     const char *label, int checked, uint32_t flags);
void reliefos_ui_groupbox(struct reliefos_ui_surface *surface, uint32_t x, uint32_t y,
                        uint32_t w, uint32_t h, const char *title);
void reliefos_ui_tab_state_init(struct reliefos_ui_tab_state *state, uint32_t selected_id);
uint32_t reliefos_ui_tab_height(void);
void reliefos_ui_tab_control(struct reliefos_ui_surface *surface, uint32_t x, uint32_t y,
                           uint32_t w, const struct reliefos_ui_tab_item *items,
                           uint32_t count, const struct reliefos_ui_tab_state *state);
int reliefos_ui_tab_control_handle_mouse(struct reliefos_ui_tab_state *state,
                                       int32_t px, int32_t py,
                                       uint32_t x, uint32_t y, uint32_t w,
                                       const struct reliefos_ui_tab_item *items,
                                       uint32_t count);
int reliefos_ui_tab_control_handle_mouse_ex(struct reliefos_ui_tab_state *state,
                                           int32_t px, int32_t py,
                                           uint32_t x, uint32_t y, uint32_t w,
                                           const struct reliefos_ui_tab_item *items,
                                           uint32_t count, uint32_t *closed_id);
int reliefos_ui_tab_control_handle_key(struct reliefos_ui_tab_state *state,
                                     uint8_t keycode,
                                     const struct reliefos_ui_tab_item *items,
                                     uint32_t count);
void reliefos_ui_tab_body(struct reliefos_ui_surface *surface, uint32_t x, uint32_t y,
                        uint32_t w, uint32_t h);
void reliefos_ui_color_input_state_init(struct reliefos_ui_color_input_state *state,
                                      uint32_t color);
void reliefos_ui_color_input(struct reliefos_ui_surface *surface, uint32_t x, uint32_t y,
                           uint32_t w, const struct reliefos_ui_color_input_state *state,
                           uint32_t flags);
int reliefos_ui_color_input_handle_mouse(struct reliefos_ui_color_input_state *state,
                                       int32_t px, int32_t py,
                                       uint32_t x, uint32_t y, uint32_t w,
                                       uint32_t flags);
int reliefos_ui_color_input_handle_key(struct reliefos_ui_color_input_state *state,
                                     uint8_t keycode, uint32_t flags);
void reliefos_ui_date_input_state_init(struct reliefos_ui_date_input_state *state,
                                     uint16_t year, uint8_t month, uint8_t day);
void reliefos_ui_date_input(struct reliefos_ui_surface *surface, uint32_t x, uint32_t y,
                          uint32_t w, const struct reliefos_ui_date_input_state *state,
                          uint32_t flags);
int reliefos_ui_date_input_handle_mouse(struct reliefos_ui_date_input_state *state,
                                      int32_t px, int32_t py,
                                      uint32_t x, uint32_t y, uint32_t w,
                                      uint32_t flags);
int reliefos_ui_date_input_handle_key(struct reliefos_ui_date_input_state *state,
                                    uint8_t keycode, uint32_t flags);
void reliefos_ui_statusbar(struct reliefos_ui_surface *surface, uint32_t y, uint32_t h,
                         const char *text);
void reliefos_ui_toolbar(struct reliefos_ui_surface *surface, uint32_t x, uint32_t y,
                       uint32_t w, uint32_t h);
void reliefos_ui_toolbar_button(struct reliefos_ui_surface *surface, uint32_t x, uint32_t y,
                              uint32_t w, const char *label, uint32_t flags);
void reliefos_ui_splitter(struct reliefos_ui_surface *surface, uint32_t x, uint32_t y,
                        uint32_t w, uint32_t h, uint32_t vertical);
void reliefos_ui_menubar(struct reliefos_ui_surface *surface, uint32_t x, uint32_t y,
                       uint32_t w);
void reliefos_ui_menubar_item(struct reliefos_ui_surface *surface, uint32_t x, uint32_t y,
                            uint32_t w, const char *label, uint32_t active);
void reliefos_ui_menubar_draw(struct reliefos_ui_surface *surface, uint32_t x, uint32_t y,
                            uint32_t w, const struct reliefos_ui_menubar_item *items,
                            uint32_t count, uint32_t active_id);
int reliefos_ui_menubar_hit(int32_t px, int32_t py, uint32_t x, uint32_t y,
                          const struct reliefos_ui_menubar_item *items,
                          uint32_t count, uint32_t *out_id);
int reliefos_ui_menubar_item_rect(uint32_t x, uint32_t y,
                                const struct reliefos_ui_menubar_item *items,
                                uint32_t count, uint32_t id,
                                struct reliefos_ui_rect *out_rect);
uint32_t reliefos_ui_menu_popup_height(uint32_t count);
void reliefos_ui_menu_popup(struct reliefos_ui_surface *surface, uint32_t x, uint32_t y,
                          uint32_t w, const struct reliefos_ui_context_menu_item *items,
                          uint32_t count, uint32_t selected_id);
int reliefos_ui_menu_popup_hit(int32_t px, int32_t py, uint32_t x, uint32_t y,
                             uint32_t w, const struct reliefos_ui_context_menu_item *items,
                             uint32_t count, uint32_t *out_id);
void reliefos_ui_property_grid(struct reliefos_ui_surface *surface, uint32_t x, uint32_t y,
                             uint32_t w, const struct reliefos_ui_property_item *items,
                             uint32_t count, uint32_t label_w, uint32_t row_h);
void reliefos_ui_split_pane_init(struct reliefos_ui_split_pane_state *state,
                               uint32_t vertical, uint32_t split,
                               uint32_t min_first, uint32_t min_second);
void reliefos_ui_split_pane_layout(struct reliefos_ui_split_pane_state *state,
                                 uint32_t x, uint32_t y, uint32_t w, uint32_t h);
void reliefos_ui_split_pane_draw(struct reliefos_ui_surface *surface,
                               const struct reliefos_ui_split_pane_state *state);
int reliefos_ui_split_pane_handle_mouse(struct reliefos_ui_split_pane_state *state,
                                      int32_t px, int32_t py, uint32_t buttons);
void reliefos_ui_slider(struct reliefos_ui_surface *surface, uint32_t x, uint32_t y,
                      uint32_t w, uint32_t h, uint32_t value, uint32_t max,
                      uint32_t flags);
int reliefos_ui_slider_handle_mouse(uint32_t *value, uint32_t max,
                                  uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                                  int32_t px, int32_t py);
void reliefos_ui_stepper(struct reliefos_ui_surface *surface, uint32_t x, uint32_t y,
                       uint32_t w, uint32_t h, int32_t value, int32_t min,
                       int32_t max, uint32_t flags);
int reliefos_ui_stepper_handle_mouse(int32_t *value, int32_t min, int32_t max,
                                   int32_t step, uint32_t x, uint32_t y,
                                   uint32_t w, uint32_t h, int32_t px, int32_t py);
void reliefos_ui_toast_show(struct reliefos_ui_toast_state *state, const char *message,
                          unsigned long now, uint32_t duration_ms, uint32_t kind);
void reliefos_ui_toast_clear(struct reliefos_ui_toast_state *state);
int reliefos_ui_toast_active(struct reliefos_ui_toast_state *state, unsigned long now);
void reliefos_ui_toast_draw(struct reliefos_ui_surface *surface,
                          struct reliefos_ui_toast_state *state,
                          unsigned long now);

#endif
