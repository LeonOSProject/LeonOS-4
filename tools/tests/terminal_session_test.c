#include <assert.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/wait.h>

int tcsetwinsize(int fd, const struct winsize *winsize);

#define ioctl leonos_decl_ioctl
#define wait4 leonos_decl_wait4
#define main terminal_app_main
#include "../../userland/apps/terminal/main.c"
#undef main
#undef wait4
#undef ioctl

uint32_t leonos_ui_tab_height(void)
{
    return 26;
}

int tcsetwinsize(int fd, const struct winsize *winsize)
{
    return ioctl(fd, TIOCSWINSZ, winsize);
}

static void test_shell(void)
{
    pid_t parent = getpid();
    char *argv[] = {"sh", "-c", "printf 'PTY_SHELL_READY'; read answer; printf 'REPLY:%s' \"$answer\"", NULL};
    char *envp[] = {"TERM=xterm", NULL};
    struct terminal_session *session = terminal_open_session("/bin/sh", argv, envp);
    int status;

    /* Returning here in the child reproduces the erroneous GUI failure path. */
    if (getpid() != parent) _exit(91);
    assert(session && session->child_pid > 0);
    assert((fcntl(session->pty_fd, F_GETFL) & O_NONBLOCK) != 0);
    struct pollfd pfd = {.fd = session->pty_fd, .events = POLLIN};
    assert(poll(&pfd, 1, 2000) > 0);
    assert(terminal_pump_all_output() == 1);
    const char *expected = "PTY_SHELL_READY";
    for (size_t i = 0; expected[i]; ++i) {
        assert(history[0].cells[i].codepoint == (uint32_t)expected[i]);
    }
    assert(terminal_pump_all_output() == 0);
    assert(terminal_write_input("ok\n", 3) == 1);
    assert(waitpid(session->child_pid, &status, 0) == session->child_pid);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    assert(terminal_pump_all_output() == 1);
    expected = "REPLY:ok";
    for (size_t i = 0; expected[i]; ++i) {
        assert(history[1].cells[i].codepoint == (uint32_t)expected[i]);
    }
    close(session->pty_fd);
    puts("Terminal: shell exec, prompt, input and reply passed");
}

static void test_tab(void)
{
    active_session = &sessions[0];
    active_session->used = 1;
    terminal_reset_style();
    terminal_clear();
    cursor_column = 94; /* Default width is 95 columns, next tab stop is 96. */
    alarm(2);
    terminal_put_char('\t');
    alarm(0);
    assert(history_count == 1 && cursor_column == 94);
    terminal_put_char('X');
    assert(history[0].cells[94].codepoint == 'X');
    puts("Terminal: right-edge tab remains bounded");
}

static void test_cursor_down(void)
{
    active_session = &sessions[0];
    active_session->used = 1;
    terminal_reset_style();
    terminal_clear();
    for (int i = 0; i < 159; ++i) terminal_put_char('\n');
    assert(history_count == 160);
    alarm(2);
    terminal_put_text("\033[999B");
    alarm(0);
    assert(history_count == 160 && terminal_logical_active() == 159);
    terminal_put_char('X');
    assert(history[active_line].cells[0].codepoint == 'X');
    puts("Terminal: cursor down at full history remains bounded");
}

static void test_color_query(void)
{
    int fds[2];
    assert(pipe2(fds, O_NONBLOCK) == 0);
    active_session = &sessions[0];
    active_pty_fd = fds[1];
    terminal_reset_style();
    terminal_clear();
    /* Queries can be split over arbitrary PTY reads and use BEL or ST. */
    terminal_put_text("\033[38;2;255;0;0;48;2;255;255;255m");
    terminal_put_text("\033]11;");
    terminal_put_text("?\007\033]10;?\033");
    terminal_put_text("\\\033[c");
    const char *expected = "\033]11;rgb:0000/0000/0000\007"
                           "\033]10;rgb:d7d7/e3e3/f4f4\033\\\033[?1;0c";
    char reply[128] = {0};
    assert(read(fds[0], reply, sizeof(reply)) == (ssize_t)strlen(expected));
    assert(strcmp(reply, expected) == 0);
    assert(cursor_column == 0);
    /* Unsupported, malformed and oversized strings stay out of the grid. */
    terminal_put_text("\033]0;ignored title\007\033]11;?garbage\007\033]11;?");
    for (unsigned i = 0; i < 256; ++i) terminal_put_char('x');
    terminal_put_text("\033\\\033]11;?\030X");
    assert(cursor_column == 1 && history[0].cells[0].codepoint == 'X');
    assert(read(fds[0], reply, sizeof(reply)) == -1 && errno == EAGAIN);
    close(fds[0]);
    close(fds[1]);
    puts("Terminal: fragmented OSC color queries, DA1 and malformed strings passed");
}

static void test_extended_colors(void)
{
    active_session = &sessions[0];
    terminal_reset_style();
    terminal_clear();
    terminal_put_text("\033[38;2;18;52;86;48;2;101;67;33mA");
    assert(history[0].cells[0].foreground == 0x123456);
    assert(history[0].cells[0].background == 0x654321);
    assert(!text_dim && !text_bright);
    terminal_put_text("\033[38;5;196;48;5;232mB");
    assert(history[0].cells[1].foreground == 0xff0000);
    assert(history[0].cells[1].background == 0x080808);
    terminal_put_text("\033[38;5;255;48;5;16mC");
    assert(history[0].cells[2].foreground == 0xeeeeee);
    assert(history[0].cells[2].background == 0x000000);
    terminal_put_text("\033[39;49mD");
    assert(history[0].cells[3].foreground == 0xd7e3f4);
    assert(history[0].cells[3].background == 0);
    terminal_put_text("\033[38;2;256;0;0m\033[38;2;12mE");
    assert(history[0].cells[4].foreground == 0xd7e3f4 && !text_dim);
    puts("Terminal: RGB, 256-color, default reset and malformed SGR passed");
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    if (strcmp(argv[1], "shell") == 0) test_shell();
    else if (strcmp(argv[1], "tab") == 0) test_tab();
    else if (strcmp(argv[1], "cursor-down") == 0) test_cursor_down();
    else if (strcmp(argv[1], "color-query") == 0) test_color_query();
    else if (strcmp(argv[1], "extended-colors") == 0) test_extended_colors();
    else abort();
    return 0;
}
