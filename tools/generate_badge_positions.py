#!/usr/bin/env python3
"""
Generate badge_pcb_positions.json from PCB layout files.

Parses the four different layout file formats (fman25.csv, reactor.csv,
crest.csv, tron.pos), filters for SK6812 LEDs, and outputs a unified
JSON file with PCB coordinates and pixel offset mappings for the
browser-based visualizer.

Usage:
    python tools/generate_badge_positions.py
"""

import csv
import json
import os
import re
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).parent
LAYOUTS_DIR = SCRIPT_DIR / "layouts"
OUTPUT_FILE = SCRIPT_DIR / "viz_ui" / "badge_pcb_positions.json"
BADGE_LAYOUTS_FILE = SCRIPT_DIR / "viz_ui" / "badge_layouts.json"

# Per-badge configuration
BADGE_CONFIG = {
    "FMAN25": {
        "file": "fman25.csv",
        "format": "standard_csv",
        "designator_offset": 1,  # D1 → index 0
        "pixel_offset": list(range(45)),  # Identity mapping
    },
    "TRON": {
        "file": "tron.pos",
        "format": "kicad_pos",
        "designator_offset": 4,  # D4 → index 0
        "pixel_offset": (
            list(range(27))  # Inner 0–26: identity
            + [72, 71, 70, 69, 68, 67, 66, 65, 64, 63, 62, 61, 60, 59, 58,
               57, 56, 55, 54, 53, 52, 51, 50, 49, 48, 47, 46, 45, 44, 43,
               42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 32, 31, 30, 29, 28,
               27, 76, 75, 74, 73]  # Outer ring reversed + 4 corner LEDs wrap
        ),
    },
    "REACTOR": {
        "file": "reactor.csv",
        "format": "standard_csv",
        "designator_offset": 1,  # D1 → index 0
        "pixel_offset": (
            list(range(24))  # Inner 0–23: identity
            + [24, 47, 46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34,
               33, 32, 31, 30, 29, 28, 27, 26, 25]  # Outer ring reversed
        ),
    },
    "CREST": {
        "file": "crest.csv",
        "format": "kicad_csv",
        "designator_offset": 1,  # D1 → index 0
        "pixel_offset": list(range(59)),  # Identity mapping
    },
}


def parse_standard_csv(filepath):
    """Parse fman25.csv / reactor.csv format: standard CSV with pos_x, pos_y, designator, value columns."""
    leds = []
    with open(filepath, "r", encoding="utf-8-sig") as f:
        reader = csv.DictReader(f)
        for row in reader:
            # Strip whitespace/CR from keys and values
            row = {k.strip(): v.strip() for k, v in row.items()}
            val = row.get("value", "")
            if "SK6812" not in val:
                continue
            designator = row["designator"]
            pos_x = float(row["pos_x"])
            pos_y = float(row["pos_y"])
            leds.append((designator, pos_x, pos_y))
    return leds


def parse_kicad_csv(filepath):
    """Parse crest.csv format: first line is title, then CSV with Ref, Val, PosX, PosY columns."""
    leds = []
    with open(filepath, "r") as f:
        # Skip the title line (e.g. "hyrule_concept-all-pos")
        f.readline()
        reader = csv.DictReader(f)
        for row in reader:
            val = row.get("Val", "")
            if "SK6812" not in val:
                continue
            ref = row["Ref"]
            pos_x = float(row["PosX"])
            pos_y = float(row["PosY"])
            leds.append((ref, pos_x, pos_y))
    return leds


def parse_kicad_pos(filepath):
    """Parse tron.pos format: comment lines (#/##), space-delimited columns."""
    leds = []
    with open(filepath, "r") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            parts = line.split()
            if len(parts) < 7:
                continue
            ref, val, _package, pos_x, pos_y, _rot, _side = parts[:7]
            if "SK6812" not in val:
                continue
            leds.append((ref, float(pos_x), float(pos_y)))
    return leds


def extract_designator_number(designator):
    """Extract numeric part from designator like 'D4' → 4."""
    m = re.match(r"D(\d+)", designator)
    if m:
        return int(m.group(1))
    return None


def generate_badge_data(badge_name, config):
    """Generate PCB position data for a single badge."""
    filepath = LAYOUTS_DIR / config["file"]
    fmt = config["format"]

    if fmt == "standard_csv":
        leds = parse_standard_csv(filepath)
    elif fmt == "kicad_csv":
        leds = parse_kicad_csv(filepath)
    elif fmt == "kicad_pos":
        leds = parse_kicad_pos(filepath)
    else:
        raise ValueError(f"Unknown format: {fmt}")

    offset = config["designator_offset"]

    # Build dict: physical_index → (posX, posY)
    led_map = {}
    for designator, px, py in leds:
        d_num = extract_designator_number(designator)
        if d_num is None:
            continue
        physical_index = d_num - offset
        if physical_index < 0:
            print(f"  WARNING: {badge_name} {designator} → negative index {physical_index}, skipping")
            continue
        led_map[physical_index] = [round(px, 4), round(py, 4)]

    # Sort by physical index and build coords array
    max_index = max(led_map.keys()) if led_map else 0
    coords = []
    for i in range(max_index + 1):
        if i in led_map:
            coords.append(led_map[i])
        else:
            print(f"  WARNING: {badge_name} missing physical index {i}")
            coords.append([0, 0])

    return {
        "coords": coords,
        "pixel_offset": config["pixel_offset"],
        "designator_offset": offset,
    }


def main():
    # Load badge_layouts.json for validation
    with open(BADGE_LAYOUTS_FILE, "r") as f:
        badge_layouts = json.load(f)

    result = {}
    all_ok = True

    for badge_name, config in BADGE_CONFIG.items():
        print(f"Processing {badge_name} ({config['file']})...")
        data = generate_badge_data(badge_name, config)

        expected_count = badge_layouts[badge_name]["led_count"]
        actual_count = len(data["coords"])
        offset_count = len(data["pixel_offset"])

        if actual_count != expected_count:
            print(f"  ERROR: {badge_name} has {actual_count} LEDs, expected {expected_count}")
            all_ok = False
        else:
            print(f"  OK: {actual_count} LEDs")

        if offset_count != expected_count:
            print(f"  ERROR: {badge_name} pixel_offset has {offset_count} entries, expected {expected_count}")
            all_ok = False

        result[badge_name] = data

    if not all_ok:
        print("\nERROR: Validation failed. Fix issues above before proceeding.")
        sys.exit(1)

    # Write output
    with open(OUTPUT_FILE, "w") as f:
        json.dump(result, f, indent=2)
        f.write("\n")

    print(f"\nWrote {OUTPUT_FILE}")


if __name__ == "__main__":
    main()
