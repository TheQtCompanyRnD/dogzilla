/*
 * oledd - show a text file on the Dogzilla's 128x32 SSD1306 OLED.
 *
 * Talks to the display from userspace over /dev/i2c-1, deliberately avoiding
 * the kernel's ssd130x framebuffer driver: that route worked (as /dev/fb1 with
 * a 4-line virtual console) but panicked the kernel with "scheduling while
 * atomic" when I2C traffic collided with Wi-Fi interrupts on the Pi 5's RP1.
 * In userspace a bus hiccup is just an EIO from write(2).
 *
 * The display content is a plain UTF-8 text file (see -f): up to one line per
 * 8-pixel page, redrawn whenever the file changes (inotify) and periodically
 * as a safety net. dogzillad's ConsoleDashboard writes that file.
 *
 * A well-behaved writer rewrites the file from the start every time, so the
 * file holds exactly one frame. We are nevertheless tolerant of a writer that
 * streams frames at us like a terminal (which ConsoleDashboard's tty mode
 * does, with a "home the cursor" escape before each frame): we read a window
 * at the *end* of the file, keep what follows the last home/clear escape, and
 * drop any other escape sequences.
 *
 * Build: cc -O2 -Wall -o oledd oledd.c   (no libraries at all)
 */

#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#if defined(__has_include)
#  if __has_include(<linux/i2c-dev.h>)
#    include <linux/i2c-dev.h>
#  endif
#endif
#ifndef I2C_SLAVE
#  define I2C_SLAVE 0x0703 /* from linux/i2c-dev.h, hardcoded so we need no headers */
#endif

#include "font.h"

/* SSD1306 command set (SSD1306 datasheet rev 1.1, chapter 9). */
enum {
    SetContrast = 0x81,
    DisplayAllOnResume = 0xa4,
    NormalDisplay = 0xa6,
    InvertDisplay = 0xa7,
    DisplayOff = 0xae,
    DisplayOn = 0xaf,
    SetDisplayOffset = 0xd3,
    SetComPins = 0xda,
    SetVComDetect = 0xdb,
    SetDisplayClockDiv = 0xd5,
    SetPrecharge = 0xd9,
    SetMultiplex = 0xa8,
    SetStartLine = 0x40,
    MemoryMode = 0x20,
    ColumnAddr = 0x21,
    PageAddr = 0x22,
    ComScanInc = 0xc0,
    ComScanDec = 0xc8,
    SegRemap = 0xa0,
    ChargePump = 0x8d,
};

#define MAX_WIDTH 128
#define MAX_PAGES 8
#define MAX_TEXT 4096

typedef struct {
    const char *bus;
    int addr;
    int fd;
    int width;
    int height;
    int pages;
    int contrast;
    int compins;  /* -1 = derive from height */
    int flip;     /* 0 = as the Dogzilla's panel is mounted, 1 = upside down */
    int invert;   /* black on white */
    unsigned char fb[MAX_WIDTH * MAX_PAGES];
    unsigned char sent[MAX_WIDTH * MAX_PAGES];
    int sent_valid;
} Oled;

static volatile sig_atomic_t quit_requested;
static int verbose;

static void on_signal(int sig)
{
    (void)sig;
    quit_requested = 1;
}

static void logv(const char *fmt, ...)
{
    if (!verbose)
        return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
}

static void logerr(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
}

/* ---------------------------------------------------------------- display */

/* A control byte of 0x00 means "everything that follows is a command". */
static int oled_cmds(Oled *o, const unsigned char *cmds, size_t n)
{
    unsigned char buf[32];
    if (n + 1 > sizeof(buf))
        return -1;
    buf[0] = 0x00;
    memcpy(buf + 1, cmds, n);
    ssize_t w = write(o->fd, buf, n + 1);
    if (w != (ssize_t)(n + 1)) {
        logerr("oledd: command write failed: %s", strerror(errno));
        return -1;
    }
    return 0;
}

static int oled_cmd1(Oled *o, unsigned char c)
{
    return oled_cmds(o, &c, 1);
}

static int oled_open(Oled *o)
{
    o->fd = open(o->bus, O_RDWR);
    if (o->fd < 0) {
        logerr("oledd: %s: %s", o->bus, strerror(errno));
        return -1;
    }
    if (ioctl(o->fd, I2C_SLAVE, o->addr) < 0) {
        logerr("oledd: cannot select address 0x%02x on %s: %s", o->addr, o->bus, strerror(errno));
        close(o->fd);
        o->fd = -1;
        return -1;
    }
    return 0;
}

