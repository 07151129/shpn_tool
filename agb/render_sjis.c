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
static void (*upload_glyph_obj)(const uint32_t*, uint16_t x, char y, uint32_t, uint32_t, uint16_t,
    char) = (void (*)(const uint32_t*, uint16_t, char, uint32_t, uint32_t, uint16_t, char))
        0x8006F91;

static uint16_t* new_keys = (void*)0x3002AE6;
static uint32_t* text_col = (uint32_t*)0x300234C;
static uint32_t* text_row = (uint32_t*)0x3002340;

struct oam_data {
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
};
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

#define DELAY_DEFAULT 3
#define NCOLS_PER_ROW 16
#define NROWS_MAX 7

#define TILE_DIM 8
#define NTILES_GLYPH 4

static void upload_glyph(const void* tiles, uint32_t x, uint32_t y, uint16_t xoffs, uint16_t yoffs) {
    volatile struct dma_cnt* dma3_cnt = (void*)0x40000DC;
    volatile uint32_t* dma3_src = (void*)0x40000D4;
    volatile uint32_t* dma3_dst = (void*)0x40000D8;

    uint32_t idx = x + y * NCOLS_PER_ROW;
    void* glyph_tiles_vram = (void*)(128 * idx + 0x800 + 0x600F800);

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
        .VPos = y * 14 + 15,
        .AffineMode = 0,
        .ObjMode = 0,
        .Mosaic = 0,
        .ColorMode = 0,
        .Shape = 0,
        .HPos = x * 14 + xoffs,
        .AffineParamNo_L = 0,
        .HFlip = 0,
        .VFlip = 0,
        .Size = 1,
        .CharNo = 4 * idx,
        .Priority = 0,
        .Pltt = 14,
        .AffineParam = 0
    };

    while (dma3_cnt->Enable)
        ;
    struct oam_data* oam_scratch = (void*)0x30024C0;
    oam_scratch[idx] = gly_obj;

    *dma3_src = (uint32_t)&oam_scratch[idx];
    *dma3_dst = (uint32_t)(0x7000000 + idx * sizeof(gly_obj));
    dma_cnt.cnt.Enable = 1;
    dma_cnt.cnt.Count = sizeof(struct oam_data) / sizeof(uint16_t);
    *dma3_cnt = dma_cnt.cnt;
}

__attribute__ ((noinline))
static
void render_sjis(const char* sjis, uint32_t len, uint16_t start_at_y, uint16_t color,
    bool no_delay, uint16_t a6, uint16_t a7) {
    (void)len;

    uint32_t buf[0x71 + 0x40];
    buf[0] = 0;
    buf[1] = 0xad;
    buf[2] = 0x15e;
    buf[3] = 0x1f4;
    buf[4] = 0;
    buf[5] = 0;
    buf[6] = 0x272;

    uint32_t col = 0, row = 0;

    if (start_at_y << 16)
        row = *text_row + 1;

    uint32_t delay = DELAY_DEFAULT;
    *text_col = 0;
    *text_row = 0;

    for (uint32_t i = 0; sjis[i];) {
        uint16_t csum = 0;

        uint32_t first = sjis[i] & UINT8_MAX;
        uint32_t second = sjis[i + 1] & UINT8_MAX;

        /* Prevent overflow */
        if (row == NROWS_MAX)
            break;

        /* Skip delay digit */
        if (first == 'W' && isdigit(second)) {
            delay = parse_wait_command(sjis, i);
            i += 2;
            continue;
        } else if (first == '\r') {
            col = 0;
            row++;
            i++;
            continue;
        } else if (first == ' ') {
            col++;
            i++;
            continue;
        }

        /* Automatic line wrap */
        if (col == NCOLS_PER_ROW) {
            col = 0;
            row++;
        }

        /* Interpret as ascii */
        if (0x21 <= first && first <= 0x7a) {
            uint16_t fw = hw_to_fw(first);
            first = fw >> 8;
            second = fw & UINT8_MAX;
            i++;
        } else
            i += 2;

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

        upload_glyph(&buf[0x47], col, row, a6, a7);

        *text_col = col;
        *text_row = row;

        col++;
    }
}

__attribute__ ((section(".entry")))
void render_sjis_entry(const char* sjis, uint32_t len, uint16_t start_at_y, uint16_t color,
    const bool no_delay, uint16_t a6, uint16_t a7) {
    render_sjis(sjis, len, start_at_y, color, no_delay, a6, a7);
}
