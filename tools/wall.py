# renders every tile in web/wall/scripts.txt (name: KEY KEY ...) and lays them out into one
# image, web/wall/wall-1x.png and wall-2x.png, the background on the showcase page. 8 x 8
# slots with half a gap round each so it tiles without a seam. the page picks 1x or 2x by
# the screen. squashed to 8 bit after, it's like 6 colours. was svgs once, pixels as rects
# look bad shrunk
#
#   python3 tools/wall.py

import os, random, subprocess, sys, tempfile
from PIL import Image, ImageDraw

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
APP = os.path.join(ROOT, "build-pc", "fluxion-app")
WALL = os.path.join(ROOT, "web", "wall")


def tiles():
    with open(os.path.join(WALL, "scripts.txt")) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            name, keys = line.split(":", 1)
            yield name.strip(), keys.split()


COLS, ROWS = 8, 8
BEZEL, GAP, ROUND = 5, 14, 7  # at 1x, the 2x one doubles them


def render(keys, scale):
    # history lives in ./build-pc/store, so a fresh dir each time or tiles leak into each other
    with tempfile.TemporaryDirectory() as tmp:
        os.makedirs(os.path.join(tmp, "build-pc", "store"))
        png = os.path.join(tmp, "tile.png")
        subprocess.run([APP, "--png", png, "--scale", str(scale)] + keys, cwd=tmp, check=True,
                       stdout=subprocess.DEVNULL)
        return Image.open(png).convert("RGB")


def compose(scale):
    bezel, gap = BEZEL * scale, GAP * scale
    slot_w, slot_h = 320 * scale + 2 * bezel + gap, 240 * scale + 2 * bezel + gap
    screens = [render(keys, scale) for _, keys in tiles()]
    # same shuffle every run (and both sizes) so the image only changes when a tile does
    random.Random(84).shuffle(screens)
    wall = Image.new("RGBA", (COLS * slot_w, ROWS * slot_h), (0, 0, 0, 0))
    draw = ImageDraw.Draw(wall)
    for i in range(COLS * ROWS):
        x, y = i % COLS * slot_w + gap // 2, i // COLS * slot_h + gap // 2
        draw.rounded_rectangle([x, y, x + slot_w - gap - 1, y + slot_h - gap - 1], ROUND * scale, fill="#1c1c1f")
        wall.paste(screens[i % len(screens)], (x + bezel, y + bezel))
    out = os.path.join(WALL, f"wall-{scale}x.png")
    wall.save(out)
    subprocess.run(["magick", out, "-strip", "PNG8:" + out], check=True)
    print(f"{out}: {len(screens)} screens, {wall.width} x {wall.height}, {os.path.getsize(out)} bytes")


if __name__ == "__main__":
    compose(1)  # plain screens and phones
    compose(2)  # high dpi
