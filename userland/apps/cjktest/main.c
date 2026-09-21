#include <leonos/fs.h>
#include <leonos/gui.h>
#include <leonos/stdio.h>
#include <leonos/syscall.h>
#include <leonos/ui.h>

#define CJKTEST_W 720
#define CJKTEST_H 430

static uint32_t pixels[CJKTEST_W * CJKTEST_H];
static char status_line[160] = "准备测试中文显示";
static char text_buffer[512] =
    "中文显示测试\n"
    "你好，LeonOS 4。\n"
    "中文标点：，。！？；：《》【】（）\n"
    "宽度测试：ASCII=1 cell，中文=2 cells。\n"
    "文件名测试会创建 /测试目录/你好.txt\n";
static struct leonos_ui_text_area_state text_state;

static void tty_write_text(const char *text)
{
    if (text) {
        (void)write(1, text, strlen(text));
    }
}

static void tty_write_chunked(const char *text)
{
    size_t length;
    size_t split;

    if (!text) {
        return;
    }
    length = strlen(text);
    split = length > 5 ? 5 : length;
    (void)write(1, text, split);
    sleep_ms(20);
    (void)write(1, text + split, length - split);
}

static int run_tty_test(void)
{
    tty_write_text("\033[2J\033[H");
    tty_write_text("LeonOS CJK PTY/TTY test\r\n");
    tty_write_text("1. UTF-8 分段写入: ");
    tty_write_chunked("中文 / 한국어 / 日本語 / 繁體中文");
    tty_write_text("\r\n");
    tty_write_text("2. 双宽对齐: |ASCII    |\r\n");
    tty_write_text("               |中文中文|\r\n");
    tty_write_text("               |한글日本|\r\n");
    tty_write_text("3. 标点与符号: ，。！？；：《》【】（）「」￥€\r\n");
    tty_write_text("4. 光标擦除: 保留这一行，下一行将被擦除\r\n");
    tty_write_text("   临时中文行: 临时文字\033[2K\r");
    tty_write_text("   已擦除并重写: 擦除成功\r\n");
    tty_write_text("5. 退格场景: 中\b\b文  (观察双宽字符退格)\r\n");
    tty_write_text("6. UTF-8 完整性: ");
    tty_write_chunked("零一二三四五六七八九");
    tty_write_text("\r\n\r\nCJK PTY/TTY test complete.\r\n");
    return 0;
}

static void append_char(char *dst, uint32_t *pos, uint32_t cap, char ch)
{
    if (!dst || !pos || *pos + 1 >= cap) {
        return;
    }
    dst[*pos] = ch;
    ++(*pos);
    dst[*pos] = 0;
}

static void append_text(char *dst, uint32_t *pos, uint32_t cap, const char *text)
{
    while (text && *text) {
        append_char(dst, pos, cap, *text++);
    }
}

static void append_int(char *dst, uint32_t *pos, uint32_t cap, int value)
{
    char tmp[16];
    uint32_t n = 0;
    unsigned int v;
    if (value < 0) {
        append_char(dst, pos, cap, '-');
        v = (unsigned int)(-value);
    } else {
        v = (unsigned int)value;
    }
    if (v == 0) {
        append_char(dst, pos, cap, '0');
        return;
    }
    while (v && n < sizeof(tmp)) {
        tmp[n++] = (char)('0' + (v % 10u));
        v /= 10u;
    }
    while (n) {
        append_char(dst, pos, cap, tmp[--n]);
    }
}

static void run_file_test(void)
{
    const char *dir = "/测试目录";
    const char *path = "/测试目录/你好.txt";
    const char *content = "你好，LeonOS 4。中文文件名和 UTF-8 内容测试。\n";
    struct leonos_stat st;
    struct leonos_dir_entry entry;
    uint32_t pos = 0;
    int mkdir_ret = mkdir(dir, 0777);
    int fd = open(path, LEONOS_O_CREAT | LEONOS_O_TRUNC | LEONOS_O_WRONLY, 0666);
    int write_ret = -1;
    int stat_ret;
    int readdir_seen = 0;

    if (fd >= 0) {
        write_ret = (int)write(fd, content, strlen(content));
        close(fd);
    }
    stat_ret = leonos_stat_legacy(path, &st);
    fd = open(dir, LEONOS_O_RDONLY, 0);
    if (fd >= 0) {
        while (leonos_readdir(fd, &entry) > 0) {
            printf("[cjktest.elf] readdir name=%s type=%d\n", entry.name, (int)entry.type);
            if (entry.name[0]) {
                readdir_seen = 1;
            }
        }
        close(fd);
    }

    append_text(status_line, &pos, sizeof(status_line), "mkdir=");
    append_int(status_line, &pos, sizeof(status_line), mkdir_ret);
    append_text(status_line, &pos, sizeof(status_line), " write=");
    append_int(status_line, &pos, sizeof(status_line), write_ret);
    append_text(status_line, &pos, sizeof(status_line), " stat=");
    append_int(status_line, &pos, sizeof(status_line), stat_ret);
    append_text(status_line, &pos, sizeof(status_line), " list=");
    append_text(status_line, &pos, sizeof(status_line), readdir_seen ? "OK" : "EMPTY");

    printf("[cjktest.elf] unicode file test %s\n", status_line);
}

