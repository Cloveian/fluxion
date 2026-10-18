# the readme logo, ƒlµxion out of archivo extra bold (same as the page's heading) turned into
# plain paths, github won't load a web font inside an svg. one per github theme, the readme
# picks with <picture>
#
#   python3 tools/logo.py 'Archivo[wdth,wght].ttf'
#   (the font's in github.com/google/fonts, ofl/archivo, OFL so outlines in a logo are fine)

import os, sys
from fontTools.ttLib import TTFont
from fontTools.varLib.instancer import instantiateVariableFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen
from fontTools.pens.boundsPen import BoundsPen

TEXT = "ƒlµxion"
MU_AT = TEXT.index("µ")
# text colour and the µ's pink (the page's palette), the light one's deeper so it holds up on white
THEMES = {"dark": ("#e6e2ee", "#f5a9c9"), "light": ("#1f2328", "#c94f86")}

font = instantiateVariableFont(TTFont(sys.argv[1]), {"wght": 800, "wdth": 100})
glyphs, cmap, hmtx = font.getGlyphSet(), font.getBestCmap(), font["hmtx"]
tracking = -0.02 * font["head"].unitsPerEm   # the heading's letter-spacing

ink, mu, bounds = SVGPathPen(glyphs), SVGPathPen(glyphs), BoundsPen(glyphs)
x = 0
for i, ch in enumerate(TEXT):
    name = cmap[ord(ch)]
    flip = (1, 0, 0, -1, x, 0)   # font y goes up, svg y goes down
    glyphs[name].draw(TransformPen(mu if i == MU_AT else ink, flip))
    glyphs[name].draw(TransformPen(bounds, flip))
    x += hmtx[name][0] + tracking

x0, y0, x1, y1 = bounds.bounds
pad = 20
box = f"{x0 - pad:.0f} {y0 - pad:.0f} {x1 - x0 + 2 * pad:.0f} {y1 - y0 + 2 * pad:.0f}"
media = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "media")
for theme, (text_colour, mu_colour) in THEMES.items():
    with open(os.path.join(media, f"logo-{theme}.svg"), "w") as f:
        f.write(f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="{box}" role="img" aria-label="Fluxion">'
                f'<path fill="{text_colour}" d="{ink.getCommands()}"/>'
                f'<path fill="{mu_colour}" d="{mu.getCommands()}"/></svg>\n')
    print(f"media/logo-{theme}.svg")
