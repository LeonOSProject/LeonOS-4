#include <assert.h>
#include <sys/stat.h>
#include <string.h>

#include "../../userland/apps/desktop/state.c"
#include "../../userland/apps/desktop/util.c"
static int asset_open(const char *path, int flags, ...);
#define open asset_open
#include "../../userland/apps/desktop/draw.c"
#undef open
#include "../../userland/apps/desktop/screen.c"

static uint32_t scanout[MAX_FB_W * MAX_FB_H];
static const uint32_t background = 0x00123456;

static const char *asset_path(const char *path)
{
    return !strcmp(path, CURSOR_BMP_PATH) ? "system/resources/mouse.bmp" : path;
}
static int asset_open(const char *path, int flags, ...)
{ return open(asset_path(path), flags); }

int reliefos_gui_mouse_visible(void) { return 1; }
int reliefos_fb_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                     uint32_t stride, const uint32_t *pixels)
{
    for (uint32_t row = 0; row < h; ++row)
        memcpy(scanout + (y + row) * MAX_FB_W + x,
               pixels + row * stride, w * sizeof(*pixels));
    return 0;
}
void reliefos_ui_pixel(struct reliefos_ui_surface *surface,
                       uint32_t x, uint32_t y, uint32_t color)
{
    if (x < surface->width && y < surface->height)
        surface->pixels[y * surface->stride + x] = color;
}
int reliefos_stat_legacy(const char *path, struct reliefos_stat *out)
{
    struct stat st;
    if (stat(asset_path(path), &st) < 0) return -1;
    out->type = S_ISREG(st.st_mode) ? RELIEFOS_FS_TYPE_FILE : RELIEFOS_FS_TYPE_DIR;
    out->size = st.st_size;
    return 0;
}

static void cursor_test(uint32_t style, int atlas, int large)
{
    for (unsigned i = 0; i < MAX_FB_W * MAX_FB_H; ++i)
        screen[i] = scanout[i] = background;
    ui.pixels = screen;
    ui.width = desktop_logical_w = MAX_FB_W;
    ui.height = desktop_logical_h = MAX_FB_H;
    ui.stride = MAX_FB_W;
    desktop_scale = 1;
    cursor_visible = 1;
    cursor_bitmap_loaded = atlas;
    desktop_cursor_style = style;
    cursor_x = 40;
    cursor_y = 40;
    struct rect old = cursor_rect_at(cursor_x, cursor_y);
    flush_region(large ? rect_make(0, 0, MAX_FB_W, MAX_FB_H) : old);
    unsigned drawn = 0;
    for (unsigned i = 0; i < MAX_FB_W * MAX_FB_H; ++i) {
        if (screen[i] != background) {
            fprintf(stderr, "cursor style=%u atlas=%d large=%d modified backing pixel=%u\n",
                    style, atlas, large, i);
            assert(screen[i] == background);
        }
        drawn += scanout[i] != background;
    }
    assert(drawn > 0);
    cursor_x = 120;
    cursor_y = 60;
    flush_region(rect_union(old, cursor_rect_at(cursor_x, cursor_y)));
    for (int y = old.y; y < old.y + old.h; ++y)
        for (int x = old.x; x < old.x + old.w; ++x)
            assert(scanout[y * MAX_FB_W + x] == background);
    /* Hotspots and clipping must also work at the screen corners. */
    cursor_x = cursor_y = 0;
    flush_region(rect_make(0, 0, 64, 64));
    cursor_x = MAX_FB_W - 1;
    cursor_y = MAX_FB_H - 1;
    flush_region(rect_make(MAX_FB_W - 64, MAX_FB_H - 64, 64, 64));
    for (unsigned i = 0; i < MAX_FB_W * MAX_FB_H; ++i)
        assert(screen[i] == background);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    if (!strcmp(argv[1], "cursor")) {
        for (uint32_t style = 0; style < CURSOR_STYLE_COUNT; ++style) {
            cursor_test(style, 0, 0);
            cursor_test(style, 0, 1);
        }
    } else if (!strcmp(argv[1], "alpha")) {
        uint32_t w, h;
        assert(load_bmp_argb("system/resources/mouse.bmp", CURSOR_MAX_W,
                            CURSOR_MAX_H, CURSOR_BMP_MAX_BYTES, cursor_pixels,
                            CURSOR_MAX_W, &w, &h));
        uint32_t original[CURSOR_MAX_W * CURSOR_MAX_H];
        memcpy(original, cursor_pixels, sizeof(original));
        assert(load_cursor_bmp());
        assert(!memcmp(original, cursor_pixels, sizeof(original)));
        cursor_test(RELIEFOS_GUI_CURSOR_ARROW, 1, 0);
        memset(cursor_pixels, 0, sizeof(cursor_pixels));
        cursor_pixels[0] = 0x80000000;
        cursor_x = cursor_y = 10;
        desktop_cursor_style = RELIEFOS_GUI_CURSOR_ARROW;
        flush_region(cursor_rect_at(10, 10));
        assert(scanout[10 * MAX_FB_W + 10] == 0x00091a2b);
        assert(screen[10 * MAX_FB_W + 10] == background);
    } else if (!strcmp(argv[1], "assets")) {
        uint32_t w, h;
        assert(load_bmp_argb("system/resources/mouse.bmp", CURSOR_MAX_W,
                            CURSOR_MAX_H, CURSOR_BMP_MAX_BYTES, cursor_pixels,
                            CURSOR_MAX_W, &w, &h));
        assert(w == CURSOR_TILE_W && h == CURSOR_MAX_H);
        for (uint32_t style = 0; style < CURSOR_STYLE_COUNT; ++style) {
            cursor_test(style, 1, 0);
            cursor_test(style, 1, 1);
        }
        assert(load_bmp_argb("system/resources/wallpaper-metro.bmp", WALLPAPER_MAX_W,
                            WALLPAPER_MAX_H, WALLPAPER_BMP_MAX_BYTES, wallpaper_pixels,
                            WALLPAPER_MAX_W, &w, &h));
        assert(w == 1280 && h == 720);
        struct app_icon_cache_entry *entry = &app_icon_cache[0];
        assert(load_bmp_argb("resources/build-art/app-icons/fileman.bmp", APP_ICON_MAX_W,
                            APP_ICON_MAX_H, APP_ICON_BMP_MAX_BYTES, entry->pixels,
                            APP_ICON_MAX_W, &w, &h));
        assert(w == 32 && h == 32);
    } else {
        assert(!"unknown scenario");
    }
    puts("PASS desktop rendering: assets, clean backing, cursor movement and clipping");
    return 0;
}
