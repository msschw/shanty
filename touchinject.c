/*
 * touchinject — virtual multi-touch touchscreen via uinput, for driving a
 * headless Sailfish OS emulator over ssh (Qt evdev picks the device up and
 * feeds Silica gestures natively).
 *
 * Protocol B (slot/tracking-id). ABS ranges map 1:1 to the 720x1600
 * compositor screen; override with SHANTY_TOUCH_W/H if the profile differs.
 *
 * Usage: sudo ./touchinject   (reads commands from stdin, one per line:
 *   tap X Y [HOLD_MS]
 *   swipe X1 Y1 X2 Y2 TOTAL_MS
 *   key EVDEV_CODE            (press+release a key, e.g. 28 = ENTER)
 *   type STRING                (types ASCII via keycodes; 'a'-'z', '0'-'9',
 *                               ' ', '.', ',', '-', '/', RETURN)
 *   quit
 * stdin EOF also exits.) The device exists for the process lifetime, so
 * keep the process running across commands (ssh stdin pipe).
 *
 * Build (SDK i486 target): gcc -O2 -o touchinject touchinject.c
 * Copyright 2026 Shanty contributors. SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/uinput.h>
#include <sys/ioctl.h>

static int fd = -1;

static void emit(int type, int code, int value) {
    struct input_event e;
    memset(&e, 0, sizeof e);
    e.type = type;
    e.code = code;
    e.value = value;
    if (write(fd, &e, sizeof e) < 0)
        perror("emit");
}

static void syn(void) { emit(EV_SYN, SYN_REPORT, 0); }

static int width(void) {
    const char *w = getenv("SHANTY_TOUCH_W");
    return w ? atoi(w) : 720;
}
static int height(void) {
    const char *h = getenv("SHANTY_TOUCH_H");
    return h ? atoi(h) : 1600;
}

static void down(int x, int y) {
    emit(EV_ABS, ABS_MT_SLOT, 0);
    emit(EV_ABS, ABS_MT_TRACKING_ID, 1);
    emit(EV_ABS, ABS_MT_POSITION_X, x);
    emit(EV_ABS, ABS_MT_POSITION_Y, y);
    emit(EV_KEY, BTN_TOUCH, 1);
    syn();
}

static void move(int x, int y) {
    emit(EV_ABS, ABS_MT_SLOT, 0);
    emit(EV_ABS, ABS_MT_POSITION_X, x);
    emit(EV_ABS, ABS_MT_POSITION_Y, y);
    syn();
}

static void up(void) {
    emit(EV_ABS, ABS_MT_SLOT, 0);
    emit(EV_ABS, ABS_MT_TRACKING_ID, -1);
    emit(EV_KEY, BTN_TOUCH, 0);
    syn();
}

// ---- keyboard --------------------------------------------------------------

static void key_evdev(int code, int pressed) {
    emit(EV_KEY, code, pressed);
    syn();
}

static void key_tap(int code) {
    key_evdev(code, 1);
    usleep(30000);
    key_evdev(code, 0);
    usleep(120000);  // slow cadence: the boosted app's evdev reader drops
                     // events typed back-to-back too fast
}

/// US layout evdev keycodes.
static int ascii_code(char c, int* shift) {
    *shift = 0;
    if (c >= 'a' && c <= 'z') return KEY_A + (c - 'a');
    if (c >= 'A' && c <= 'Z') {
        *shift = 1;
        return KEY_A + (c - 'A');
    }
    if (c >= '1' && c <= '9') return KEY_1 + (c - '1');
    if (c == '0') return KEY_0;
    switch (c) {
        case ' ': return KEY_SPACE;
        case '.': return KEY_DOT;
        case ',': return KEY_COMMA;
        case '-': return KEY_MINUS;
        case '/': return KEY_SLASH;
        case '\n': return KEY_ENTER;
        default: return -1;
    }
}

static void do_type(const char* s) {
    for (; *s; ++s) {
        int shift = 0;
        int code = ascii_code(*s, &shift);
        if (code < 0) continue;
        if (shift) key_tap(KEY_LEFTSHIFT);
        key_tap(code);
        usleep(40000);
    }
    printf("ok type\n");}

static void do_tap(int x, int y, int holdMs) {
    down(x, y);
    usleep(holdMs ? holdMs * 1000 : 40000);
    up();
    usleep(40000);
}

static void do_swipe(int x1, int y1, int x2, int y2, int totalMs) {
    const int steps = 24;
    int i;
    down(x1, y1);
    usleep(60000);
    for (i = 1; i <= steps; ++i) {
        int x = x1 + (x2 - x1) * i / steps;
        int y = y1 + (y2 - y1) * i / steps;
        move(x, y);
        usleep(totalMs * 1000 / steps);
    }
    usleep(30000);
    up();
    usleep(40000);
}

static void create_device(void) {
    struct uinput_user_dev u;
    int w = width(), h = height();

    fd = open("/dev/uinput", O_WRONLY);
    if (fd < 0) { perror("/dev/uinput"); exit(1); }

    ioctl(fd, UI_SET_EVBIT, EV_KEY);
    ioctl(fd, UI_SET_KEYBIT, BTN_TOUCH);
    ioctl(fd, UI_SET_EVBIT, EV_ABS);
    // Keyboard keys: enough of the US layout for the picker text fields.
    ioctl(fd, UI_SET_EVBIT, EV_KEY);
    ioctl(fd, UI_SET_KEYBIT, KEY_ENTER);
    ioctl(fd, UI_SET_KEYBIT, KEY_LEFTSHIFT);
    ioctl(fd, UI_SET_KEYBIT, KEY_SPACE);
    ioctl(fd, UI_SET_KEYBIT, KEY_DOT);
    ioctl(fd, UI_SET_KEYBIT, KEY_COMMA);
    ioctl(fd, UI_SET_KEYBIT, KEY_MINUS);
    ioctl(fd, UI_SET_KEYBIT, KEY_SLASH);
    ioctl(fd, UI_SET_KEYBIT, KEY_0);
    ioctl(fd, UI_SET_KEYBIT, KEY_9);
    for (int code = KEY_A; code <= KEY_Z; ++code)
        ioctl(fd, UI_SET_KEYBIT, code);
    ioctl(fd, UI_SET_EVBIT, EV_SYN);
    ioctl(fd, UI_SET_ABSBIT, ABS_MT_SLOT);
    ioctl(fd, UI_SET_ABSBIT, ABS_MT_TRACKING_ID);
    ioctl(fd, UI_SET_ABSBIT, ABS_MT_POSITION_X);
    ioctl(fd, UI_SET_ABSBIT, ABS_MT_POSITION_Y);

    memset(&u, 0, sizeof u);
    strcpy(u.name, "shanty-touch");
    u.id.bustype = BUS_VIRTUAL;
    u.id.vendor = 0x0001;
    u.id.product = 0x0001;
    u.id.version = 1;
    u.absmin[ABS_MT_POSITION_X] = 0;
    u.absmax[ABS_MT_POSITION_X] = w - 1;
    u.absmin[ABS_MT_POSITION_Y] = 0;
    u.absmax[ABS_MT_POSITION_Y] = h - 1;
    u.absmax[ABS_MT_SLOT] = 9;
    u.absmax[ABS_MT_TRACKING_ID] = 65535;
    if (write(fd, &u, sizeof u) < 0) { perror("setup"); exit(1); }
    if (ioctl(fd, UI_DEV_CREATE) < 0) { perror("UI_DEV_CREATE"); exit(1); }
    fprintf(stderr, "touchinject: shanty-touch created (%dx%d)\n", w, h);
}

int main(void) {
    char line[512];
    setvbuf(stdout, NULL, _IOLBF, 0);
    create_device();
    // Per-batch mode (process lifetime = one command batch): Qt attaches a
    // freshly hotplugged device asynchronously; events written before the
    // attach are buffered by the kernel but a swipe can overflow the 64
    // event buffer, and destroying the device too early discards events
    // that have not been read yet (both observed as "lost gestures").
    sleep(1);  // let the consumer attach before the first event

    while (fgets(line, sizeof line, stdin)) {
        char cmd[32];
        int a, b, c, d, e;
        int n = sscanf(line, "%31s %d %d %d %d %d", cmd, &a, &b, &c, &d, &e);
        if (n <= 0) continue;
        if (!strcmp(cmd, "quit")) break;
        if (!strcmp(cmd, "tap") && (n == 3 || n == 4)) {
            do_tap(a, b, n == 4 ? c : 0);
            printf("ok tap %d %d\n", a, b);
        } else if (!strcmp(cmd, "swipe") && n == 6) {
            do_swipe(a, b, c, d, e);
            printf("ok swipe\n");
        } else if (!strcmp(cmd, "key") && n == 2) {
            key_tap(a);
            printf("ok key %d\n", a);
        } else if (!strcmp(cmd, "type")) {
            const char* arg = strchr(line, ' ');
            if (arg && arg[1] != '\0' && arg[1] != '\n') {
                do_type(arg + 1);
            } else {
                printf("err: type needs an argument\n");
            }
        } else {
            printf("err: %s", line);
        }
        fflush(stdout);
    }

    // Grace period: give the consumer time to drain the last events before
    // the device (and its buffer) disappears.
    fflush(stdout);
    sleep(1);
    ioctl(fd, UI_DEV_DESTROY);
    close(fd);
    fprintf(stderr, "touchinject: device destroyed\n");
    return 0;
}
