#ifndef RELIEFOS_WINDOWD_H
#define RELIEFOS_WINDOWD_H

#include <reliefos/gui.h>
#include <stdint.h>

enum reliefos_windowd_msg {
    RELIEFOS_WIN_MSG_ERROR = 0,
    RELIEFOS_WIN_MSG_HELLO = 10,
    RELIEFOS_WIN_MSG_HELLO_ACK = 11,
    RELIEFOS_WIN_MSG_POLICY_HELLO = 12,
    RELIEFOS_WIN_MSG_CREATE = 20,
    RELIEFOS_WIN_MSG_CREATE_ACK = 21,
    RELIEFOS_WIN_MSG_DESTROY = 22,
    RELIEFOS_WIN_MSG_PRESENT = 23,
    RELIEFOS_WIN_MSG_UPDATE = 24,
    RELIEFOS_WIN_MSG_FETCH = 25,
    RELIEFOS_WIN_MSG_FETCH_ACK = 26,
    RELIEFOS_WIN_MSG_BUFFER = 27,
    RELIEFOS_WIN_MSG_BUFFER_ACK = 28,
    RELIEFOS_WIN_MSG_EVENT = 30,
    RELIEFOS_WIN_MSG_INPUT = 31,
    RELIEFOS_WIN_MSG_WINDOW_NOTIFY = 32,
    RELIEFOS_WIN_MSG_MOUSE_VISIBLE = 40,
    RELIEFOS_WIN_MSG_CURSOR_REQUEST = 41,
    RELIEFOS_WIN_MSG_CURSOR_REGION = 42,
    RELIEFOS_WIN_MSG_TASKBAR = 43,
    RELIEFOS_WIN_MSG_DISPLAY_STATE = 50,
    RELIEFOS_WIN_MSG_DISPLAY_REQUEST = 51,
    RELIEFOS_WIN_MSG_APPEARANCE_STATE = 52,
    RELIEFOS_WIN_MSG_APPEARANCE_REQUEST = 53,
};

#define RELIEFOS_WIN_POLICY_TOKEN "desktop-policy-v1"
#define RELIEFOS_WIN_ROLE_APP 1u
#define RELIEFOS_WIN_ROLE_POLICY 2u

struct reliefos_win_hello {
    uint32_t pid;
    uint32_t role;
};

struct reliefos_win_hello_ack {
    uint32_t version;
    uint32_t reserved;
};

struct reliefos_win_policy_hello {
    uint32_t pid;
    uint32_t reserved;
    char token[32];
};

struct reliefos_win_create {
    uint32_t width;
    uint32_t height;
    uint32_t flags;
    char title[48];
    char text[1024];
};

struct reliefos_win_create_ack {
    uint32_t window_id;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
};

struct reliefos_win_destroy {
    uint32_t window_id;
    uint32_t reserved;
};

struct reliefos_win_present {
    uint32_t window_id;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
};

/* BUFFER attaches a fully painted replacement via SCM_RIGHTS. Its stride
 * counts bytes; PRESENT retains the historical pixel-stride convention. */
struct reliefos_win_buffer {
    uint32_t window_id;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
};

#define RELIEFOS_WIN_SURFACE_REPLACED 1u /* WINDOW_NOTIFY type 2, data */

struct reliefos_win_update {
    uint32_t window_id;
    uint32_t mask;
    uint32_t flags;
    char title[48];
};

struct reliefos_win_fetch {
    uint32_t window_id;
    uint32_t capacity_width;
    uint32_t capacity_height;
    uint32_t stride;
};

struct reliefos_win_fetch_ack {
    uint32_t window_id;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
};

struct reliefos_win_taskbar {
    uint32_t window_id;
    uint32_t visible;
};

struct reliefos_win_mouse_visible {
    uint32_t window_id;
    uint32_t visible;
};

struct reliefos_win_error {
    int32_t code;
    uint32_t reserved;
};

#endif
