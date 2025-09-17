#!/usr/bin/env python3
import sys
import re
from pathlib import Path
from collections import Counter
import statistics

def parse_sub(path: Path):
    text = path.read_text()
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

def analyze_timing_pattern(values):
    """Analyze timing patterns in CAME signal"""
    groups = []
    current_sign = None
    current_duration = 0
    
    for v in values:
        sign = 1 if v > 0 else -1
        if sign == current_sign:
            current_duration += abs(v)
        else:
            if current_sign is not None:
                groups.append((current_sign, current_duration))
            current_sign = sign
            current_duration = abs(v)
    
    if current_sign is not None:
        groups.append((current_sign, current_duration))
    
    return groups

def find_sync_gaps(groups, min_sync_us=20000):
    """Find sync gaps"""
    sync_positions = []
    for i, (sign, duration) in enumerate(groups):
        if duration >= min_sync_us:
            sync_positions.append(i)
    return sync_positions

def analyze_bit_patterns(groups, sync_positions):
    """Analyze bit patterns in detail"""
    all_patterns = []
    
    for sync_idx in sync_positions[:3]:  # Analyze first 3 sync positions
        i = sync_idx + 1  # Skip sync
        bit_count = 0
        patterns = []
        
        while i + 1 < len(groups) and bit_count < 24:
            high_dur = groups[i][1]
            low_dur = groups[i + 1][1]
            
            # Skip if too long (next sync)
            if high_dur > 15000 or low_dur > 15000:
                break
                
            patterns.append((high_dur, low_dur))
            all_patterns.append((high_dur, low_dur))
            
            i += 2
            bit_count += 1
        
        if len(patterns) == 24:
            print(f"\nSync position {sync_idx} - 24-bit pattern:")
            for j, (h, l) in enumerate(patterns):
                print(f"  Bit {j:2d}: HIGH={h:3d}µs, LOW={l:3d}µs, Total={h+l:3d}µs")
    
    return all_patterns

def cluster_timings(patterns):
    """Cluster timings to find short/long patterns"""
    high_durs = [p[0] for p in patterns]
    low_durs = [p[1] for p in patterns]
    total_durs = [p[0] + p[1] for p in patterns]
    
    print(f"\nTiming analysis:")
    print(f"High durations: min={min(high_durs)}, max={max(high_durs)}, median={statistics.median(high_durs):.0f}")
    print(f"Low durations:  min={min(low_durs)}, max={max(low_durs)}, median={statistics.median(low_durs):.0f}")
    print(f"Total durations: min={min(total_durs)}, max={max(total_durs)}, median={statistics.median(total_durs):.0f}")
    
    # Find clusters using percentiles
    high_25 = statistics.quantiles(high_durs, n=4)[0]
    high_75 = statistics.quantiles(high_durs, n=4)[2]
    low_25 = statistics.quantiles(low_durs, n=4)[0]
    low_75 = statistics.quantiles(low_durs, n=4)[2]
    
    print(f"\nClusters (25th-75th percentiles):")
    print(f"High short: {high_25:.0f}-{high_75:.0f}µs")
    print(f"Low short:  {low_25:.0f}-{low_75:.0f}µs")
    
    # Suggest timings
    high_short_avg = statistics.mean([d for d in high_durs if d <= high_75])
    high_long_avg = statistics.mean([d for d in high_durs if d > high_75])
    low_short_avg = statistics.mean([d for d in low_durs if d <= low_75])
    low_long_avg = statistics.mean([d for d in low_durs if d > low_75])
    
    print(f"\nSuggested timings:")
    print(f"#define CAME_SHORT_PULSE {int((high_short_avg + low_short_avg) / 2)}")
    print(f"#define CAME_LONG_PULSE {int((high_long_avg + low_long_avg) / 2)}")
    
    return high_durs, low_durs, total_durs

def main():
    if len(sys.argv) < 2:
        print("Usage: detailed_came_analysis.py <file.sub>")
        sys.exit(1)
    
    path = Path(sys.argv[1])
    values = parse_sub(path)
    groups = analyze_timing_pattern(values)
    sync_positions = find_sync_gaps(groups)
    
    print(f"File: {path.name}")
    print(f"Total groups: {len(groups)}")
    print(f"Sync positions: {len(sync_positions)}")
    
    # Analyze bit patterns
    patterns = analyze_bit_patterns(groups, sync_positions)
    
    if patterns:
        cluster_timings(patterns)
    else:
        print("No bit patterns found")

if __name__ == "__main__":
    main()
