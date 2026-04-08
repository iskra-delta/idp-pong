#!/usr/bin/env python3

import argparse
import math
import sys

import numpy as np
from skimage.draw import line as draw_line
from skimage.io import imread, imsave
from skimage.morphology import skeletonize, binary_dilation


NEIGHBORS_8 = [
    (-1, -1), (0, -1), (1, -1),
    (-1,  0),          (1,  0),
    (-1,  1), (0,  1), (1,  1),
]


def load_binary_image(path):
    img = imread(path)

    if img.ndim == 3:
        img = img[..., 0]

    img = np.asarray(img)

    if img.dtype == np.bool_:
        bw = img
    else:
        bw = img > 0

    return bw


def maybe_dilate(bw, dilate_steps):
    out = bw
    for _ in range(dilate_steps):
        out = binary_dilation(out)
    return out


def skeletonize_image(bw):
    return skeletonize(bw)


def pixel_neighbors(img, x, y):
    h, w = img.shape
    out = []

    for dx, dy in NEIGHBORS_8:
        nx = x + dx
        ny = y + dy

        if 0 <= nx < w and 0 <= ny < h and img[ny, nx]:
            out.append((nx, ny))

    return out


def build_graph(img):
    ys, xs = np.nonzero(img)
    nodes = set(zip(xs.tolist(), ys.tolist()))
    adj = {}

    for x, y in nodes:
        adj[(x, y)] = pixel_neighbors(img, x, y)

    return nodes, adj


def is_keypoint(p, adj):
    return len(adj[p]) != 2


def trace_edge(start, nxt, adj, visited_edges):
    path = [start, nxt]
    a = start
    b = nxt

    visited_edges.add(frozenset((a, b)))

    while True:
        if len(adj[b]) != 2:
            break

        n0, n1 = adj[b]
        c = n0 if n1 == a else n1

        edge = frozenset((b, c))
        if edge in visited_edges:
            break

        path.append(c)
        visited_edges.add(edge)
        a, b = b, c

    return path


def extract_paths(nodes, adj):
    keypoints = {p for p in nodes if is_keypoint(p, adj)}
    visited_edges = set()
    paths = []

    for kp in sorted(keypoints):
        for nb in adj[kp]:
            edge = frozenset((kp, nb))
            if edge in visited_edges:
                continue

            path = trace_edge(kp, nb, adj, visited_edges)
            if len(path) >= 2:
                paths.append(path)

    remaining = set()
    for p in nodes:
        for q in adj[p]:
            edge = frozenset((p, q))
            if edge not in visited_edges:
                remaining.add(edge)

    while remaining:
        edge = next(iter(remaining))
        a, b = tuple(edge)

        path = [a, b]
        visited_edges.add(edge)
        remaining.discard(edge)

        prev = a
        cur = b

        while True:
            nxts = [n for n in adj[cur] if n != prev]
            if not nxts:
                break

            nxt = nxts[0]
            e = frozenset((cur, nxt))

            if e in visited_edges:
                break

            path.append(nxt)
            visited_edges.add(e)
            remaining.discard(e)
            prev, cur = cur, nxt

            if cur == a:
                break

        if len(path) >= 2:
            paths.append(path)

    return paths


def perpendicular_distance(pt, a, b):
    x, y = pt
    x1, y1 = a
    x2, y2 = b

    if x1 == x2 and y1 == y2:
        return math.hypot(x - x1, y - y1)

    num = abs((y2 - y1) * x - (x2 - x1) * y + x2 * y1 - y2 * x1)
    den = math.hypot(x2 - x1, y2 - y1)

    if den == 0.0:
        return 0.0

    return num / den


def rdp(points, epsilon):
    if len(points) < 3:
        return points[:]

    start = points[0]
    end = points[-1]

    max_dist = -1.0
    index = -1

    for i in range(1, len(points) - 1):
        d = perpendicular_distance(points[i], start, end)
        if d > max_dist:
            max_dist = d
            index = i

    if max_dist > epsilon:
        left = rdp(points[:index + 1], epsilon)
        right = rdp(points[index:], epsilon)
        return left[:-1] + right

    return [start, end]


def paths_to_segments(paths, epsilon):
    segments = []

    for path in paths:
        simplified = rdp(path, epsilon)

        for i in range(len(simplified) - 1):
            x0, y0 = simplified[i]
            x1, y1 = simplified[i + 1]

            if x0 == x1 and y0 == y1:
                continue

            segments.append((x0, y0, x1, y1))

    return segments


def segment_length(seg):
    x0, y0, x1, y1 = seg
    return math.hypot(x1 - x0, y1 - y0)


