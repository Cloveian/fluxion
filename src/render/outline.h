#ifndef FLUXION_OUTLINE_H
#define FLUXION_OUTLINE_H

/* antialiased fill of an outline: the glyphs, and the brackets and roots that have to grow. desktop only, one at a time.
   begin, any number of lines and curves (closed paths, the winding gives the holes), then end hands back the coverage */

void outline_begin(int w, int h);
void outline_line(float x0, float y0, float x1, float y1);
void outline_quad(float x0, float y0, float cx, float cy, float x1, float y1);
/* alpha gets w x h bytes, 0 to 255 */
void outline_end(unsigned char *alpha);

#endif
