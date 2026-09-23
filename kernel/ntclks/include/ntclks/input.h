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
 * diverge. Queues runtime input for the execution-serialized consumer; early
 * boot input is published directly. Callable from interrupt context; takes
 * input_lock internally, so callers must not hold it.
 */
void input_handle_scancode(uint8_t keycode, uint8_t pressed);
/** @brief Drain queued keyboard input outside the physical keyboard ISR.
 * The caller must hold the kernel execution transaction, without input_lock.
 */
void input_process_pending(void);
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
/** @brief Set graphical origin and publish pointer/modifier state when resuming a VT.
 * @param number Active graphical VT, or zero for text/boot input.
 */
void input_set_graphical_vt(uint32_t number);
/** @brief Read events filtered by their graphical VT at production time.
 * @param device_kind Keyboard or mouse device kind.
 * @param cursor In/out reader sequence position.
 * @param buffer Writable array of Linux input_event records.
 * @param length Buffer size in bytes, a multiple of input_event size.
 * @param grab_token Open description's EVIOCGRAB token.
 * @param number Graphical VT filter, or zero for all events.
 * @return Bytes read or negative errno.
 */
int input_evdev_read_vt(uint32_t device_kind, uint64_t *cursor,
                        void *buffer, uint32_t length, uint64_t grab_token, uint32_t number);
/** @brief Test whether the same filtered read would produce a record.
 * @param device_kind Keyboard or mouse device kind.
 * @param cursor Reader sequence position.
 * @param grab_token Open description's EVIOCGRAB token.
 * @param number Graphical VT filter, or zero for all events.
 * @return Nonzero if a matching record is available.
 */
int input_evdev_available_vt(uint32_t device_kind, uint64_t cursor,
                             uint64_t grab_token, uint32_t number);
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
