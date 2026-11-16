#!/usr/bin/env python3
"""Makes the pictures the installer shows (Installer/Assets) from the design, Installer.png.

    python3 Installer/Design/make-assets.py            (needs Pillow and numpy)

The design shows the installer's first page. The installer shows that picture itself, pixel
for pixel. For its other pages it needs the same picture without the first page's words and
buttons, which is what this script paints out: the dark panel behind them is black with a
faint diagonal weave, and the weave is continued through the gaps.

Written to Installer/Assets:

    picture.png          the window as designed: the design, cut to the window
    picture.sum          a sum over that picture's pixels, for the build to compare with
    patchText.png        laid over picture.png: the headline and the two lines under it are gone
    patchNoButtons.png   laid over that: both buttons are gone as well
    patchFinish.png      laid over that instead: the red button without its label, no Cancel

Where they go is stated in WINDOW, TEXT and BUTTONS below, and again in
Installer/Source/InstallerAssets.cpp.
"""
import os
import sys

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, '..', 'Assets')

# The part of the design that is the window: left, top, width, height in the design's pixels.
# The window is laid out in units of 980 x 386; the design has 1.8 pixels to each of them.
WINDOW = (58, 86, 1764, 695)

# What is painted out, as left, top, right, bottom in the window's pixels: the two blocks of
# text as rectangles, the buttons by their own shapes (the red one with the light around it).
# Two red lines of the frame fade out just above the Cancel button and have to stay.
HEADLINE = (846, 224, 1676, 372)
BODY     = (846, 392, 1500, 476)
NEXT     = (863, 552, 1257, 622)        # the red button; rounded by 14, lit up for 27 around
CANCEL   = (1295, 552, 1659, 622)       # the outlined button; rounded by 21
LABEL    = (992, 560, 1128, 614)        # on the red button: "Next" and its arrow

# The rectangles the patches cover
TEXT    = (HEADLINE[0], HEADLINE[1], HEADLINE[2], BODY[3])
BUTTONS = (834, 524, 1666, 650)


def rectangle(shape, box):
    left, top, right, bottom = box
    mask = np.zeros(shape, bool)
    mask[top:bottom, left:right] = True
    return mask


def rounded(shape, box, radius, grow):
    """A rectangle with round corners, made `grow` larger all round."""
    left, top, right, bottom = box
    left, top, right, bottom, radius = left - grow, top - grow, right + grow, bottom + grow, radius + grow
    y, x = np.mgrid[0:shape[0], 0:shape[1]]
    dx = np.maximum(np.maximum(left + radius - x, x - (right - 1 - radius)), 0)
    dy = np.maximum(np.maximum(top + radius - y, y - (bottom - 1 - radius)), 0)
    return (dx * dx + dy * dy <= radius * radius) & (x >= left) & (x < right) & (y >= top) & (y < bottom)


def clean_weave(picture):
    """Where the picture shows nothing but the panel's weave: no letters, lines or light."""
    r, g, b = picture[..., 0], picture[..., 1], picture[..., 2]
    something = (picture.max(axis=2) > 13) | (r - (g + b) / 2 > 5.5)

    # ...and nowhere near any of that either
    near = something.copy()
    for _ in range(7):
        grown = near.copy()
        grown[1:, :] |= near[:-1, :]; grown[:-1, :] |= near[1:, :]
        grown[:, 1:] |= near[:, :-1]; grown[:, :-1] |= near[:, 1:]
        near = grown

    clean = ~near
    clean[:, :806] = False          # the photograph
    clean[:22, :] = False           # the frame
    clean[674:, :] = False
    return clean


def continue_weave(picture, hole):
    """Fills the pixels marked in `hole` with the weave around them.

    The weave's stripes run down to the right, one pixel across for each pixel down, so a
    pixel in a gap takes its colour from further along its own stripe: from as far beyond the
    gap's edge as the pixel lies inside it.
    """
    height, width, _ = picture.shape
    source = clean_weave(picture) & ~hole
    result = picture.copy()
    unfilled = 0

    for y, x in zip(*np.where(hole)):
        found = []

        for step in (1, -1):
            # how far to the gap's edge along the stripe, and how long the clean run beyond it is
            depth = 1
            while True:
                px, py = x + step * depth, y + step * depth
                if not (0 <= px < width and 0 <= py < height):
                    depth = None
                    break
                if not hole[py, px]:
                    break
                depth += 1

            if depth is None:
                continue

            # (right at the gap's edge there may be the last of a button's outline or light:
            # the weave is taken from where it is clean again)
            for _ in range(12):
                px, py = x + step * depth, y + step * depth
                if not (0 <= px < width and 0 <= py < height) or source[py, px] or hole[py, px]:
                    break
                depth += 1

            run = 0
            while True:
                px, py = x + step * (depth + run), y + step * (depth + run)
                if not (0 <= px < width and 0 <= py < height) or not source[py, px]:
                    break
                run += 1

            if run >= 4:
                # back and forth along the run, so that no stretch of stripe repeats at once
                i = (depth - 1) % (2 * run - 2)
                i = i if i < run else 2 * run - 2 - i
                found.append((depth, picture[y + step * (depth + i), x + step * (depth + i)]))

        if len(found) == 2:
            (d1, c1), (d2, c2) = found
            # the nearer edge decides; where both are about as far, the two are mixed
            share = np.clip(0.5 + (d2 - d1) / 24.0, 0.0, 1.0)
            result[y, x] = share * c1 + (1.0 - share) * c2
        elif len(found) == 1:
            result[y, x] = found[0][1]
        else:
            unfilled += 1
            result[y, x] = (1.0, 1.0, 1.0)

    return result, unfilled


