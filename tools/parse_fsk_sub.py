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

def analyze_fsk_pattern(values):
    """Analyze 2-FSK pattern: positive = f0, negative = f1"""
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

def find_sync_gaps(groups, min_sync_us=15000):
    """Find sync gaps (long periods of same frequency)"""
    sync_positions = []
    for i, (sign, duration) in enumerate(groups):
        if duration >= min_sync_us:
            sync_positions.append(i)
    return sync_positions

def extract_bits_from_groups(groups, start_idx, bit_count=24):
    """Extract bits from FSK groups starting at start_idx"""
    bits = []
    i = start_idx
    
    # Skip initial sync
    while i < len(groups) and groups[i][1] > 10000:  # Skip long sync
        i += 1
    
    # Extract bits: look for short/long patterns
    while i + 1 < len(groups) and len(bits) < bit_count:
        freq0, dur0 = groups[i]
        freq1, dur1 = groups[i + 1]
        
        # Both should be same frequency (f0 or f1)
        if freq0 != freq1:
            i += 1
            continue
            
        # Analyze duration pattern
        total_dur = dur0 + dur1
        
        # Heuristic: short total = 0, long total = 1
        # Adjust thresholds based on your data
        if total_dur < 600:  # Short pattern = 0
            bits.append(0)
        elif total_dur > 600:  # Long pattern = 1
            bits.append(1)
        else:
            i += 1
            continue
            
        i += 2
    
    return bits if len(bits) == bit_count else None

def bits_to_hex(bits):
    if len(bits) != 24:
        return None
    val = 0
    for b in bits:
        val = (val << 1) | (1 if b else 0)
    return f"0x{val:06X}"

def try_different_thresholds(groups, start_idx):
    """Try different duration thresholds for bit detection"""
    candidates = []
    
    # Try different thresholds
    for threshold in [400, 500, 600, 700, 800]:
        bits = []
        i = start_idx
        
        # Skip sync
        while i < len(groups) and groups[i][1] > 10000:
            i += 1
            
        while i + 1 < len(groups) and len(bits) < 24:
            freq0, dur0 = groups[i]
            freq1, dur1 = groups[i + 1]
            
            if freq0 != freq1:
                i += 1
                continue
                
            total_dur = dur0 + dur1
            if total_dur < threshold:
                bits.append(0)
            else:
                bits.append(1)
            i += 2
            
        if len(bits) == 24:
            candidates.append(bits_to_hex(bits))
    
    return candidates

def main():
    if len(sys.argv) < 2:
        print("Usage: parse_fsk_sub.py <file.sub> [file2.sub ...]")
        sys.exit(1)
    
    all_candidates = Counter()
    
    for arg in sys.argv[1:]:
        path = Path(arg)
        values = parse_sub(path)
        groups = analyze_fsk_pattern(values)
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
            candidates = try_different_thresholds(groups, sync_idx)
            if candidates:
                print(f"  Candidates: {candidates}")
                for cand in candidates:
                    file_candidates[cand] += 1
            else:
                print(f"  No valid 24-bit sequences found")
        
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