/*
 * Same initialisation the vendor's Adafruit_SSD1306 python used on this robot,
 * which is what makes the pixels land where they should:
 *
 * - SetComPins 0x02 says the panel has *sequential* COM pin wiring. Getting
 *   this wrong is the "scrambled pixels" failure: with the alternative (0x12)
 *   mapping every other row is interleaved. It is the same knob as the
 *   sequential=1 parameter we needed on the kernel's ssd1306 DT overlay.
 * - SegRemap|1 plus ComScanDec rotate by 180 degrees, because the panel is
 *   mounted upside down in the dog (the overlay wanted com-invdir=1 for the
 *   same reason). Pass --flip to get the other orientation.
 */
static int oled_init(Oled *o)
{
    const int compins = o->compins >= 0 ? o->compins : (o->height > 32 ? 0x12 : 0x02);
    const unsigned char init[] = {
        DisplayOff,
        SetDisplayClockDiv, 0x80,
        SetMultiplex, (unsigned char)(o->height - 1),
        SetDisplayOffset, 0x00,
        SetStartLine | 0x00,
        ChargePump, 0x14,               /* 0x14 = generate Vcc from the 3.3V rail */
        MemoryMode, 0x00,               /* horizontal addressing */
        (unsigned char)(o->flip ? SegRemap : SegRemap | 0x01),
        (unsigned char)(o->flip ? ComScanInc : ComScanDec),
        SetComPins, (unsigned char)compins,
        SetContrast, (unsigned char)o->contrast,
        SetPrecharge, 0xf1,
        SetVComDetect, 0x40,
        DisplayAllOnResume,
        (unsigned char)(o->invert ? InvertDisplay : NormalDisplay),
    };
    for (size_t i = 0; i < sizeof(init); i += 8) {
        size_t n = sizeof(init) - i < 8 ? sizeof(init) - i : 8;
        if (oled_cmds(o, init + i, n) < 0)
            return -1;
    }
    o->sent_valid = 0;
    return oled_cmd1(o, DisplayOn);
}

static void oled_close(Oled *o, int blank)
{
    if (o->fd < 0)
        return;
    if (blank) {
        memset(o->fb, 0, sizeof(o->fb));
        for (int page = 0; page < o->pages; ++page) {
            unsigned char addr[] = { ColumnAddr, 0, (unsigned char)(o->width - 1),
                                     PageAddr, (unsigned char)page, (unsigned char)page };
            unsigned char buf[1 + MAX_WIDTH];
            buf[0] = 0x40;
            memset(buf + 1, 0, (size_t)o->width);
            if (oled_cmds(o, addr, sizeof(addr)) == 0)
                (void)!write(o->fd, buf, (size_t)o->width + 1);
        }
        oled_cmd1(o, DisplayOff);
    }
    close(o->fd);
    o->fd = -1;
    o->sent_valid = 0;
}

/* Push only the pages that changed; the clock ticking usually dirties one. */
static int oled_flush(Oled *o)
{
    for (int page = 0; page < o->pages; ++page) {
        unsigned char *row = o->fb + page * o->width;
        unsigned char *old = o->sent + page * o->width;
        if (o->sent_valid && memcmp(row, old, (size_t)o->width) == 0)
            continue;

        unsigned char addr[] = { ColumnAddr, 0, (unsigned char)(o->width - 1),
                                 PageAddr, (unsigned char)page, (unsigned char)page };
        if (oled_cmds(o, addr, sizeof(addr)) < 0)
            return -1;

        unsigned char buf[1 + MAX_WIDTH];
        buf[0] = 0x40; /* control byte: data follows */
        memcpy(buf + 1, row, (size_t)o->width);
        ssize_t w = write(o->fd, buf, (size_t)o->width + 1);
        if (w != (ssize_t)o->width + 1) {
            logerr("oledd: page %d write failed: %s", page, strerror(errno));
            return -1;
        }
        memcpy(old, row, (size_t)o->width);
    }
    o->sent_valid = 1;
    return 0;
}

static void oled_dump(const Oled *o)
{
    printf("+");
    for (int x = 0; x < o->width; ++x)
        printf("-");
    printf("+\n");
    for (int page = 0; page < o->pages; ++page) {
        for (int bit = 0; bit < 8; ++bit) {
            printf("|");
            for (int x = 0; x < o->width; ++x)
                printf("%c", (o->fb[page * o->width + x] & (1 << bit)) ? '#' : ' ');
            printf("|\n");
        }
    }
    printf("+");
    for (int x = 0; x < o->width; ++x)
        printf("-");
    printf("+\n");
}