def without_label(picture):
    """The red button with "Next ->" taken off it: each row is drawn through from left to right."""
    left, top, right, bottom = LABEL
    result = picture.copy()
    part = picture[top:bottom, left:right]
    dark = part[..., 0] < 185.0                        # the label is black on red

    for _ in range(4):                                 # ...and its soft edge
        grown = dark.copy()
        grown[1:, :] |= dark[:-1, :]; grown[:-1, :] |= dark[1:, :]
        grown[:, 1:] |= dark[:, :-1]; grown[:, :-1] |= dark[:, 1:]
        dark = grown

    for row in range(bottom - top):
        columns = np.where(dark[row])[0]
        if len(columns) == 0:
            continue
        first, last = columns.min(), columns.max()
        a = picture[top + row, left + first - 6:left + first - 1].mean(axis=0)
        b = picture[top + row, left + last + 2:left + last + 7].mean(axis=0)
        for column in range(first - 1, last + 2):
            t = (column - (first - 1)) / max(1, (last + 1) - (first - 1))
            result[top + row, left + column] = (1.0 - t) * a + t * b

    return result


def save(array, box, name):
    left, top, right, bottom = box
    image = Image.fromarray(np.clip(np.rint(array[top:bottom, left:right]), 0, 255).astype('uint8'), 'RGB')
    path = os.path.normpath(os.path.join(OUT, name))
    image.save(path, optimize=True)
    print('%-20s %4d x %3d at (%d, %d)   %7d bytes' % (name, right - left, bottom - top, left, top, os.path.getsize(path)))


def main():
    design = Image.open(os.path.join(HERE, 'Installer.png')).convert('RGB')
    left, top, width, height = WINDOW
    picture = np.asarray(design.crop((left, top, left + width, top + height))).astype(float)

    shape = picture.shape[:2]
    red_button = rounded(shape, NEXT, 14, 27)
    cancel_button = rounded(shape, CANCEL, 21, 4)

    no_text, missed_a = continue_weave(picture, rectangle(shape, HEADLINE) | rectangle(shape, BODY))
    no_buttons, missed_b = continue_weave(no_text, red_button | cancel_button)
    finish, missed_c = continue_weave(without_label(no_text), cancel_button)

    if missed_a or missed_b or missed_c:
        sys.exit('pixels that found no weave to continue: %d, %d, %d' % (missed_a, missed_b, missed_c))

    os.makedirs(OUT, exist_ok=True)
    save(picture, (0, 0, width, height), 'picture.png')

    # A sum over the picture's pixels (all rows but the last, red, green, blue in turn). The
    # installer's self-test states the same sum over what it shows, and the build compares the
    # two: on every system the first page has to be made of exactly these pixels.
    total = 2166136261
    for value in np.rint(picture[:-1]).astype(int).reshape(-1).tolist():
        total = ((total ^ value) * 16777619) & 0xffffffff
    with open(os.path.join(OUT, 'picture.sum'), 'w') as file:
        file.write('%x\n' % total)
    print('%-20s %x' % ('picture.sum', total))
    save(no_text, TEXT, 'patchText.png')
    save(no_buttons, BUTTONS, 'patchNoButtons.png')
    save(finish, BUTTONS, 'patchFinish.png')

    if len(sys.argv) > 1:       # for looking at the result: the whole pictures, into a folder
        for name, array in (('1-picture', picture), ('2-no-text', no_text), ('3-no-buttons', no_buttons), ('4-finish', finish)):
            Image.fromarray(np.clip(np.rint(array), 0, 255).astype('uint8'), 'RGB').save(os.path.join(sys.argv[1], name + '.png'))


if __name__ == '__main__':
    main()
