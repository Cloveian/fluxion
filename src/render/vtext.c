#include "vtext.h"

#include "outline.h"
#include "vfonts.h"
#include "platform/screen.h"

#ifdef SCREEN_RESIZABLE

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* a TrueType reader for the four subset faces in vfonts.c: simple glyphs, cmap format 4, no hinting, no kerning. what it
   doesn't know it draws as nothing */

enum { F_REGULAR, F_BOLD, F_OBLIQUE, F_BOLD_OBLIQUE, F_COUNT };

typedef struct {
    const unsigned char *loca, *glyf, *hmtx, *cmap;
    int units, loca_long, hmetrics, segs;
} face_t;

static face_t faces[F_COUNT];
static bool faces_ready;

static unsigned u16(const unsigned char *p) { return (unsigned)p[0] << 8 | p[1]; }
static int s16(const unsigned char *p) { return (short)u16(p); }
static unsigned long u32(const unsigned char *p) { return (unsigned long)u16(p) << 16 | u16(p + 2); }

static const unsigned char *table(const unsigned char *d, const char *tag) {
    int n = (int)u16(d + 4), i;

    for(i = 0; i < n; i++) {
        const unsigned char *r = d + 12 + 16 * i;

        if(memcmp(r, tag, 4) == 0)
            return d + u32(r + 8);
    }
    return NULL;
}

static bool load(face_t *f, const unsigned char *d) {
    const unsigned char *head = table(d, "head"), *hhea = table(d, "hhea"), *cmap = table(d, "cmap");
    int n, i;

    f->glyf = table(d, "glyf");
    f->loca = table(d, "loca");
    f->hmtx = table(d, "hmtx");
    f->cmap = NULL;
    if(head == NULL || hhea == NULL || cmap == NULL || f->glyf == NULL || f->loca == NULL || f->hmtx == NULL)
        return false;
    f->units = (int)u16(head + 18);
    f->loca_long = s16(head + 50);
    f->hmetrics = (int)u16(hhea + 34);
    n = (int)u16(cmap + 2);
    for(i = 0; i < n && f->cmap == NULL; i++) { /* the unicode one, format 4 */
        const unsigned char *r = cmap + 4 + 8 * i, *sub = cmap + u32(r + 4);

        if((u16(r) == 3 && u16(r + 2) == 1) && u16(sub) == 4)
            f->cmap = sub;
    }
    if(f->cmap == NULL || f->units == 0 || f->hmetrics == 0)
        return false;
    f->segs = (int)u16(f->cmap + 6) / 2;
    return true;
}

static bool load_all(void) {
    if(!faces_ready)
        faces_ready = load(&faces[F_REGULAR], vface_regular) && load(&faces[F_BOLD], vface_bold)
                      && load(&faces[F_OBLIQUE], vface_oblique) && load(&faces[F_BOLD_OBLIQUE], vface_bold_oblique);
    return faces_ready;
}

static int glyph_of(const face_t *f, unsigned code) {
    const unsigned char *end = f->cmap + 14, *start = end + 2 * f->segs + 2, *delta = start + 2 * f->segs, *range = delta + 2 * f->segs;
    int i;

    for(i = 0; i < f->segs; i++) {
        unsigned off;

        if(code > u16(end + 2 * i))
            continue;
        if(code < u16(start + 2 * i))
            return 0;
        off = u16(range + 2 * i);
        if(off == 0)
            return (int)((code + u16(delta + 2 * i)) & 0xFFFF);
        off = u16(range + 2 * i + off + 2 * (code - u16(start + 2 * i)));
        return off == 0 ? 0 : (int)((off + u16(delta + 2 * i)) & 0xFFFF);
    }
    return 0;
}

static const unsigned char *glyph_data(const face_t *f, int g, int *len) {
    unsigned long a, b;

    if(f->loca_long) {
        a = u32(f->loca + 4 * g);
        b = u32(f->loca + 4 * g + 4);
    } else {
        a = 2UL * u16(f->loca + 2 * g);
        b = 2UL * u16(f->loca + 2 * g + 2);
    }
    *len = (int)(b - a);
    return f->glyf + a;
}

/* font units of advance */
static int advance_units(const face_t *f, int g) {
    return (int)u16(f->hmtx + 4 * (g < f->hmetrics ? g : f->hmetrics - 1));
}

typedef struct {
    float x, y;
    bool on;
} pt_t;

#define PTS_MAX 256