/* ------------------------------------------------------------------- text */

/* Decode one UTF-8 sequence, advancing *p. Invalid bytes decode to U+FFFD. */
static unsigned utf8_next(const char **p)
{
    const unsigned char *s = (const unsigned char *)*p;
    unsigned c = *s++;
    int extra;
    if (c < 0x80) {
        *p = (const char *)s;
        return c;
    } else if ((c & 0xe0) == 0xc0) {
        c &= 0x1f;
        extra = 1;
    } else if ((c & 0xf0) == 0xe0) {
        c &= 0x0f;
        extra = 2;
    } else if ((c & 0xf8) == 0xf0) {
        c &= 0x07;
        extra = 3;
    } else {
        *p = (const char *)s;
        return 0xfffd;
    }
    while (extra--) {
        if ((*s & 0xc0) != 0x80) {
            *p = (const char *)s;
            return 0xfffd;
        }
        c = (c << 6) | (*s++ & 0x3f);
    }
    *p = (const char *)s;
    return c;
}

/*
 * Fill cols[] with one glyph. Codepoints the font doesn't cover get either a
 * drawn substitute (the box-drawing characters ConsoleDashboard's battery gauge
 * uses) or the nearest ASCII lookalike.
 */
static void glyph_cols(const OledFont *f, unsigned cp, unsigned char *cols)
{
    memset(cols, 0, f->width);
    switch (cp) {
    case 0x2551: /* '║' full battery bar: two vertical rules */
        cols[1] = 0xff;
        cols[f->width > 5 ? 4 : 3] = 0xff;
        return;
    case 0x2502: /* '│' half */
        cols[f->width / 2] = 0xff;
        return;
    case 0x2588: /* '█' */
        memset(cols, 0xff, f->width);
        return;
    case 0x2580: /* '▀' */
        memset(cols, 0x0f, f->width);
        return;
    case 0x2584: /* '▄' */
        memset(cols, 0xf0, f->width);
        return;
    default:
        break;
    }

    if (cp < f->first || cp > f->last) {
        static const struct { unsigned cp; unsigned char ascii; } aliases[] = {
            { 0x00b7, '.' }, { 0x00b0, 'o' }, { 0x2022, '.' }, { 0x2500, '-' },
            { 0x2550, '=' }, { 0x00b5, 'u' }, { 0x2192, '>' }, { 0x2190, '<' },
        };
        unsigned sub = '?';
        for (size_t i = 0; i < sizeof(aliases) / sizeof(aliases[0]); ++i) {
            if (aliases[i].cp == cp) {
                sub = aliases[i].ascii;
                break;
            }
        }
        cp = sub;
        if (cp < f->first || cp > f->last)
            return;
    }
    memcpy(cols, f->bits + (size_t)(cp - f->first) * f->width, f->width);
}

/* Lay out text into the framebuffer: one text line per 8-pixel page. */
static void render_text(Oled *o, const OledFont *f, const char *text)
{
    memset(o->fb, 0, sizeof(o->fb));
    const int cols = o->width / f->width;
    const char *p = text;
    for (int line = 0; line < o->pages; ++line) {
        int col = 0;
        while (*p && *p != '\n') {
            unsigned cp = utf8_next(&p);
            if (cp == '\r')
                continue;
            if (cp == '\t')
                cp = ' ';
            if (col >= cols)
                continue; /* keep scanning to the newline */
            unsigned char g[8];
            glyph_cols(f, cp, g);
            memcpy(o->fb + line * o->width + col * f->width, g, f->width);
            ++col;
        }
        if (!*p)
            break;
        ++p; /* skip the newline */
    }
}

/* ------------------------------------------------------------------- loop */

static long elapsed_ms(const struct timespec *from, const struct timespec *to)
{
    return (to->tv_sec - from->tv_sec) * 1000L + (to->tv_nsec - from->tv_nsec) / 1000000L;
}

static void now_mono(struct timespec *ts)
{
    clock_gettime(CLOCK_MONOTONIC, ts);
}

