#!/usr/bin/env python3
import sys
import re
from pathlib import Path
from collections import Counter

# Defaults (auto-tuned per file)
SYNC_MIN_US_DEFAULT = 10000
MIN_DUR_US = 40
MAX_DUR_US_FOR_SYMBOL = 2000
BIT_COUNT = 24


def parse_sub(path: Path):
    text = path.read_text()
    # Extract all RAW_Data lines and split numbers
    raw_lines = re.findall(r"RAW_Data:\s*(.*)", text)
    values = []
    for line in raw_lines:
        parts = line.strip().split()
        for p in parts:
            try:
                values.append(int(p))
            except ValueError:
                pass
    return values


def auto_sync_threshold(values):
    # Estimate sync threshold as 4x the 95th percentile of symbol durations
    abs_vals = [abs(v) for v in values if MIN_DUR_US <= abs(v) <= MAX_DUR_US_FOR_SYMBOL]
    if not abs_vals:
        return SYNC_MIN_US_DEFAULT
    abs_vals.sort()
    p95 = abs_vals[int(0.95 * (len(abs_vals) - 1))]
    return max(SYNC_MIN_US_DEFAULT, 4 * p95)


def segment_frames(values, sync_th):
    frames, frame = [], []
    for v in values:
        if abs(v) >= sync_th:
            if frame:
                frames.append(frame)
                frame = []
        else:
            frame.append(v)
    if frame:
        frames.append(frame)
    return frames


def kmeans_threshold(durations):
    # Simple 1D k-means (k=2) to split short/long
    data = [d for d in durations if MIN_DUR_US <= d <= MAX_DUR_US_FOR_SYMBOL]
    if len(data) < 20:
        return None
    c0, c1 = min(data), max(data)
    if c0 == c1:
        return None
    for _ in range(20):
        g0, g1 = [], []
        mid = (c0 + c1) / 2.0
        for d in data:
            (g0 if d < mid else g1).append(d)
        if not g0 or not g1:
            break
        new_c0 = sum(g0) / len(g0)
        new_c1 = sum(g1) / len(g1)
        if abs(new_c0 - c0) < 1 and abs(new_c1 - c1) < 1:
            c0, c1 = new_c0, new_c1
            break
        c0, c1 = new_c0, new_c1
    return (c0 + c1) / 2.0


def decode_with_threshold(pulses_abs, th, start_phase, invert_mapping, lsb_first):
    bits = []
    i = start_phase
    while i + 1 < len(pulses_abs) and len(bits) < BIT_COUNT:
        hi = pulses_abs[i]
        lo = pulses_abs[i + 1]
        hi_is_long = hi >= th
        lo_is_long = lo >= th
        if hi_is_long and not lo_is_long:
            bit = 1
        elif not hi_is_long and lo_is_long:
            bit = 0
        else:
            # Unclassifiable pair, slide by one
            i += 1
            continue
        if invert_mapping:
            bit ^= 1
        bits.append(bit)
        i += 2
    if len(bits) != BIT_COUNT:
        return None
    if lsb_first:
        bits = list(reversed(bits))
    val = 0
    for b in bits:
        val = (val << 1) | (1 if b else 0)
    return val


def try_decode_frame(frame, th):
    pulses_abs = [abs(x) for x in frame if MIN_DUR_US <= abs(x) <= MAX_DUR_US_FOR_SYMBOL or abs(x) < th]
    candidates = []
    for start_phase in (0, 1):
        for invert_mapping in (False, True):
            for lsb_first in (False, True):
                v = decode_with_threshold(pulses_abs, th, start_phase, invert_mapping, lsb_first)
                if v is not None:
                    candidates.append(v)
    return candidates


def main():
    if len(sys.argv) < 2:
        print("Usage: parse_flipper_sub.py <file.sub> [file2.sub ...]")
        sys.exit(1)
    totals = Counter()
    for arg in sys.argv[1:]:
        path = Path(arg)
        values = parse_sub(path)
        sync_th = auto_sync_threshold(values)
        frames = segment_frames(values, sync_th)
        print(f"File: {path.name} frames={len(frames)} sync_th≈{int(sync_th)}us")
        file_candidates = Counter()
        for idx, f in enumerate(frames):
            th = kmeans_threshold([abs(x) for x in f])
            if th is None:
                # fallback: median-based threshold
                dur = sorted([abs(x) for x in f if MIN_DUR_US <= abs(x) <= MAX_DUR_US_FOR_SYMBOL])
                th = dur[len(dur)//2] if dur else (MAX_DUR_US_FOR_SYMBOL // 2)
            cand = try_decode_frame(f, th)
            if cand:
                best, n = Counter(cand).most_common(1)[0]
                file_candidates[best] += 1
                print(f"  Frame {idx}: candidates={len(cand)} best=0x{best:06X} th≈{int(th)}")
            else:
                print(f"  Frame {idx}: no candidate")
        if file_candidates:
            best, n = file_candidates.most_common(1)[0]
            print(f"  File majority: 0x{best:06X} (seen {n} times)")
            totals[best] += n
        else:
            print("  No 24-bit majority for this file")
    if totals:
        best, n = totals.most_common(1)[0]
        print(f"Global majority: 0x{best:06X} (total {n})")
    else:
        print("No 24-bit candidate across inputs. Adjust thresholds or provide longer capture.")


if __name__ == "__main__":
    main()