/* one closed contour as lines and quadratic cirves. off-curve points in a row have the on-curve one halfway between them */
static void contour(const pt_t *p, int m) {
    pt_t start, cur;
    int i = 0, stop = m;

    if(m < 2)
        return;
    if(p[0].on) {
        start = p[0];
        i = 1;
    } else if(p[m - 1].on) {
        start = p[m - 1];
        stop = m - 1;
    } else {
        start.x = (p[0].x + p[m - 1].x) / 2;
        start.y = (p[0].y + p[m - 1].y) / 2;
        start.on = true;
    }
    cur = start;
    while(i < stop) {
        if(p[i].on) {
            outline_line(cur.x, cur.y, p[i].x, p[i].y);
            cur = p[i++];
        } else if(i + 1 < stop && !p[i + 1].on) {
            pt_t mid;

            mid.x = (p[i].x + p[i + 1].x) / 2;
            mid.y = (p[i].y + p[i + 1].y) / 2;
            outline_quad(cur.x, cur.y, p[i].x, p[i].y, mid.x, mid.y);
            cur = mid;
            i++;
        } else {
            pt_t next = i + 1 < stop ? p[i + 1] : start;

            outline_quad(cur.x, cur.y, p[i].x, p[i].y, next.x, next.y);
            cur = next;
            i += 2;
        }
    }
    outline_line(cur.x, cur.y, start.x, start.y);
}

/* glyph g as outline calls, s pixels to a font unit, y up in the font and down here, (ox, oy) the origin. false when it's empty or too big */
static bool trace(const face_t *f, int g, float s, float ox, float oy) {
    int len, nc, n, i, first, c;
    const unsigned char *d = glyph_data(f, g, &len), *ends, *p;
    unsigned char fl[PTS_MAX];
    pt_t pts[PTS_MAX];
    int v;

    if(len < 12 || (nc = s16(d)) <= 0)
        return false;
    ends = d + 10;
    n = (int)u16(ends + 2 * (nc - 1)) + 1;
    if(n > PTS_MAX)
        return false;
    p = d + 12 + 2 * nc + u16(ends + 2 * nc);
    for(i = 0; i < n;) {
        unsigned char fv = *p++;

        fl[i++] = fv;
        if(fv & 8) {
            int rep = *p++;

            while(rep-- > 0 && i < n)
                fl[i++] = fv;
        }
    }
    for(v = 0, i = 0; i < n; i++) {
        if(fl[i] & 2) {
            int dx = *p++;

            v += (fl[i] & 16) ? dx : -dx;
        } else if(!(fl[i] & 16)) {
            v += s16(p);
            p += 2;
        }
        pts[i].x = ox + (float)v * s;
        pts[i].on = (fl[i] & 1) != 0;
    }
    for(v = 0, i = 0; i < n; i++) {
        if(fl[i] & 4) {
            int dy = *p++;

            v += (fl[i] & 32) ? dy : -dy;
        } else if(!(fl[i] & 32)) {
            v += s16(p);
            p += 2;
        }
        pts[i].y = oy - (float)v * s;
    }
    for(first = 0, c = 0; c < nc; c++) {
        int last = (int)u16(ends + 2 * c);

        contour(pts + first, last - first + 1);
        first = last + 1;
    }
    return true;
}

/* rendered glyphs, one slot each by a hash of what they were drawn as */
typedef struct {
    int face, size, code, sub; /* size is the em in real pixels times 16, sub a quarter pixel across */
    int w, h, left, top;      /* the alpha's size and where its corner is from the pen and the baseline */
    unsigned char *a;
    bool used;
} cell_t;

#define CELLS 1024
static cell_t cells[CELLS];

static const cell_t *glyph(int face, float em_px, unsigned code, int sub) {
    int size = (int)lroundf(em_px * 16), slot = (int)((code * 7u + (unsigned)face * 131u + (unsigned)sub * 17u + (unsigned)size * 13u) & (CELLS - 1));
    cell_t *c = &cells[slot];
    const face_t *f = &faces[face];
    float s = em_px / (float)f->units, sx = (float)sub / 4;
    int g, len, x0, x1, y0, y1;
    const unsigned char *d;

    if(c->used && c->face == face && c->size == size && c->code == (int)code && c->sub == sub)
        return c;
    free(c->a);
    memset(c, 0, sizeof *c);
    c->used = true;
    c->face = face;
    c->size = size;
    c->code = (int)code;
    c->sub = sub;
    g = glyph_of(f, code);
    d = glyph_data(f, g, &len);
    if(g == 0 || len < 12)
        return c;
    x0 = (int)floorf((float)s16(d + 2) * s + sx) - 1;
    x1 = (int)ceilf((float)s16(d + 6) * s + sx) + 1;
    y0 = (int)floorf(-(float)s16(d + 8) * s) - 1;
    y1 = (int)ceilf(-(float)s16(d + 4) * s) + 1;
    c->w = x1 - x0;
    c->h = y1 - y0;
    c->left = x0;
    c->top = y0;
    c->a = calloc((size_t)c->w * (size_t)c->h, 1);
    if(c->a == NULL)
        return c;
    outline_begin(c->w, c->h);
    if(trace(f, g, s, sx - (float)x0, -(float)y0))
        outline_end(c->a);
    return c;
}

