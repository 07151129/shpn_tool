#include <limits.h>
#include <stdalign.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static uint32_t (*parse_wait_command)(const char* buf, uint32_t idx) =
    (uint32_t (*)(const char*, uint32_t))0x8004D85;
static void (*sub_8004CD4)(uint32_t idx, uint32_t* buf) = (void (*)(uint32_t, uint32_t*))0x8004CD5;
static void (*sub_8004C34)(uint32_t*, uint32_t*, char) = (void (*)(uint32_t*, uint32_t*, char))0x8004C35;
static void (*await_input)(void) = (void (*)(void))0x80130A1;

static uint16_t* new_keys = (void*)0x3002AE6;
static uint32_t* cursor_col = (uint32_t*)0x300234C;
static uint32_t* cursor_row = (uint32_t*)0x3002340;

static struct oam_data {
    uint32_t VPos:8;             // Y Coordinate
    uint32_t AffineMode:2;       // Affine Mode
    uint32_t ObjMode:2;          // OBJ Mode
    uint32_t Mosaic:1;           // Mosaic
    uint32_t ColorMode:1;        // 16 colors/256 colors Select
    uint32_t Shape:2;            // OBJ Shape

    uint32_t HPos:9;             // X Coordinate
    uint32_t AffineParamNo_L:3;  // Affine Transformation Parameter No.  Lower 3 bits
    uint32_t HFlip:1;            // H Flip
    uint32_t VFlip:1;            // V Flip
    uint32_t Size:2;             // OBJ Size

    uint16_t CharNo:10;          // Character No.
    uint16_t Priority:2;         // Display priority
    uint16_t Pltt:4;             // Palette No.
    uint16_t AffineParam;        // Affine Trasnformation Parameter
} * oam_base = (void*)0x7000000;
_Static_assert(sizeof(struct oam_data) == sizeof(uint64_t), "");

struct dma_cnt {
    uint16_t Count;              // Transfer Count
    uint16_t Dummy_21_16:5;
    uint16_t DestpCnt:2;         // Destination Address Control
    uint16_t SrcpCnt:2;          // Source Address Control
    uint16_t ContinuousON:1;     // Continuous Mode
    uint16_t BusSize:1;          // Bus Size 16/32Bit Select
    uint16_t DataRequest:1;      // Data Request Synchronize Mode
    uint16_t Timming:2;          // Timing Select
    uint16_t IF_Enable:1;        // Interrupt Request Enable
    uint16_t Enable:1;           // DMA Enable
};
_Static_assert(sizeof(struct dma_cnt) == sizeof(uint32_t), "");

static bool isdigit(char c) {
    return '0' <= c && c <= '9';
}

static uint16_t hw_to_fw(char c) {
    if ('a' <= c && c <= 'z')
        return 0x8281 + c - 'a';
    if ('A' <= c && c <= 'Z')
        return 0x8260 + c - 'A';
    if (isdigit(c))
        return 0x824f + c - '0';
    switch (c) {
        case '!': return 0x8149;
        case '?': return 0x8148;
        case '&': return 0x8195;
        case '(': return 0x8169;
        case ')': return 0x816a;
        case ',': return 0x8143;
        case '.': return 0x8144;
        case '-': return 0x815d;
    }
    return c;
}

struct glyph_margins {
    uint8_t lmargin;
    uint8_t rmargin;
};

static struct glyph_margins margins_az[] = {
    {2, 5}, {2, 5}, {2, 5}, {2, 5}, {2, 5}, {3, 6}, {2, 5}, {2, 5}, {5, 8}, {3, 6}, {3, 5}, /* k */
    {4, 8}, {1, 4}, {2, 5}, {2, 5}, {2, 5}, {2, 5}, {4, 6}, {2, 5}, {3, 5}, {2, 5}, {2, 5}, /* v */
    {1, 4}, {2, 5}, {2, 5}, {2, 5} /* z */
};
_Static_assert(sizeof(margins_az) / sizeof(*margins_az) == 'z' - 'a' + 1, "");

