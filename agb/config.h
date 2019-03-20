#ifndef CONFIG_H
#define CONFIG_H

/* All dimensions are in pixels */

/* Glyph dimension */
#define RENDER_GLYPH_DIM 16

/* Text left margin */
#define RENDER_TEXT_LMARGIN (RENDER_GLYPH_DIM)
/* Text right margin */
#define RENDER_TEXT_RMARGIN (240 - RENDER_TEXT_LMARGIN - RENDER_GLYPH_DIM)
/* Text upper margin */
#define RENDER_TEXT_UMARGIN 15

/* Horizontal space width */
#define RENDER_SPACE_W 6
/* Vertical space width */
#define RENDER_VSPACE 14

/* Delay in vertical blanking periods before next glyph is rendered */
#define RENDER_DELAY_DEFAULT 3
/* Amount of OAM objects used, 1 reserved for the cursor */
#define RENDER_NCHARS_MAX (128 - 2 /* Stolen by cursor */)

_Static_assert(RENDER_NCHARS_MAX <= 126, "Too many glyphs to render");

#endif