static void deadline_in(struct timespec *ts, const struct timespec *from, long ms)
{
    ts->tv_sec = from->tv_sec + ms / 1000;
    ts->tv_nsec = from->tv_nsec + (ms % 1000) * 1000000L;
    if (ts->tv_nsec >= 1000000000L) {
        ts->tv_sec++;
        ts->tv_nsec -= 1000000000L;
    }
}

/* Drop ANSI escape sequences in place; the writer may talk to us like a tty. */
static size_t strip_escapes(char *s, size_t n)
{
    size_t w = 0;
    for (size_t r = 0; r < n;) {
        if (s[r] != 0x1b) {
            s[w++] = s[r++];
            continue;
        }
        ++r;
        if (r < n && s[r] == '[') {
            ++r;
            while (r < n && (unsigned char)s[r] < 0x40) /* parameter bytes */
                ++r;
        }
        if (r < n)
            ++r; /* final byte, or the single character of a short escape */
    }
    s[w] = '\0';
    return w;
}

/*
 * Offset just past the last "cursor home" or "clear screen" escape, which is
 * where the newest frame begins in a stream of them.
 */
static size_t last_frame_start(const char *s, size_t n)
{
    size_t start = 0;
    for (size_t i = 0; i < n; ++i) {
        if (s[i] != 0x1b)
            continue;
        size_t j = i + 1;
        if (j < n && s[j] == '[') {
            const size_t params = ++j;
            while (j < n && (unsigned char)s[j] < 0x40)
                ++j;
            if (j < n) {
                const char final = s[j];
                if (final == 'H' || final == 'f' /* cursor position */
                    || (final == 'J' && memchr(s + params, '2', j - params)))
                    start = j + 1;
            }
        } else if (j < n && s[j] == 'c') { /* RIS, full reset */
            start = j + 1;
        }
    }
    return start;
}

/*
 * Read the newest frame into buf. Returns 1 if there is something to show,
 * 0 if the file is (momentarily) empty, -1 if it cannot be read.
 */
static int read_frame(const char *path, char *buf, size_t size)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0)
        return -1;
    const off_t end = lseek(fd, 0, SEEK_END);
    if (end < 0) {
        close(fd);
        return -1;
    }
    /* Always read relative to the end, so a stream of frames costs us nothing. */
    const off_t from = end > (off_t)(size - 1) ? end - (off_t)(size - 1) : 0;
    ssize_t n = -1;
    if (lseek(fd, from, SEEK_SET) >= 0)
        n = read(fd, buf, size - 1);
    close(fd);
    if (n < 0)
        return -1;
    buf[n] = '\0';

    size_t start = last_frame_start(buf, (size_t)n);
    while (start < (size_t)n && (buf[start] == '\n' || buf[start] == '\r'))
        ++start;
    size_t len = (size_t)n - start;
    memmove(buf, buf + start, len);
    buf[len] = '\0';
    len = strip_escapes(buf, len);
    return len ? 1 : 0;
}

static void usage(FILE *f)
{
    fprintf(f,
        "usage: oledd [options]\n"
        "  -f, --file PATH      text file to display (default /tmp/dogzilla-oled.txt)\n"
        "  -b, --bus PATH       I2C bus (default /dev/i2c-1)\n"
        "  -a, --addr N         I2C address (default 0x3c)\n"
        "  -W, --width N        panel width (default 128)\n"
        "  -H, --height N       panel height, 32 or 64 (default 32)\n"
        "  -F, --font NAME      5x8 (25 columns, default) or 6x8 (21 columns)\n"
        "  -c, --contrast N     0..255 (default 143)\n"
        "  -C, --compins N      COM pin config override: 0x02 sequential, 0x12 alternative\n"
        "      --flip           rotate 180 degrees from the Dogzilla's mounting\n"
        "      --invert         black on white\n"
        "  -m, --min-interval MS  minimum time between redraws (default 100)\n"
        "  -s, --settle MS      wait for the file to go quiet this long before drawing,\n"
        "                       so a frame written line by line is never shown half done\n"
        "                       (default 50)\n"
        "  -p, --poll SEC       re-read the file this often as a safety net (default 5)\n"
        "  -t, --text STR       display this text instead of watching a file, then exit\n"
        "  -1, --once           draw the file once and exit\n"
        "  -n, --dry-run        render to stdout as ASCII art, touch no hardware\n"
        "  -v, --verbose\n"
        "  -h, --help\n");
}