static struct glyph_margins margins_AZ[] = {
    {1, 4}, {2, 5}, {2, 4}, {2, 4}, {2, 5}, {2, 5}, {2, 4}, {2, 5}, {5, 8}, {2, 5}, {2, 4}, /* K */
    {2, 5}, {1, 4}, {2, 4}, {2, 4}, {2, 5}, {2, 4}, {2, 5}, {2, 5}, {1, 4}, {2, 4}, {1, 4}, /* V */
    {1, 4}, {1, 4}, {1, 4}, {2, 5} /* Z */
};
_Static_assert(sizeof(margins_AZ) / sizeof(*margins_AZ) == 'Z' - 'A' + 1, "");

static struct glyph_margins margins_cyr_lo[] = {
    {2, 5}, {2, 5}, {2, 5}, {2, 6}, {1, 5}, {2, 5}, {2, 5}, {1, 4}, {2, 5}, {2, 5}, {2, 5}, /* й */
    {2, 6}, {2, 5}, {1, 4}, {2, 5}, {2, 5}, {2, 5}, {2, 5}, {2, 5}, {2, 5}, {3, 5}, {0, 3}, /* ф */
    {2, 5}, {1, 5}, {2, 5}, {1, 4}, {0, 5}, {1, 4}, {1, 5}, {2, 5}, {2, 5}, {1, 4}, {2, 5} /* я */
};
_Static_assert(sizeof(margins_cyr_lo) / sizeof(*margins_cyr_lo) == 33, "");

static struct glyph_margins margins_cyr_cap[] = {
    {1, 4}, {2, 5}, {2, 5}, {2, 6}, {1, 5}, {2, 5}, {2, 5}, {0, 3}, {2, 5}, {1, 5}, {1, 5}, /* Й */
    {2, 5}, {2, 5}, {1, 4}, {2, 5}, {1, 5}, {2, 5}, {2, 5}, {1, 5}, {2, 5}, {2, 5}, {1, 4}, /* Ф */
    {2, 5}, {1, 5}, {2, 5}, {1, 4}, {0, 4}, {1, 4}, {1, 4}, {2, 5}, {1, 5}, {0, 4}, {2, 5} /* Я */
};
_Static_assert(sizeof(margins_cyr_cap) / sizeof(*margins_cyr_cap) == 33, "");

static struct glyph_margins margins_digit[] = {
    {2, 5}, {5, 8}, {2, 5}, {2, 5}, {2, 4}, {2, 6}, {2, 5}, {2, 5}, {2, 5}, {2, 5}
};
_Static_assert(sizeof(margins_digit) / sizeof(*margins_digit) == '9' - '0' + 1, "");

#define NCOLS_PER_ROW 30

#define TILE_DIM 8
#define NTILES_GLYPH 4
#define GLYPH_DIM 16

#define TEXT_LMARGIN (GLYPH_DIM)
#define TEXT_RMARGIN (240 - TEXT_LMARGIN - GLYPH_DIM)

#define SPACE_W 6
#define VSPACE 14
#define TEXT_UMARGIN 15

#define CURSOR_OAM_IDX 112

static struct glyph_margins glyph_margin(uint16_t c) {
    if ('a' <= c && c <= 'z')
        return margins_az[c - 'a'];
    if ('A' <= c && c <= 'Z')
        return margins_AZ[c - 'A'];
    if (/* a */ 0x8470 <= c && c <= 0x8491 /* я */)
        return margins_cyr_lo[c - 0x8470];
    if (/* А */ 0x8440 <= c && c <= 0x8460 /* Я */)
        return margins_cyr_cap[c - 0x8440];
    if (isdigit(c))
        return margins_digit[c - '0'];
    switch (c) {
        case '!': return (struct glyph_margins){5, 8};
        case '?': return (struct glyph_margins){2, 6};
        case '&': return (struct glyph_margins){2, 5};
        case '(': return (struct glyph_margins){7, 4};
        case ')': return (struct glyph_margins){1, 8};
        case ',': return (struct glyph_margins){0, 12};
        case '.': return (struct glyph_margins){1, 11};
        case '-': return (struct glyph_margins){3, 6};
    }

