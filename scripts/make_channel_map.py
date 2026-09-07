#!/usr/bin/env python3
"""Generate channel_map.csv (the analysis macros' map) from geometry/pads.csv.

analyzeUVW.C reads channel_map.csv from its working directory and needs one
row per readout channel:

    aget,raw_channel,view(0=U 1=V 2=W),strip,s_mm

s_mm is the position of the strip along the pitch direction of its view, in
DETECTOR coordinates: the dot product of the centre of any pad of that strip
with the unit pitch vector of the view. The pitch vectors are the ones of
TPCReco GeometryTPC (the same definition as make_pads.py):

    u_dir  = (cos a, sin a) with the strip angles U 90, V -30, W +30 degrees
    p_U = unit(-(u_W + u_V)), p_V = unit(u_U + u_W), p_W = unit(u_V - u_U)

pads.csv is written in SIMULATION coordinates, so the detector coordinates
are recovered with the transformation of its own header:

    x_det = x_sim ,  y_det = y_edge - (z_sim + 250)      (d = 0)

y_edge is read from that header, so this script carries no detector number of
its own and is safe to commit; its INPUT and OUTPUT are geometry derived and
stay local (both are gitignored). d, the distance from the entrance window to
the upstream pad edge, is 0 as long as it is not measured (TODO/06); pads.csv
made with d != 0 would need the same d subtracted here.

Usage: make_channel_map.py <pads.csv> <out.csv> [reference channel_map.csv]

With a reference the output is checked against it (mapping identical, strip
positions within 1e-3 mm) and a mismatch is a non-zero exit code.
"""
import math
import re
import sys

VIEW_INDEX = {'U': 0, 'V': 1, 'W': 2}
STRIP_ANGLE_DEG = {'U': 90.0, 'V': -30.0, 'W': 30.0}
Z_ENTRANCE_MM = -250.0
TOLERANCE_MM = 1e-3


def unit(v):
    n = math.hypot(*v)
    return (v[0] / n, v[1] / n)


def pitch_vectors():
    """The unit pitch vector of every view, in detector coordinates."""
    u = {k: (math.cos(math.radians(t)), math.sin(math.radians(t)))
         for k, t in STRIP_ANGLE_DEG.items()}
    return {'U': unit((-(u['W'][0] + u['V'][0]), -(u['W'][1] + u['V'][1]))),
            'V': unit((u['U'][0] + u['W'][0], u['U'][1] + u['W'][1])),
            'W': unit((u['V'][0] - u['U'][0], u['V'][1] - u['U'][1]))}


def read_pads(path):
    """(y_edge, {(aget, raw channel): (view letter, strip, x_det, y_det)})."""
    y_edge = None
    strips = {}
    for line in open(path):
        line = line.strip()
        if not line:
            continue
        if line.startswith('#'):
            # "#   x_sim = x_det ; z_sim = -250 + d + (48.930 - y_det)"
            m = re.search(r'\(\s*([-+0-9.eE]+)\s*-\s*y_det\s*\)', line)
            if m:
                y_edge = float(m.group(1))
            continue
        f = line.split(',')
        if f[0] == 'pad_id':  # header
            continue
        direction, strip, aget, raw = f[1], int(f[2]), int(f[4]), int(f[5])
        cx, cz = float(f[7]), float(f[8])
        # All pads of one strip give the same s, so the first one is enough.
        strips.setdefault((aget, raw),
                          (direction, strip, cx, y_edge - (cz - Z_ENTRANCE_MM)))
    if y_edge is None:
        sys.exit(f"{path}: no '(<y_edge> - y_det)' line in the header")
    if not strips:
        sys.exit(f"{path}: no pads")
    return y_edge, strips


def read_channel_map(path):
    rows = {}
    for line in open(path):
        line = line.strip()
        if not line or line.startswith('#'):
            continue
        aget, raw, view, strip, s = line.split(',')
        rows[(int(aget), int(raw))] = (int(view), int(strip), float(s))
    return rows


def main():
    if len(sys.argv) not in (3, 4):
        sys.exit(__doc__)
    pads_path, out_path = sys.argv[1], sys.argv[2]
    reference_path = sys.argv[3] if len(sys.argv) > 3 else None

    y_edge, strips = read_pads(pads_path)
    pitch = pitch_vectors()

    rows = {}
    for (aget, raw), (direction, strip, x_det, y_det) in strips.items():
        p = pitch[direction]
        rows[(aget, raw)] = (VIEW_INDEX[direction], strip,
                             x_det * p[0] + y_det * p[1])

    with open(out_path, 'w') as out:
        out.write("# aget,raw_channel,view(0=U 1=V 2=W),strip,s_mm\n")
        for aget, raw in sorted(rows):
            view, strip, s = rows[(aget, raw)]
            out.write(f"{aget},{raw},{view},{strip},{s:.3f}\n")
    print(f"{out_path}: {len(rows)} channels from {pads_path} "
          f"(y_edge = {y_edge:.3f}, d = 0)")

    if reference_path is None:
        return
    reference = read_channel_map(reference_path)
    if set(reference) != set(rows):
        sys.exit(f"{reference_path}: {len(reference)} channels, "
                 f"{out_path}: {len(rows)}, and the sets differ")
    worst, worst_channel, mismatches = 0.0, None, 0
    for channel, (view, strip, s) in sorted(reference.items()):
        got = rows[channel]
        if got[0] != view or got[1] != strip:
            mismatches += 1
            print(f"aget {channel[0]} channel {channel[1]}: view/strip "
                  f"{got[0]}/{got[1]}, reference {view}/{strip}")
        # The reference and the output are both rounded to 1e-3 mm.
        if abs(got[2] - s) > worst:
            worst, worst_channel = abs(got[2] - s), channel
    print(f"{len(reference)} channels checked against {reference_path}: "
          f"{mismatches} mapping mismatch(es), largest |ds| = {worst:.2e} mm "
          f"at aget {worst_channel[0]} channel {worst_channel[1]}")
    if mismatches or worst > TOLERANCE_MM:
        sys.exit(1)


if __name__ == '__main__':
    main()
