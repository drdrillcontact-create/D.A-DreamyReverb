#!/usr/bin/env python3
"""Looks at a photograph of the screen with the installer's window on it, and says whether the
window is see-through around its frame.

During its self-test the installer has a plain sheet of one colour behind its window (see
SelfTest in Installer/Source/InstallerMain.cpp). The window is a rectangle, the frame drawn in
it a slightly smaller one with its corners cut off. Between the two, where the corners are cut
off, and along the window's own edge, the photograph has to show the sheet: the very colour it
shows beside the window. Inside the frame it has to show the window.

With <design.png>, the picture the first page is made of (Installer/Assets/picture.png, which
fills the window), it also says whether the photograph shows that picture within the frame:
both are divided into small squares, and the colours of the squares are compared.

Usage: look-at-screenshot.py <photograph.png> <sheet colour, rrggbb>
                             <window: x y width height> <frame: x y width height> <cut>
                             [<design.png> [<margin>]]

The window and the frame are given as the installer's log gives them ("self-test: window at",
"self-test: frame at"): in the screen's units, the cut being what is taken off each side of
the frame at a corner. The photograph is the window and <margin> more on every side, as
capture-screen.sh takes it (24 unless stated).

Needs nothing but Python 3. Exit code 0: it is as it should be; 1: it is not; 2: the photograph
cannot be judged (which is then said, and is no verdict on the window).
"""
import struct
import sys
import zlib


def read_png(path):
    """The picture as (width, height, rows); each row a list of (r, g, b)."""
    with open(path, "rb") as f:
        data = f.read()

    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("not a PNG file")

    at, packed, header = 8, b"", None

    while at + 8 <= len(data):
        length, kind = struct.unpack(">I4s", data[at:at + 8])
        body = data[at + 8:at + 8 + length]
        at += 12 + length

        if kind == b"IHDR":
            header = struct.unpack(">IIBBBBB", body)
        elif kind == b"IDAT":
            packed += body
        elif kind == b"IEND":
            break

    if header is None:
        raise ValueError("a PNG file without a header")

    width, height, depth, colour, _, _, interlaced = header
    channels = {2: 3, 6: 4}.get(colour)

    if channels is None or depth not in (8, 16) or interlaced:
        raise ValueError("a kind of PNG this script does not read (colour type %d, %d bits%s)"
                         % (colour, depth, ", interlaced" if interlaced else ""))

    step = depth // 8                   # bytes to a sample (of 16 bits, the first is the one that counts)
    pixel = channels * step             # bytes to a pixel
    line = width * pixel
    raw = zlib.decompress(packed)

    if len(raw) < height * (line + 1):
        raise ValueError("a PNG file that ends too soon")

    rows, before = [], bytearray(line)

    for y in range(height):
        start = y * (line + 1)
        how = raw[start]
        row = bytearray(raw[start + 1:start + 1 + line])

        if how == 1:
            for i in range(pixel, line):
                row[i] = (row[i] + row[i - pixel]) & 255
        elif how == 2:
            for i in range(line):
                row[i] = (row[i] + before[i]) & 255
        elif how == 3:
            for i in range(line):
                left = row[i - pixel] if i >= pixel else 0
                row[i] = (row[i] + ((left + before[i]) >> 1)) & 255
        elif how == 4:
            for i in range(line):
                a = row[i - pixel] if i >= pixel else 0
                b = before[i]
                c = before[i - pixel] if i >= pixel else 0
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                row[i] = (row[i] + (a if pa <= pb and pa <= pc else (b if pb <= pc else c))) & 255
        elif how != 0:
            raise ValueError("a PNG file with a row filter of an unknown kind")

        rows.append([(row[i], row[i + step], row[i + 2 * step]) for i in range(0, line, pixel)])
        before = row

    return width, height, rows