def canonical_segment(seg):
    x0, y0, x1, y1 = seg
    a = (x0, y0)
    b = (x1, y1)
    if a <= b:
        return (x0, y0, x1, y1)
    return (x1, y1, x0, y0)


def deduplicate_segments(segments):
    seen = set()
    out = []

    for seg in segments:
        key = canonical_segment(seg)
        if key in seen:
            continue
        seen.add(key)
        out.append(seg)

    return out


def normalize(vx, vy):
    mag = math.hypot(vx, vy)
    if mag == 0.0:
        return (0.0, 0.0)
    return (vx / mag, vy / mag)


def segment_direction(seg):
    x0, y0, x1, y1 = seg
    return normalize(x1 - x0, y1 - y0)


def angle_between_segments_deg(a, b):
    ax, ay = segment_direction(a)
    bx, by = segment_direction(b)

    dot = ax * bx + ay * by
    dot = max(-1.0, min(1.0, dot))

    return math.degrees(math.acos(abs(dot)))


def point_distance(a, b):
    return math.hypot(b[0] - a[0], b[1] - a[1])


def segment_endpoints(seg):
    x0, y0, x1, y1 = seg
    return (x0, y0), (x1, y1)


def all_endpoint_pairs(seg_a, seg_b):
    a0, a1 = segment_endpoints(seg_a)
    b0, b1 = segment_endpoints(seg_b)

    return [
        (a0, b0),
        (a0, b1),
        (a1, b0),
        (a1, b1),
    ]


def line_coefficients(seg):
    x0, y0, x1, y1 = seg
    dx = x1 - x0
    dy = y1 - y0

    length = math.hypot(dx, dy)
    if length == 0.0:
        return None

    nx = -dy / length
    ny = dx / length
    c = -(nx * x0 + ny * y0)

    return nx, ny, c


def point_line_distance(point, seg):
    coeffs = line_coefficients(seg)
    if coeffs is None:
        return 0.0

    nx, ny, c = coeffs
    x, y = point
    return abs(nx * x + ny * y + c)


def segment_line_distance(seg_a, seg_b):
    a0, a1 = segment_endpoints(seg_a)
    b0, b1 = segment_endpoints(seg_b)

    d0 = point_line_distance(a0, seg_b)
    d1 = point_line_distance(a1, seg_b)
    d2 = point_line_distance(b0, seg_a)
    d3 = point_line_distance(b1, seg_a)

    return max(d0, d1, d2, d3)


def best_endpoint_gap(seg_a, seg_b):
    best = None
    for p, q in all_endpoint_pairs(seg_a, seg_b):
        d = point_distance(p, q)
        if best is None or d < best:
            best = d
    return best if best is not None else 0.0


def project_point_onto_axis(point, origin, axis):
    px = point[0] - origin[0]
    py = point[1] - origin[1]
    return px * axis[0] + py * axis[1]


def merge_two_segments(seg_a, seg_b):
    pts = [
        (seg_a[0], seg_a[1]),
        (seg_a[2], seg_a[3]),
        (seg_b[0], seg_b[1]),
        (seg_b[2], seg_b[3]),
    ]

    ax, ay = segment_direction(seg_a)
    if ax == 0.0 and ay == 0.0:
        ax, ay = segment_direction(seg_b)

    origin = pts[0]
    projected = [(project_point_onto_axis(p, origin, (ax, ay)), p) for p in pts]
    projected.sort(key=lambda item: item[0])

    p0 = projected[0][1]
    p1 = projected[-1][1]

    return (
        int(round(p0[0])), int(round(p0[1])),
        int(round(p1[0])), int(round(p1[1]))
    )


def can_merge(seg_a, seg_b, angle_thresh, gap_thresh, line_thresh):
    if segment_length(seg_a) == 0.0 or segment_length(seg_b) == 0.0:
        return False

    if angle_between_segments_deg(seg_a, seg_b) > angle_thresh:
        return False

    if best_endpoint_gap(seg_a, seg_b) > gap_thresh:
        return False

    if segment_line_distance(seg_a, seg_b) > line_thresh:
        return False

    return True


def merge_segments_once(segments, angle_thresh, gap_thresh, line_thresh):
    used = [False] * len(segments)
    out = []

    order = sorted(
        range(len(segments)),
        key=lambda i: segment_length(segments[i])
    )

    for idx in order:
        if used[idx]:
            continue

        current = segments[idx]
        used[idx] = True

        changed = True
        while changed:
            changed = False
            best_j = None
            best_gap = None

            for j in order:
                if used[j]:
                    continue

                other = segments[j]

                if can_merge(current, other,
                             angle_thresh, gap_thresh, line_thresh):
                    gap = best_endpoint_gap(current, other)

                    if best_gap is None or gap < best_gap:
                        best_gap = gap
                        best_j = j

            if best_j is not None:
                current = merge_two_segments(current, segments[best_j])
                used[best_j] = True
                changed = True

        out.append(current)

    return out


