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

def analyze_came_timing(values):
    """Analyze CAME timing from raw data"""
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
    """Find sync gaps (very long periods)"""
    sync_positions = []
    for i, (sign, duration) in enumerate(groups):
        if duration >= min_sync_us:
            sync_positions.append(i)
    return sync_positions

def decode_came_bits(groups, start_idx):
    """Decode CAME 24-bit from groups starting at start_idx"""
    bits = []
    i = start_idx
    
    # Skip initial sync
    while i < len(groups) and groups[i][1] > 15000:  # Skip long sync
        i += 1
    
    # Decode 24 bits
    while i + 1 < len(groups) and len(bits) < 24:
        high_dur = groups[i][1]
        low_dur = groups[i + 1][1]
        
        # CAME encoding:
        # Bit 0: short high + long low
        # Bit 1: long high + short low
        if high_dur < low_dur:  # short high + long low = 0
            bits.append(0)
        else:  # long high + short low = 1
            bits.append(1)
        
        i += 2
    
    return bits if len(bits) == 24 else None

def bits_to_hex(bits):
    if len(bits) != 24:
        return None
    val = 0
    for b in bits:
        val = (val << 1) | (1 if b else 0)
    return f"0x{val:06X}"

def main():
    if len(sys.argv) < 2:
        print("Usage: parse_came_raw.py <file.sub> [file2.sub ...]")
        sys.exit(1)
    
    all_candidates = Counter()
    
    for arg in sys.argv[1:]:
        path = Path(arg)
        values = parse_sub(path)
        groups = analyze_came_timing(values)
        sync_positions = find_sync_gaps(groups)
        
        print(f"\nFile: {path.name}")
        print(f"Total groups: {len(groups)}")
        print(f"Sync positions: {sync_positions}")
        
        # Show first few groups for analysis
        print("First 20 groups (sign, duration_us):")
        for i, (sign, dur) in enumerate(groups[:20]):
            print(f"  {i}: {sign:+2d}, {dur:4d}us")
        
        file_candidates = Counter()
        
        # Try each sync position
        for sync_idx in sync_positions:
            print(f"\nTrying sync position {sync_idx}:")
            bits = decode_came_bits(groups, sync_idx)
            if bits:
                hex_val = bits_to_hex(bits)
                print(f"  Decoded: {hex_val} (bits: {bits})")
                file_candidates[hex_val] += 1
            else:
                print(f"  No valid 24-bit sequence found")
        
        if file_candidates:
            best, count = file_candidates.most_common(1)[0]
            print(f"\nFile majority: {best} (seen {count} times)")
            all_candidates[best] += count
        else:
            print(f"\nNo valid candidates for {path.name}")
    
    if all_candidates:
        best, total = all_candidates.most_common(1)[0]
        print(f"\nGlobal majority: {best} (total {total})")
    else:
        print("\nNo valid 24-bit candidates found")

if __name__ == "__main__":
    main()