    return (struct glyph_margins){0, 0};
}

static uint8_t upload_glyph(const void* tiles, uint32_t idx, uint32_t row, uint16_t xoffs,
    uint16_t yoffs, uint8_t rmargin_prev, uint8_t lmargin) {
    volatile struct dma_cnt* dma3_cnt = (void*)0x40000DC;
    volatile uint32_t* dma3_src = (void*)0x40000D4;
    volatile uint32_t* dma3_dst = (void*)0x40000D8;

    /* Cursor steals OAM slot 112... */
    if (idx >= CURSOR_OAM_IDX)
        idx++;

    void* glyph_tiles_vram = (void*)(TILE_DIM * TILE_DIM * sizeof(uint16_t) * idx + 0x800 +
        0x600F800);

    while (dma3_cnt->Enable)
        ;

    union {
        struct dma_cnt cnt;
        uint32_t val;
    } dma_cnt;
    dma_cnt.val = 0;

    dma_cnt.cnt.Enable = 1;
    dma_cnt.cnt.Count = TILE_DIM * TILE_DIM * NTILES_GLYPH / sizeof(uint16_t);

    *dma3_src = (uint32_t)tiles;
    *dma3_dst = (uint32_t)glyph_tiles_vram;
    *dma3_cnt = dma_cnt.cnt;

    struct oam_data gly_obj = {
        .VPos = row * VSPACE + TEXT_UMARGIN + yoffs,
        .AffineMode = 0,
        .ObjMode = 0,
        .Mosaic = 0,
        .ColorMode = 0,
        .Shape = 0,
        .AffineParamNo_L = 0,
        .HFlip = 0,
        .VFlip = 0,
        .Size = 1,
        .CharNo = 4 * idx,
        .Priority = 0,
        .Pltt = 14,
        .AffineParam = 0
    };

    gly_obj.HPos = TEXT_LMARGIN;
    if (xoffs > TEXT_LMARGIN)
        gly_obj.HPos = xoffs - lmargin - rmargin_prev;

    /* FIXME: Is it faster to have tiles uploaded asynchronously and copy gly_obj manually? */
    while (dma3_cnt->Enable)
        ;

    *dma3_src = (uint32_t)&gly_obj;
    *dma3_dst = (uint32_t)&oam_base[idx];
    dma_cnt.cnt.Enable = 1;
    dma_cnt.cnt.Count = sizeof(struct oam_data) / sizeof(uint16_t);
    *dma3_cnt = dma_cnt.cnt;

    while (dma3_cnt->Enable)
        ;

    return gly_obj.HPos + GLYPH_DIM;
}

#define TILE_SZ 0x20
#define DELAY_DEFAULT 3
#define NCHARS_MAX (128 - 2 /* Stolen by cursor */)