def merge_segments(segments,
                   angle_thresh=6.0,
                   gap_thresh=4.0,
                   line_thresh=1.2,
                   rounds=4):
    current = segments[:]

    for _ in range(rounds):
        before = len(current)
        current = merge_segments_once(
            current,
            angle_thresh=angle_thresh,
            gap_thresh=gap_thresh,
            line_thresh=line_thresh
        )
        current = deduplicate_segments(current)
        after = len(current)

        if after == before:
            break

    return current


def remove_short_segments(segments, min_length):
    return [seg for seg in segments if segment_length(seg) >= min_length]


def scale_segments_to_fit(segments, width, height, max_width, max_height):
    if not segments:
        return segments, width, height

    scale_x = max_width / width
    scale_y = max_height / height
    scale = min(scale_x, scale_y, 1.0)

    new_w = max(1, int(round(width * scale)))
    new_h = max(1, int(round(height * scale)))

    if scale == 1.0:
        return segments, width, height

    out = []
    for x0, y0, x1, y1 in segments:
        sx0 = int(round(x0 * scale))
        sy0 = int(round(y0 * scale))
        sx1 = int(round(x1 * scale))
        sy1 = int(round(y1 * scale))
        if sx0 == sx1 and sy0 == sy1:
            continue
        out.append((sx0, sy0, sx1, sy1))

    return deduplicate_segments(out), new_w, new_h


def rasterize_segments(segments, width, height):
    img = np.zeros((height, width), dtype=np.uint8)

    for x0, y0, x1, y1 in segments:
        rr, cc = draw_line(y0, x0, y1, x1)
        mask = (
            (rr >= 0) & (rr < height) &
            (cc >= 0) & (cc < width)
        )
        img[rr[mask], cc[mask]] = 255

    return img


def emit_c(segments, out):
    print("typedef struct { int x0, y0, x1, y1; } line_t;", file=out)
    print("", file=out)
    print("static const line_t lines[] = {", file=out)

    for x0, y0, x1, y1 in segments:
        print(f"    {{ {x0}, {y0}, {x1}, {y1} }},", file=out)

    print("};", file=out)
    print("", file=out)
    print("static const int line_count = sizeof(lines) / sizeof(lines[0]);",
          file=out)


def main():
    parser = argparse.ArgumentParser(
        description="Convert a PBM image into C line segments."
    )
    parser.add_argument("input", help="Input PBM file")
    parser.add_argument("--epsilon", type=float, default=1.5)
    parser.add_argument("--min-length", type=float, default=1.0)
    parser.add_argument("--merge-gap", type=float, default=4.0)
    parser.add_argument("--merge-angle", type=float, default=6.0)
    parser.add_argument("--merge-line", type=float, default=1.2)
    parser.add_argument("--merge-rounds", type=int, default=4)
    parser.add_argument("--max-width", type=int, default=1024)
    parser.add_argument("--max-height", type=int, default=512)
    parser.add_argument("--preview", default="new.png")
    parser.add_argument("--dilate", type=int, default=1)
    parser.add_argument("--no-skeletonize", action="store_true")
    args = parser.parse_args()

    bw = load_binary_image(args.input)

    if args.dilate > 0:
        bw = maybe_dilate(bw, args.dilate)

    if not args.no_skeletonize:
        bw = skeletonize_image(bw)

    height, width = bw.shape

    nodes, adj = build_graph(bw)
    paths = extract_paths(nodes, adj)
    segments = paths_to_segments(paths, args.epsilon)
    segments = deduplicate_segments(segments)
    segments = remove_short_segments(segments, args.min_length)
    segments = merge_segments(
        segments,
        angle_thresh=args.merge_angle,
        gap_thresh=args.merge_gap,
        line_thresh=args.merge_line,
        rounds=args.merge_rounds
    )
    segments = deduplicate_segments(segments)
    segments = remove_short_segments(segments, args.min_length)

    segments, out_w, out_h = scale_segments_to_fit(
        segments, width, height, args.max_width, args.max_height
    )

    preview = rasterize_segments(segments, out_w, out_h)
    imsave(args.preview, preview)

    emit_c(segments, sys.stdout)


if __name__ == "__main__":
    main()