static void draw(struct leonos_ui_surface *ui)
{
    uint32_t sample_w;
    leonos_ui_rect(ui, 0, 0, CJKTEST_W, CJKTEST_H, LEONOS_UI_WHITE);

    leonos_ui_text(ui, 18, 16, "你好，LeonOS 4。中文显示测试。", LEONOS_UI_BLACK, LEONOS_UI_WHITE);
    leonos_ui_text(ui, 18, 42, "标点：，。！？；：《》【】（）", LEONOS_UI_BLACK, LEONOS_UI_WHITE);
    leonos_ui_text(ui, 18, 68, "ASCII ABC 123  |  中文宽字符  |  mixed 混排", LEONOS_UI_BLACK, LEONOS_UI_WHITE);

    sample_w = leonos_ui_text_width("你好，LeonOS 4。");
    leonos_ui_text(ui, 18, 100, "leonos_ui_text_width(\"你好，LeonOS 4。\"):", LEONOS_UI_DARK, LEONOS_UI_WHITE);
    leonos_ui_progress(ui, 328, 98, 220, 18, sample_w, 220);

    leonos_ui_groupbox(ui, 18, 132, 684, 190, "UTF-8 文本域");
    leonos_ui_text_area_state_draw(ui, 32, 158, 656, 138, &text_state, 0);

    leonos_ui_statusbar(ui, CJKTEST_H - 28, 28, status_line);
}

int main(int argc, char **argv)
{
    struct leonos_ui_surface ui;
    struct leonos_gui_app_event event;
    int window_id;

    if (argc > 1 && argv[1] && strcmp(argv[1], "--tty") == 0) {
        return run_tty_test();
    }

    puts("[cjktest.elf] CJK display test starting");
    run_file_test();

    window_id = leonos_gui_create_app_window_ex("中文显示测试", "UTF-8 / CJK font test",
                                                CJKTEST_W, CJKTEST_H,
                                                LEONOS_GUI_WINDOW_NO_RESIZE);
    if (window_id <= 0) {
        printf("[cjktest.elf] create window failed=%d\n", window_id);
        return 1;
    }

    leonos_ui_bind(&ui, pixels, CJKTEST_W, CJKTEST_H, CJKTEST_W);
    leonos_ui_text_area_state_init(&text_state, text_buffer, sizeof(text_buffer));

    draw(&ui);
    leonos_gui_present_window((uint32_t)window_id, CJKTEST_W, CJKTEST_H, CJKTEST_W, pixels);

    for (;;) {
        event.window_id = (uint32_t)window_id;
        if (leonos_gui_wait_app_event(&event, LEONOS_GUI_IDLE_WAIT_MS) > 0) {
            if (event.type == LEONOS_GUI_APP_EVENT_CLOSE) {
                break;
            }
            if (event.type == LEONOS_GUI_APP_EVENT_MOUSE_BUTTON ||
                event.type == LEONOS_GUI_APP_EVENT_MOUSE_MOVE) {
                if (leonos_ui_text_area_state_handle_mouse(&text_state,
                                                           event.x,
                                                           event.y,
                                                           32,
                                                           158,
                                                           656,
                                                           138,
                                                           event.buttons)) {
                    draw(&ui);
                    leonos_gui_present_window((uint32_t)window_id,
                                              CJKTEST_W,
                                              CJKTEST_H,
                                              CJKTEST_W,
                                              pixels);
                }
            }
            if (event.type == LEONOS_GUI_APP_EVENT_MOUSE_WHEEL) {
                if (leonos_ui_vscrollbar_handle_wheel(&text_state.scroll_line,
                                                      text_state.line_count,
                                                      6,
                                                      event.dy)) {
                    draw(&ui);
                    leonos_gui_present_window((uint32_t)window_id,
                                              CJKTEST_W,
                                              CJKTEST_H,
                                              CJKTEST_W,
                                              pixels);
                }
            }
            if (event.type == LEONOS_GUI_APP_EVENT_KEY_DOWN ||
                event.type == LEONOS_GUI_APP_EVENT_KEY_UP) {
                if (leonos_ui_text_area_state_handle_key(&text_state,
                                                         event.keycode,
                                                         event.pressed,
                                                         636,
                                                         118)) {
                    draw(&ui);
                    leonos_gui_present_window((uint32_t)window_id,
                                              CJKTEST_W,
                                              CJKTEST_H,
                                              CJKTEST_W,
                                              pixels);
                }
            }
            if (event.type == LEONOS_GUI_APP_EVENT_RESIZE ||
                event.type == LEONOS_GUI_APP_EVENT_FOCUS) {
                draw(&ui);
                leonos_gui_present_window((uint32_t)window_id, CJKTEST_W, CJKTEST_H, CJKTEST_W, pixels);
            }
        } else {
            sleep_ms(10);
        }
    }
    return 0;
}