__attribute__ ((noinline))
static
uint8_t render_sjis(const char* sjis, uint32_t len, uint16_t start_at_y, uint16_t color,
    bool no_delay, uint16_t xoffs, uint16_t yoffs, uint8_t nchars_offs) {
    (void)len;

    /* FIXME: Is there any good reason why the original code can draw only 112 glyphs? */
    /* Hide glyphs past the cursor */
    for (unsigned i = CURSOR_OAM_IDX + 1; i < NCHARS_MAX + 2; i++)
        oam_base[i].AffineMode = 2; /* Hide */

    uint32_t buf[0x71 + 0x40];
    buf[0] = 0;
    buf[1] = 0xad;
    buf[2] = 0x15e;
    buf[3] = 0x1f4;
    buf[4] = 0;
    buf[5] = 0;
    buf[6] = 0x272;

    uint32_t col = 0, row = 0;

    if (start_at_y)
        row = *cursor_row + 1;

    uint32_t delay = DELAY_DEFAULT;

    uint8_t rmargin_prev = 0, xpos_prev = TEXT_LMARGIN;
    unsigned nchars = nchars_offs;

    for (uint32_t i = 0; sjis[i];) {
        uint16_t csum = 0;

        uint32_t first = sjis[i] & UINT8_MAX;
        uint32_t second = sjis[i + 1] & UINT8_MAX;
        struct glyph_margins margins = {0, 0};

        /* Skip delay digit */
        if (first == 'W' && isdigit(second)) {
            delay = parse_wait_command(sjis, i);
            i += 2;
            continue;
        } else if (first == '\r') {
            col = 0;
            row++;
            i++;
            xpos_prev = TEXT_LMARGIN;
            continue;
        } else if (first == ' ') {
            i++;
            xpos_prev += SPACE_W;
            continue;
        }

        /* Automatic line wrap */
        if (xpos_prev >= TEXT_RMARGIN) {
            col = 0;
            xpos_prev = TEXT_LMARGIN;
            row++;
        }

        /* Prevent overflow */
        if (nchars > NCHARS_MAX)
            break;

        /* Interpret as ascii */
        if (0x21 <= first && first <= 0x7a) {
            margins = glyph_margin(first);

            uint16_t fw = hw_to_fw(first);
            first = fw >> 8;
            second = fw & UINT8_MAX;
            i++;
        } else {
            margins = glyph_margin((first << 8) | second);
            i += 2;
        }

        uint32_t offs_first = 0;
        uint32_t offs_second = second - 0x40;

        if (offs_second > 0x3f)
            offs_second = second - 0x41;

        if (first - 0x88 > 0x17) {
            if (first - 0x81 <= 0x5e)
                offs_first = buf[first - 0x81];
            else
                offs_first = 0x140c + 0xbc * (first - 0xe0) + 4;
        } else
            offs_first = 0xbc * (first - 0x88) + 0x270;
        csum = offs_first + offs_second;

        /* On button press, render the rest of the text without delay */
        while (delay-- && !no_delay) {
            await_input();
            if (*new_keys & 1 || *new_keys & 0x100) {
                no_delay = true;
                break;
            }
        }
        delay = DELAY_DEFAULT;

        sub_8004CD4(csum, &buf[7]);
        sub_8004C34(&buf[7], &buf[0x47], color);

        if (col == 0) {
            rmargin_prev = 0;
            xpos_prev = xoffs + TEXT_LMARGIN;
        }

        xpos_prev = upload_glyph(&buf[0x47], nchars, row, xpos_prev, yoffs, rmargin_prev,
            margins.lmargin);

        rmargin_prev = margins.rmargin;

        nchars++;

        col++;
    }

    /* Cursor is drawn at coordinates for fixed-width spacing... */
#define CURSOR_COL 13
#define CURSOR_ROW 8

    *cursor_col = CURSOR_COL;
    *cursor_row = CURSOR_ROW;

    return nchars;
}

__attribute__ ((section(".entry")))
void render_sjis_entry(const char* sjis, uint32_t len, uint16_t start_at_y, uint16_t color,
    uint16_t no_delay, uint16_t a6, uint16_t a7) {
    render_sjis(sjis, len, start_at_y, color, no_delay, a6, a7, 0);
}

__attribute__ ((section(".entry_menu")))
void render_sjis_menu_entry(const char* sjis, uint32_t unused, uint32_t row, uint32_t chosen_row,
    uint16_t no_delay) {
    (void)unused;

    /* FIXME */

    uint32_t color = 15;
    if (row == chosen_row)
        color = 9;

    *cursor_row = row;
    render_sjis(sjis, 0, true, color, no_delay, 0, 0, 0);
}
