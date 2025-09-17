#!/usr/bin/env python3
import sys
import re
from pathlib import Path
from collections import Counter

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
    # Group consecutive same-sign values
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

def analyze_bit_timings(groups, sync_positions):
    """Analyze timing patterns for bits"""
    all_high_durations = []
    all_low_durations = []
    
    for sync_idx in sync_positions[:5]:  # Analyze first 5 sync positions
        i = sync_idx + 1  # Skip sync
        bit_count = 0
        
        while i + 1 < len(groups) and bit_count < 24:
            high_dur = groups[i][1]
            low_dur = groups[i + 1][1]
            
            # Skip if too long (next sync)
            if high_dur > 15000 or low_dur > 15000:
                break
                
            all_high_durations.append(high_dur)
            all_low_durations.append(low_dur)
            
            i += 2
            bit_count += 1
    
    return all_high_durations, all_low_durations

def main():
    if len(sys.argv) < 2:
        print("Usage: analyze_came_timing.py <file.sub>")
        sys.exit(1)
    
    path = Path(sys.argv[1])
    values = parse_sub(path)
    groups = analyze_timing_pattern(values)
    sync_positions = find_sync_gaps(groups)
    
    print(f"File: {path.name}")
    print(f"Total groups: {len(groups)}")
    print(f"Sync positions: {len(sync_positions)}")
    
    # Analyze bit timings
    high_durs, low_durs = analyze_bit_timings(groups, sync_positions)
    
    if high_durs and low_durs:
        print(f"\nHigh durations (first 20): {sorted(high_durs)[:20]}")
        print(f"Low durations (first 20): {sorted(low_durs)[:20]}")
        
        # Find clusters
        high_sorted = sorted(high_durs)
        low_sorted = sorted(low_durs)
        
        # Estimate short/long thresholds
        high_median = high_sorted[len(high_sorted)//2]
        low_median = low_sorted[len(low_sorted)//2]
        
        print(f"\nHigh median: {high_median}µs")
        print(f"Low median: {low_median}µs")
        
        # Find typical short/long values
        high_short = [d for d in high_durs if d < high_median]
        high_long = [d for d in high_durs if d >= high_median]
        low_short = [d for d in low_durs if d < low_median]
        low_long = [d for d in low_durs if d >= low_median]
        
        if high_short and high_long and low_short and low_long:
            print(f"\nHigh short: {min(high_short)}-{max(high_short)}µs (avg: {sum(high_short)/len(high_short):.0f}µs)")
            print(f"High long:  {min(high_long)}-{max(high_long)}µs (avg: {sum(high_long)/len(high_long):.0f}µs)")
            print(f"Low short:  {min(low_short)}-{max(low_short)}µs (avg: {sum(low_short)/len(low_short):.0f}µs)")
            print(f"Low long:   {min(low_long)}-{max(low_long)}µs (avg: {sum(low_long)/len(low_long):.0f}µs)")
            
            # Suggest CAME timings
            suggested_short = int((sum(high_short)/len(high_short) + sum(low_short)/len(low_short)) / 2)
            suggested_long = int((sum(high_long)/len(high_long) + sum(low_long)/len(low_long)) / 2)
            
            print(f"\nSuggested CAME timings:")
            print(f"#define CAME_SHORT_PULSE {suggested_short}")
            print(f"#define CAME_LONG_PULSE {suggested_long}")
    else:
        print("No bit timing data found")

if __name__ == "__main__":
    main()
