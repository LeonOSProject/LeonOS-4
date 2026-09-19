/*
 * LeonOS input queue interface: declares normalized input event operations.
 * Used by interrupt handlers, drivers, and the input manager.
 */
#ifndef NTCLKS_INPUT_H
#define NTCLKS_INPUT_H

#include <ntclks/types.h>

#define INPUT_EVENT_MOUSE 1
#define INPUT_EVENT_KEYBOARD 2
#define INPUT_EVENT_MOUSE_WHEEL 3

struct input_raw_event {
    uint32_t type;
    int32_t x;
    int32_t y;
    int32_t dx;
    int32_t dy;
    uint8_t buttons;
    uint8_t keycode;
    uint8_t pressed;
    uint8_t modifiers;
};

/**
 * @brief Which terminal sink the physical keyboards currently feed as input.
 *
 * A keystroke may drive at most one line discipline. The GUI modes route the
 * keyboard through the evdev stream to windowd and then to the focused
 * application's own PTY; the tty modes hand it to the single console PTY.
 * The mapping from LEONOS_BOOT_MODE to an owner lives in userland.c and must
 * stay in sync with system/rootfs/usr/lib/leonos/console-session.
 */
enum input_keyboard_owner {
    INPUT_KEYBOARD_OWNER_CONSOLE = 0,
    INPUT_KEYBOARD_OWNER_GUI = 1
};

/**
 * @brief Initialize the input event queue.
 */
void input_init(void);
/**
 * @brief Enqueue a mouse move/drag: absolute position (x,y), relative delta (dx,dy), button mask.
 */
void input_push_mouse(int32_t x, int32_t y, int32_t dx, int32_t dy, uint8_t buttons);
/**
 * @brief Enqueue a mouse wheel event: position (x,y), scroll amount wheel, button mask.
 */
void input_push_mouse_wheel(int32_t x, int32_t y, int32_t wheel, uint8_t buttons);
/**
 * @brief Enqueue a keyboard event: keycode is the key, pressed is 1 for down / 0 for up.
 */
void input_push_key(uint8_t keycode, uint8_t pressed);
/**
 * @brief Deliver one physical-key event from a keyboard driver to the kernel.
 * @param keycode Set-1 make/break code after 0xe0 extension normalization.
 * @param pressed Non-zero for a make code, zero for a break code.
 *
 * Shared entry point for PS/2 and USB HID so the hardware paths cannot
 * diverge. Always publishes the normalized and evdev streams, and offers the
 * event to the console PTY, which applies keyboard ownership before accepting
 * it. Callable from interrupt context; takes the input lock internally, so
 * callers must not hold it.
 */
void input_handle_scancode(uint8_t keycode, uint8_t pressed);
/**
 * @brief Transfer physical keyboard input ownership to the console or the GUI.
 * @param owner Sink that subsequent keystrokes feed as terminal input.
 *
 * Existing buffered console input is not flushed: a GUI session never fills
 * the console queue, so no stale keystroke can be delivered on the switch.
 */
void input_set_keyboard_owner(enum input_keyboard_owner owner);
/**
 * @brief Return which sink currently owns physical keyboard input.
 */
enum input_keyboard_owner input_keyboard_owner(void);
uint8_t input_caps_lock_active(void);
/**
 * @brief Dequeue the oldest event into event; returns non-zero when one was available.
 */
int input_pop(struct input_raw_event *event);

/* Linux evdev readers receive their own cursor into a bounded fan-out ring.
 * Raw input delivery to the desktop remains independent, so opening an
 * event device cannot consume the desktop compositor's input queue. */
uint64_t input_evdev_cursor_now(void);
int input_evdev_read(uint32_t device_kind, uint64_t *cursor,
                     void *buffer, uint32_t length, uint64_t grab_token);
int input_evdev_available(uint32_t device_kind, uint64_t cursor,
                          uint64_t grab_token);
/**
 * @brief Acquire or release EVIOCGRAB ownership for an event node.
 * @return New grab token on acquire/release, 0 when the device is invalid,
 *         or -16 when another client owns the exclusive grab.
 */
int64_t input_evdev_grab(uint32_t device_kind, uint64_t current_token,
                         int enable, uint32_t pid);
/**
 * @brief Release a closing descriptor's grab ownership.
 */
void input_evdev_release(uint32_t device_kind, uint64_t grab_token,
                         uint32_t pid);
/**
 * @brief Copy the current key state bitmap for EVIOCGKEY.
 */
void input_evdev_key_state(void *buffer, uint32_t length);
/**
 * @brief Copy the supported event/capability bitmap for EVIOCGBIT.
 */
void input_evdev_capabilities(uint32_t device_kind, uint32_t event_type,
                              void *buffer, uint32_t length);
struct input_absinfo;
/** @brief Return the current pointer coordinate and framebuffer axis bounds. */
int input_evdev_absinfo(uint32_t axis, struct input_absinfo *info);
/**
 * @brief Return non-zero when the event device is currently present.
 */
int input_evdev_present(uint32_t device_kind);

#endif