int main(int argc, char **argv)
{
    Oled o = {
        .bus = "/dev/i2c-1",
        .addr = 0x3c,
        .fd = -1,
        .width = 128,
        .height = 32,
        .contrast = 0x8f,
        .compins = -1,
    };
    const char *path = "/tmp/dogzilla-oled.txt";
    const char *font_name = "5x8";
    const char *literal = NULL;
    int min_interval = 100;
    int settle_ms = 50;
    int poll_sec = 5;
    int once = 0, dry_run = 0;

    static const struct option opts[] = {
        { "file", required_argument, 0, 'f' },
        { "bus", required_argument, 0, 'b' },
        { "addr", required_argument, 0, 'a' },
        { "width", required_argument, 0, 'W' },
        { "height", required_argument, 0, 'H' },
        { "font", required_argument, 0, 'F' },
        { "contrast", required_argument, 0, 'c' },
        { "compins", required_argument, 0, 'C' },
        { "flip", no_argument, 0, 'R' },
        { "invert", no_argument, 0, 'I' },
        { "min-interval", required_argument, 0, 'm' },
        { "settle", required_argument, 0, 's' },
        { "poll", required_argument, 0, 'p' },
        { "text", required_argument, 0, 't' },
        { "once", no_argument, 0, '1' },
        { "dry-run", no_argument, 0, 'n' },
        { "verbose", no_argument, 0, 'v' },
        { "help", no_argument, 0, 'h' },
        { 0, 0, 0, 0 },
    };

    int c;
    while ((c = getopt_long(argc, argv, "f:b:a:W:H:F:c:C:m:s:p:t:1nvh", opts, NULL)) != -1) {
        switch (c) {
        case 'f': path = optarg; break;
        case 'b': o.bus = optarg; break;
        case 'a': o.addr = (int)strtol(optarg, NULL, 0); break;
        case 'W': o.width = atoi(optarg); break;
        case 'H': o.height = atoi(optarg); break;
        case 'F': font_name = optarg; break;
        case 'c': o.contrast = (int)strtol(optarg, NULL, 0); break;
        case 'C': o.compins = (int)strtol(optarg, NULL, 0); break;
        case 'R': o.flip = 1; break;
        case 'I': o.invert = 1; break;
        case 'm': min_interval = atoi(optarg); break;
        case 's': settle_ms = atoi(optarg); break;
        case 'p': poll_sec = atoi(optarg); break;
        case 't': literal = optarg; break;
        case '1': once = 1; break;
        case 'n': dry_run = 1; break;
        case 'v': verbose = 1; break;
        case 'h': usage(stdout); return 0;
        default: usage(stderr); return 2;
        }
    }

    if (o.width < 1 || o.width > MAX_WIDTH || o.height < 8 || o.height > MAX_PAGES * 8
        || o.height % 8) {
        logerr("oledd: unsupported panel size %dx%d", o.width, o.height);
        return 2;
    }
    o.pages = o.height / 8;

    const OledFont *font = NULL;
    for (size_t i = 0; i < OLED_FONT_COUNT; ++i) {
        if (strcmp(oled_fonts[i].name, font_name) == 0)
            font = &oled_fonts[i];
    }
    if (!font) {
        logerr("oledd: no font named %s; available:", font_name);
        for (size_t i = 0; i < OLED_FONT_COUNT; ++i)
            logerr("  %s", oled_fonts[i].name);
        return 2;
    }
    logv("oledd: %dx%d on %s@0x%02x, font %s (%d columns x %d lines), watching %s",
         o.width, o.height, o.bus, o.addr, font->name, o.width / font->width, o.pages, path);

    struct sigaction sa = { 0 };
    sa.sa_handler = on_signal;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);

    char text[MAX_TEXT];
    /* One-shot modes: handy for checking the wiring before dogzillad feeds us. */
    if (literal || once) {
        if (literal)
            snprintf(text, sizeof(text), "%s", literal);
        else {
            const int got = read_frame(path, text, sizeof(text));
            if (got < 0) {
                logerr("oledd: %s: %s", path, strerror(errno));
                return 1;
            }
            if (got == 0)
                logerr("oledd: %s is empty", path);
        }
        render_text(&o, font, text);
        if (dry_run) {
            oled_dump(&o);
            return 0;
        }
        if (oled_open(&o) < 0 || oled_init(&o) < 0 || oled_flush(&o) < 0)
            return 1;
        close(o.fd);
        return 0;
    }

    /* Watch the directory, not the file: an atomic rename over the file (which
     * is how ConsoleDashboard publishes a frame) replaces the inode, and a
     * watch on the old inode would go deaf. */
    char dir[PATH_MAX], base[PATH_MAX];
    snprintf(dir, sizeof(dir), "%s", path);
    char *slash = strrchr(dir, '/');
    if (slash) {
        snprintf(base, sizeof(base), "%s", slash + 1);
        *slash = '\0';
        if (dir[0] == '\0')
            snprintf(dir, sizeof(dir), "/");
    } else {
        snprintf(base, sizeof(base), "%s", path);
        snprintf(dir, sizeof(dir), ".");
    }

    int ifd = inotify_init1(IN_NONBLOCK);
    int iwatch = -1;
    if (ifd < 0) {
        logerr("oledd: inotify_init: %s (falling back to polling)", strerror(errno));
    } else {
        iwatch = inotify_add_watch(ifd, dir,
                                   IN_CLOSE_WRITE | IN_MOVED_TO | IN_MODIFY | IN_CREATE);
        if (iwatch < 0)
            logerr("oledd: watch %s: %s (falling back to polling)", dir, strerror(errno));
    }

    struct timespec last_draw = { 0, 0 }, retry_at = { 0, 0 }, settle_until = { 0, 0 }, now;
    int dirty = 1;
    long backoff_ms = 0;

    while (!quit_requested) {
        now_mono(&now);

        if (dirty && elapsed_ms(&last_draw, &now) >= min_interval
            && elapsed_ms(&settle_until, &now) >= 0
            && (o.fd >= 0 || dry_run || elapsed_ms(&retry_at, &now) >= 0)) {
            const int got = read_frame(path, text, sizeof(text));
            if (got < 0)
                snprintf(text, sizeof(text), "oledd waiting for\n%.40s", base);
            if (got == 0) {
                /* Empty for the moment -- we caught the writer mid-rewrite,
                 * say: leave the display alone rather than blinking it off. */
                dirty = 0;
                continue;
            }
            render_text(&o, font, text);
            dirty = 0;
            last_draw = now;

            if (dry_run) {
                oled_dump(&o);
            } else {
                int ok = 1;
                if (o.fd < 0)
                    ok = oled_open(&o) == 0 && oled_init(&o) == 0;
                if (ok)
                    ok = oled_flush(&o) == 0;
                if (ok) {
                    backoff_ms = 0;
                } else {
                    /* The bus can be busy or the panel unplugged; keep trying,
                     * slower and slower, but never give up and never die. */
                    oled_close(&o, 0);
                    backoff_ms = backoff_ms ? (backoff_ms < 30000 ? backoff_ms * 2 : 30000) : 500;
                    deadline_in(&retry_at, &now, backoff_ms);
                    dirty = 1;
                    logv("oledd: retrying in %ld ms", backoff_ms);
                }
            }
        }

        int timeout = poll_sec > 0 ? poll_sec * 1000 : -1;
        if (dirty) {
            long wait = min_interval - elapsed_ms(&last_draw, &now);
            const long settle = elapsed_ms(&now, &settle_until);
            if (settle > wait)
                wait = settle;
            if (backoff_ms) {
                const long b = elapsed_ms(&now, &retry_at);
                if (b > wait)
                    wait = b;
            }
            timeout = wait > 0 ? (int)wait : 0;
        }

        struct pollfd pfd = { .fd = ifd, .events = POLLIN };
        int n = ifd >= 0 ? poll(&pfd, 1, timeout) : poll(NULL, 0, timeout);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            logerr("oledd: poll: %s", strerror(errno));
            break;
        }
        if (n == 0) {
            dirty = 1; /* periodic re-read, also recovers a lost notification */
            continue;
        }

        char evbuf[4096] __attribute__((aligned(__alignof__(struct inotify_event))));
        ssize_t len;
        while ((len = read(ifd, evbuf, sizeof(evbuf))) > 0) {
            for (char *e = evbuf; e < evbuf + len;) {
                const struct inotify_event *ev = (const struct inotify_event *)e;
                if (ev->len && strcmp(ev->name, base) == 0) {
                    dirty = 1;
                    /* Each further write pushes the deadline out, so we draw
                     * once the frame is complete rather than mid-frame. */
                    now_mono(&now);
                    deadline_in(&settle_until, &now, settle_ms);
                }
                e += sizeof(struct inotify_event) + ev->len;
            }
        }
    }

    logv("oledd: exiting");
    oled_close(&o, 1);
    if (ifd >= 0)
        close(ifd);
    return 0;
}
