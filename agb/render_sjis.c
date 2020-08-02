#include <limits.h>
#include <stdalign.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "config.h"
#include "glyph_margins.h"

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

bool isdigit(char c) {
    return '0' <= c && c <= '9';
}

#define TILE_DIM 8
#define NTILES_GLYPH 4

#define CURSOR_OAM_IDX 112

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
        .VPos = row * RENDER_VSPACE + RENDER_TEXT_UMARGIN + yoffs,
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

    gly_obj.HPos = RENDER_TEXT_LMARGIN;
    if (xoffs > RENDER_TEXT_LMARGIN)
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

    return gly_obj.HPos + RENDER_GLYPH_DIM;
}

__attribute__ ((noinline))
static
uint8_t render_sjis(const char* sjis, uint32_t len, uint16_t start_at_y, uint16_t color,
    bool no_delay, uint16_t xoffs, uint16_t yoffs, uint8_t nchars_offs, uint32_t* nbreaksp) {
    (void)len;

    /* FIXME: Is there any good reason why the original code can draw only 112 glyphs? */

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

    uint32_t delay = RENDER_DELAY_DEFAULT;

    uint8_t rmargin_prev = 0, xpos_prev = RENDER_TEXT_LMARGIN;
    unsigned nchars = nchars_offs;
    unsigned nbreaks = 0;

    bool in_quotes = false;

    for (uint32_t i = 0; sjis[i];) {
        uint16_t csum = 0;

        uint32_t first = sjis[i] & UINT8_MAX;
        uint32_t second;
        struct glyph_margins margins = {0, 0};

        /* Skip delay digit */
        if (glyph_is_wait_cmd(&sjis[i])) {
            delay = parse_wait_command(sjis, i);
            i += 2;
            continue;
        } else if (first == '\r') {
            /* The strtab decoder replaces \n with \r */
            col = 0;
            row++;
            i++;
            nbreaks++;
            xpos_prev = RENDER_TEXT_LMARGIN;
            continue;
        } else if (first == ' ') {
            i++;
            xpos_prev += RENDER_SPACE_W;
            continue;
        }

        /* Prevent overflow */
        if (nchars > RENDER_NCHARS_MAX)
            break;

        if (glyph_is_hw(first)) { /* Known single-byte half-width */
            margins = glyph_margin(first, in_quotes);

            uint16_t fw = glyph_hw_to_fw(first, in_quotes);
            first = fw >> 8;
            second = fw & UINT8_MAX;

            if (sjis[i] == '"')
                in_quotes = !in_quotes;

            i++;
        } else if (sjis[i+1]) { /* Try to interpret as two-byte full-width */
            second = sjis[i + 1] & UINT8_MAX;
            margins = glyph_margin((first << 8) | second, in_quotes);
            i += 2;
        } else { /* Unknown char; skip */
            i++;
            continue;
        }

#ifdef RENDER_AUTO_WRAP
        /* Automatic line wrap */
        if (xpos_prev - RENDER_GLYPH_DIM + margins.lmargin +
            (RENDER_GLYPH_DIM - margins.lmargin - margins.rmargin) - rmargin_prev
                >= RENDER_TEXT_RMARGIN) {
            col = 0;
            xpos_prev = RENDER_TEXT_LMARGIN;
            row++;
            nbreaks++;
        }
#endif

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
        delay = RENDER_DELAY_DEFAULT;

        sub_8004CD4(csum, &buf[7]);
        sub_8004C34(&buf[7], &buf[0x47], color);

        if (col == 0) {
            rmargin_prev = 0;
            xpos_prev = xoffs + RENDER_TEXT_LMARGIN;
        }

        xpos_prev = upload_glyph(&buf[0x47], nchars, row, xpos_prev, yoffs, rmargin_prev,
            margins.lmargin);

        rmargin_prev = margins.rmargin;

        nchars++;

        col++;
    }

    if (nbreaksp)
        *nbreaksp += nbreaks;

    return nchars;
}

/**
 * FIXME: Investigate random glyph corruption when rendering backlog here.
 */
__attribute__ ((section(".entry")))
void render_sjis_entry(const char* sjis, uint32_t len, uint16_t start_at_y, uint16_t color,
    uint16_t no_delay, uint16_t a6, uint16_t a7) {
    /**
     * HACK: Looks like start_at_y=1 iff. we're rendering backlog. For some reason, for choice
     * operands render_sjis is called twice for a single line, so try to ignore the second
     * (pointless) call, which might also corrupt the image.
     */
    if (start_at_y)
        return;
    render_sjis(sjis, len, 0, color, no_delay, a6, a7, 0, NULL);

    /* Cursor is drawn at coordinates for fixed-width spacing... */

    *cursor_col = RENDER_CURSOR_COL;
    *cursor_row = RENDER_CURSOR_ROW;
}

__attribute__ ((section(".entry_menu")))
void render_sjis_menu_entry(const char* sjis, uint32_t unused, uint32_t row, uint32_t chosen_row,
    uint16_t no_delay) {
    (void)unused;

    uint32_t* nchars_rendered = (uint32_t*)0x3002134;

    /**
     * HACK: We don't draw a cursor for choice so this variable can be safely reused for storing
     * OAM offset
     */
    if (row == 0) {
        *cursor_col = 0;
        *cursor_row = UINT32_MAX;

        /* Do not render empty pretext */
        if (sjis[0] == ' ') {
            *(uint32_t*)nchars_rendered = 0;
            return;
        }
    }

    uint32_t color = 15;
    if (row == chosen_row)
        color = 9;

    *cursor_col = render_sjis(sjis, 0, true, color, no_delay, 0, 0, *cursor_col, cursor_row);
    *(uint32_t*)nchars_rendered = *cursor_col;
}

__attribute__ ((section(".clear_oam")))
void clear_oam() {
    for (unsigned i = 0; i < RENDER_NCHARS_MAX + 2; i++)
        oam_base[i].AffineMode = 2; /* Hide */
}

__attribute__ ((section(".render_backlog_controls")))
void render_backlog_controls(uint32_t arg) {
    (void)arg;
}