def median(values):
    ordered = sorted(values)
    return ordered[len(ordered) // 2] if ordered else 0


def compare_with_design(rows, design_path, window, outside_frame):
    """How the photograph, within the frame, compares with the picture the first page is made
    of. Returns a line to print and whether it is a failure."""
    try:
        design_width, design_height, design = read_png(design_path)
    except (OSError, ValueError, IndexError, struct.error, zlib.error) as problem:
        return "compare: [SKIP] the design's picture could not be read: %s" % problem, False

    square = 8                                              # pixels of the photograph, each way
    to_design = design_width / (window[2] - window[0])      # the design's pixels to one of the photograph's
    differences = []

    y = int(window[1]) + 1
    while y + square <= window[3]:
        x = int(window[0]) + 1
        while x + square <= window[2]:
            # (only squares that lie well within the frame)
            if max(outside_frame(cx, cy) for cx in (x, x + square) for cy in (y, y + square)) <= -3.0:
                shown = [0, 0, 0]
                for row in rows[y:y + square]:
                    for p in row[x:x + square]:
                        shown[0] += p[0]; shown[1] += p[1]; shown[2] += p[2]

                left, right = int(round((x - window[0]) * to_design)), int(round((x + square - window[0]) * to_design))
                top, bottom = int(round((y - window[1]) * to_design)), int(round((y + square - window[1]) * to_design))
                right, bottom = min(right, design_width), min(bottom, design_height)
                meant, count = [0, 0, 0], max(1, (right - left) * (bottom - top))

                for row in design[top:bottom]:
                    for p in row[left:right]:
                        meant[0] += p[0]; meant[1] += p[1]; meant[2] += p[2]

                differences.append(max(abs(shown[i] / float(square * square) - meant[i] / float(count)) for i in range(3)))

            x += square
        y += square

    if len(differences) < 500:
        return "compare: [SKIP] the photograph is too small to be compared with the design  (%d squares)" % len(differences), False

    average, worst = sum(differences) / len(differences), max(differences)
    far_out = sum(1 for d in differences if d > 70.0)
    numbers = "in %d squares of %d by %d pixels within the frame, the colours differ by %.1f on average and %.0f at most (of 255)" \
              % (len(differences), square, square, average, worst)

    # Something else than the design shows as squares that are far out, or as a large
    # difference everywhere. (A photograph that gives all colours a little differently, as
    # one taken through a screen's colour profile does, is neither.)
    if average > 20.0 or far_out > 0.01 * len(differences):
        return "compare: [FAIL] on the screen, the first page is not the design: %s; %d squares are far out" % (numbers, far_out), True

    if average <= 5.0 and worst <= 40.0:
        return "compare: [PASS] on the screen, the first page is the design: " + numbers, False

    return "compare: [NOTE] the photograph gives the first page's colours a little differently from the design: " + numbers, False


def main(arguments):
    if len(arguments) not in (11, 12, 13):
        print(__doc__)
        return 2

    path, sheet = arguments[0], arguments[1].lstrip("#")
    wx, wy, ww, wh, fx, fy, fw, fh, cut = [float(a) for a in arguments[2:11]]
    design_path = arguments[11] if len(arguments) >= 12 else ""
    margin = float(arguments[12]) if len(arguments) == 13 else 24.0

    try:
        sheet = tuple(int(sheet[i:i + 2], 16) for i in (0, 2, 4))
        width, height, rows = read_png(path)
    except (OSError, ValueError, IndexError, struct.error, zlib.error) as problem:
        print("compare: [SKIP] the photograph of the first page could not be read: %s" % problem)
        return 2

    # What the photograph shows of the screen: the window and the margin around it, as far as
    # the screen goes to the left and upwards. (A screen may have several pixels to the unit.)
    left, top = max(0.0, wx - margin), max(0.0, wy - margin)
    across = wx + ww + margin - left
    k = width / across if across > 0 else 1.0

    def to_picture(x, y):
        return (x - left) * k, (y - top) * k

    window = to_picture(wx, wy) + to_picture(wx + ww, wy + wh)
    x0, y0 = to_picture(fx, fy)
    x1, y1 = to_picture(fx + fw, fy + fh)
    c = cut * k

    # The frame is what is left of its rectangle when the corners are cut off: on the right
    # side of all eight lines. Each line as (a, b, d): a*x + b*y - d is how far outside it a
    # point is, in pixels.
    r = 0.5 ** 0.5
    lines = [(-1.0, 0.0, -x0), (1.0, 0.0, x1), (0.0, -1.0, -y0), (0.0, 1.0, y1),
             (-r, -r, -r * (x0 + y0 + c)), (r, -r, r * (x1 - y0 - c)),
             (r, r, r * (x1 + y1 - c)), (-r, r, r * (-x0 + y1 - c))]

    def outside_frame(x, y):
        return max(a * x + b * y - d for a, b, d in lines)

    beside, around, within = [], [], []

    for y in range(height):
        row = rows[y]
        py = y + 0.5

        for x in range(width):
            px = x + 0.5
            in_window = min(px - window[0], window[2] - px, py - window[1], window[3] - py)

            if in_window <= -3.0:
                beside.append(row[x])                   # beside the window: the sheet, if it is there
            elif in_window >= 24.0 * k + c:
                within.append(row[x])                   # (far inside: certainly within the frame)
            else:
                out = outside_frame(px, py)

                # In the window but outside its frame. The window's own edge counts, and
                # the three pixels beyond it: a line drawn along that edge is to be found,
                # also should the window lie a pixel or two from where it says it is.
                if out >= 2.0:
                    around.append(row[x])
                elif out <= -3.0:
                    within.append(row[x])

    if len(beside) < 200 or len(around) < 200 or len(within) < 2000:
        print("compare: [SKIP] the photograph of the first page does not show the window with room around it"
              "  (%d x %d pixels; %d beside the window, %d around its frame, %d within it)"
              % (width, height, len(beside), len(around), len(within)))
        return 2

    seen = tuple(median([p[i] for p in beside]) for i in range(3))
    name = "%02x%02x%02x" % seen

    def is_sheet(p):
        return all(abs(p[i] - seen[i]) <= 6 for i in range(3))

    # (The photograph may give colours a little differently from how they were asked for; it
    # has to be the sheet all the same: light, and close to its colour.)
    if any(abs(seen[i] - sheet[i]) > 40 for i in range(3)) or sum(1 for p in beside if is_sheet(p)) < 0.9 * len(beside):
        print("compare: [SKIP] the photograph does not show the sheet behind the window: beside the window it is mostly %s,"
              " and the sheet is %02x%02x%02x" % ((name,) + sheet))
        return 2

    let_through = sum(1 for p in around if is_sheet(p))
    looks_like_sheet = sum(1 for p in within if is_sheet(p))
    darkest = min(around, key=sum)
    ok = True

    if let_through >= 0.998 * len(around):
        print("compare: [PASS] on the screen, nothing of the window is seen around its frame: of %d pixels from the frame to just"
              " beyond the window's own edge, %d show the sheet behind it (%s)" % (len(around), let_through, name))
    else:
        ok = False
        print("compare: [FAIL] on the screen, something of the window is seen around its frame: of %d pixels from the frame to just"
              " beyond the window's own edge, only %d show the sheet behind it (%s); the darkest there is %02x%02x%02x"
              % ((len(around), let_through, name) + darkest))

    if looks_like_sheet <= 0.02 * len(within):
        print("compare: [PASS] and within the frame it is the window that is seen  (%d of %d pixels there look like the sheet)"
              % (looks_like_sheet, len(within)))
    else:
        ok = False
        print("compare: [FAIL] within the frame, the photograph does not show the window: %d of %d pixels there look like the"
              " sheet behind it" % (looks_like_sheet, len(within)))

    if design_path:
        line, failed = compare_with_design(rows, design_path, window, outside_frame)
        print(line)
        ok = ok and not failed

    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