/* Fluxion's codes (layout.h, editor.h) to Unicode, a space for none. e and i of the constants are bold whatever the style */
static unsigned unicode_of(unsigned char c, font_style_t *style) {
    switch(c) {
    case 1: return 0x3C0;
    case 2: return 0x3B8;
    case 3: return 0x2212;
    case 4: return 0xB7;
    case 5: *style |= STYLE_BOLD; return 'e';
    case 6: *style |= STYLE_BOLD; return 'i';
    case 0x10: return 0x2264;
    case 0x11: return 0x2265;
    case 0x12: return 0x2260;
    case 0x13: return 0x221E;
    case 0x14: return 0xB0;
    case 0x15: return 0x2192;
    case 0x16: return 0xB1;
    case 0x17: return 0xF7;
    case 0x18: return 0xD7;
    case 0x19: return 0x222B;
    default: return c >= 32 && c < 127 ? c : ' ';
    }
}

/* one character: its face and code, and the advnace in the em it's drawn at */
static float step(unsigned char ch, font_style_t style, float em, int *face, unsigned *code) {
    const face_t *f;
    bool b, i;

    *code = unicode_of(ch, &style);
    b = (style & STYLE_BOLD) != 0;
    i = (style & STYLE_ITALIC) != 0;
    *face = b ? (i ? F_BOLD_OBLIQUE : F_BOLD) : (i ? F_OBLIQUE : F_REGULAR);
    f = &faces[*face];
    return (float)advance_units(f, glyph_of(f, *code)) * em / (float)f->units;
}

typedef struct {
    font_t base; /* first, so a font_t pointer of ours is also one of these */
    float em;    /* layout pixels */
} vfont_t;

#define VFONTS 8
static vfont_t vfonts[VFONTS];
static int vfont_count;

const font_t *vtext_font(float em, int ascent, int descent) {
    int i;
    vfont_t *v;

    if(!load_all())
        return NULL;
    for(i = 0; i < vfont_count; i++)
        if(vfonts[i].em == em && vfonts[i].base.ascent == ascent && vfonts[i].base.descent == descent)
            return &vfonts[i].base;
    if(vfont_count == VFONTS)
        return NULL;
    v = &vfonts[vfont_count++];
    v->base.name = "dejavu sans";
    v->base.ascent = (unsigned char)ascent;
    v->base.descent = (unsigned char)descent;
    v->em = em;
    return &v->base;
}

bool vtext_owns(const font_t *font) {
    int i;

    for(i = 0; i < vfont_count; i++)
        if(font == &vfonts[i].base)
            return true;
    return false;
}

int vtext_width(const char *text, const font_t *font, font_style_t style) {
    const vfont_t *v = (const vfont_t *)font;
    float w = 0;

    for(; *text != '\0'; text++) {
        unsigned code;
        int face;

        w += step((unsigned char)*text, style, v->em, &face, &code);
    }
    return (int)lroundf(w);
}

void vtext_draw(int x, int top, const char *text, const font_t *font, font_style_t style) {
    const vfont_t *v = (const vfont_t *)font;
    float em_px = v->em * screen_ppu(), pen = (float)x * screen_ppu();
    int baseline = screen_real((float)(top + font->ascent));

    for(; *text != '\0'; text++) {
        unsigned code;
        int face, whole = (int)floorf(pen);
        float adv = step((unsigned char)*text, style, em_px, &face, &code);

        if(code != ' ') {
            const cell_t *c = glyph(face, em_px, code, (int)((pen - (float)whole) * 4));

            if(c->a != NULL)
                screen_mask(whole + c->left, baseline + c->top, c->w, c->h, c->a, c->w);
        }
        pen += adv;
    }
}

#else
typedef int vtext_unused; /* the calculators and the web page keep the bitmap fonts */
#endif
