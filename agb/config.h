#ifndef CONFIG_H
#define CONFIG_H

/* All dimensions are in pixels */

/* Glyph dimension */
#define RENDER_GLYPH_DIM 16

/* Text left margin */
#define RENDER_TEXT_LMARGIN (RENDER_GLYPH_DIM)
/* Text right margin */
#define RENDER_TEXT_RMARGIN (240 - RENDER_TEXT_LMARGIN)
/* Text upper margin */
#define RENDER_TEXT_UMARGIN 15

/* Horizontal space width */
#define RENDER_SPACE_W 6
/* Vertical space width */
#define RENDER_VSPACE 14

/* Coordinates of cursor as if glyphs were fixed-width */
#define RENDER_CURSOR_COL 14
#define RENDER_CURSOR_ROW 8

/* Delay in vertical blanking periods before next glyph is rendered */
#define RENDER_DELAY_DEFAULT 3
#define RENDER_DELAY_MENU 10
/* Amount of OAM objects used, 1 reserved for the cursor */
#define RENDER_NCHARS_MAX (128 - 2 /* Stolen by cursor */)

_Static_assert(RENDER_NCHARS_MAX <= 126, "Too many glyphs to render");

/* Force wrap line if it does not fit */
#define RENDER_AUTO_WRAP 1

/* Approximate number of rows that fit per frame */
#define RENDER_NROWS_MAX 7

#endif
