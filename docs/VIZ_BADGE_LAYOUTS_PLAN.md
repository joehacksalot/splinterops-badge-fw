# Implementation Plan: Per-Badge PCB Layouts in Visualization

## Overview

Currently only FMAN25 uses real PCB pick-and-place coordinates for LED positioning in the visualizer (`computeFman25Positions`). The other three badges (TRON, REACTOR, CREST) fall back to generic concentric-circle rendering. This plan adds accurate PCB-based layouts for all badges, plus touch-pad overlays on the canvas and proper `--badge` propagation through the CLI.

---

## Current State

### What exists
- **Layout files** in `tools/layouts/` for all 4 badges (different formats per badge — see below)
- **`badge_layouts.json`** with LED counts, ring offsets, and touch labels for all 4 badges
- **`viz.js`** with `computeFman25Positions()` using hardcoded PCB coords, and a generic circle fallback for the other 3
- **`viz.js`** FMAN25 touch-pad overlay (`FMAN25_TOUCH_LED_MAP`, `computeTouchPadPositions`) — only for FMAN25
- **Firmware `touchMap`** in `main/src/LedControl.c` with per-badge touch→LED mappings for all 4 badges
- **`run_viz.sh`** accepts `--badge` but only passes it to the build step, not to the viz UI

### What's missing
1. PCB-coordinate LED rendering for TRON, REACTOR, CREST
2. Touch-pad canvas overlays for TRON, REACTOR, CREST
3. `--badge` CLI option propagated to the browser UI (auto-select badge on load)
4. Shape guide outlines (ring/perimeter) derived from actual PCB geometry per badge

---

## Layout File Analysis

### File Formats (all different!)

| File | Format | Coordinate columns | Designator column |
|------|--------|--------------------|-------------------|
| `fman25.csv` | CSV with header `pos_x,pos_y,...,designator,...,value,...` | `pos_x`, `pos_y` (Y is negative) | `designator` |
| `reactor.csv` | CSV with header `pos_x,pos_y,...,designator,...,value,...` | `pos_x`, `pos_y` (Y is positive) | `designator` |
| `crest.csv` | KiCad CSV with header line `hyrule_concept-all-pos`, then `Ref,Val,Package,PosX,PosY,Rot,Side` | `PosX`, `PosY` (Y is negative) | `Ref` |
| `tron.pos` | KiCad `.pos` file — comment lines (`#`/`##`), space-delimited: `Ref Val Package PosX PosY Rot Side` | `PosX`, `PosY` (Y is positive) | `Ref` |

The CSV parser must handle all four formats.

### LED Inventory Per Badge

| Badge | File | LED types in file | Designator range | Total LEDs | Inner ring (firmware) | Outer ring (firmware) |
|-------|------|-------------------|------------------|------------|----------------------|----------------------|
| **FMAN25** | `fman25.csv` | D1–D32 SK6812SIDE, D33–D45 SK6812MINI | D1–D45 | **45** ✓ | D33–D45 (idx 32–44, 13 LEDs) | D1–D32 (idx 0–31, 32 LEDs) |
| **TRON** | `tron.pos` | D4–D30 SK6812MINI, D31–D80 SK6812SIDE | D4–D80 | **77** ✓ | D4–D30 (idx 0–26, 27 LEDs) | D31–D80 (idx 27–76, 50 LEDs) |
| **REACTOR** | `reactor.csv` | D1–D48 SK6812SIDE | D1–D48 | **48** ✓ | D1–D24 (idx 0–23, 24 LEDs) | D25–D48 (idx 24–47, 24 LEDs) |
| **CREST** | `crest.csv` | D1–D59 SK6812SIDE | D1–D59 | **59** ✓ | D1–D6 (idx 0–5, 6 LEDs) | D7–D59 (idx 6–58, 53 LEDs) |

**All four files now contain the correct number of LEDs** matching firmware definitions in `LedControl.h`.

### Non-LED components to filter out
- **FMAN25**: D61 (SOD-523 diode), D62/D63 (indicator LEDs) — on `bottom` side
- **TRON**: D1 (PMEG2010AEH protection diode), D2/D3 (indicator LEDs), D81 (LED_RED) — different packages
- **REACTOR**: D49 (SOD-523 diode), D50–D53 (indicator LEDs) — on `bottom` side or different package
- **CREST**: D61 (SOD-523 diode), D62/D63 (indicator LEDs) — on `bottom` side

**Filter rule**: Keep only rows where `Val`/`value` contains `SK6812` (covers both SK6812SIDE and SK6812MINI).

### Designator-to-Index Mapping

The physical LED strip index is derived from the designator number:

| Badge | Formula | Example |
|-------|---------|---------|
| FMAN25 | `index = D_number - 1` | D1→0, D45→44 |
| TRON | `index = D_number - 4` | D4→0, D30→26, D31→27, D80→76 |
| REACTOR | `index = D_number - 1` | D1→0, D48→47 |
| CREST | `index = D_number - 1` | D1→0, D59→58 |

### Corrected Pixel Offset (Logical→Physical Remapping)

The firmware uses `correctedPixelOffset[]` to remap logical pixel indices to physical strip positions. The visualizer receives **logical** pixel data but PCB coordinates are **physical**. We must apply this mapping.

| Badge | Mapping | Notes |
|-------|---------|-------|
| **FMAN25** | Identity (0,1,...,44) | No remapping needed |
| **CREST** | Identity (0,1,...,58) | No remapping needed |
| **REACTOR** | Inner 0–23: identity; Outer: `[24]=24, [25]=47, [26]=46, ..., [47]=25` | Outer ring is reversed |
| **TRON** | Inner 0–26: identity; Outer: `[27]=72, [28]=71, ..., [72]=27, [73]=76, [74]=75, [75]=74, [76]=73` | Outer ring reversed + 4 corner LEDs wrap |

**This mapping is critical for TRON/REACTOR** — without it, lit LEDs appear at wrong physical locations.

### Coordinate System Notes

- **FMAN25**: PCB Y is negative (more negative = further down). Canvas must negate Y.
- **TRON**: PosY is positive (standard KiCad: larger Y = further down on PCB, but for display we negate).
- **REACTOR**: PosY is positive (smaller Y = top of badge).
- **CREST**: PosY is negative (same convention as FMAN25).

The generic mapper should handle both conventions by auto-detecting the Y range and mapping min→top, max→bottom on canvas.

---

## Implementation Steps

### Phase 1: CSV → JSON Position Data (Python utility)

**File**: `tools/generate_badge_positions.py` (new)

1. Parse each layout file in `tools/layouts/`, handling the four different formats:
   - **fman25.csv / reactor.csv**: Standard CSV, use `pos_x`/`pos_y` and `designator` columns
   - **crest.csv**: Skip first line (`hyrule_concept-all-pos`), then CSV with `Ref`/`PosX`/`PosY`/`Val` columns
   - **tron.pos**: Skip comment lines (`#`/`##`), parse space-delimited columns `Ref Val Package PosX PosY Rot Side`

2. For each file:
   - Filter rows where value/Val contains `SK6812`
   - Extract designator number, compute physical LED index using per-badge formula
   - Sort by physical index
   - Extract `(posX, posY)` for each LED

3. Output `tools/viz_ui/badge_pcb_positions.json`:
   ```json
   {
     "FMAN25": {
       "coords": [[x0,y0], [x1,y1], ...],
       "pixel_offset": [0,1,2,...,44],
       "designator_offset": 1
     },
     "TRON": {
       "coords": [[x0,y0], ...],
       "pixel_offset": [0,1,...,26, 72,71,...,27, 76,75,74,73],
       "designator_offset": 4
     },
     "REACTOR": {
       "coords": [[x0,y0], ...],
       "pixel_offset": [0,1,...,23, 24,47,46,...,25],
       "designator_offset": 1
     },
     "CREST": {
       "coords": [[x0,y0], ...],
       "pixel_offset": [0,1,...,58],
       "designator_offset": 1
     }
   }
   ```

4. Validate: assert `len(coords) == led_count` from `badge_layouts.json` for each badge.

### Phase 2: Refactor `viz.js` LED Position Computation

**File**: `tools/viz_ui/viz.js`

1. **Load PCB positions**: In `loadBadgeLayouts()`, also fetch `badge_pcb_positions.json` and store as `badgePcbPositions`.

2. **Generalize `computeLedPositions()`**: Replace the FMAN25-specific branch and generic-circle fallback with a single PCB-based path:
   ```js
   function computeLedPositions(layout) {
       const pcbData = badgePcbPositions[currentBadge];
       if (pcbData && pcbData.coords.length === layout.led_count) {
           return mapPcbCoordsToCanvas(pcbData, layout);
       }
       // Fallback to generic circles if no PCB data
       return computeGenericRingPositions(layout);
   }
   ```

3. **`mapPcbCoordsToCanvas(pcbData, layout)`**: Generalized from `computeFman25Positions`:
   - Apply `pixel_offset`: For each logical index `i`, the physical PCB position is `pcbData.coords[pcbData.pixel_offset[i]]`
   - Compute bounding box of all physical coords
   - Scale + center into canvas with margin
   - Tag each position with `ring: 'inner'` or `ring: 'outer'` based on `layout.inner_ring` / `layout.outer_ring` offsets

4. **Remove `computeFman25Positions()`** and inline FMAN25 PCB coords — all data now comes from `badge_pcb_positions.json`.

### Phase 3: Per-Badge Touch Pad Overlays on Canvas

**File**: `tools/viz_ui/viz.js`

Currently `FMAN25_TOUCH_LED_MAP` and `computeTouchPadPositions()` are FMAN25-only. Generalize:

1. **Add touch→LED maps for all badges** (from firmware `touchMap` in `LedControl.c`):
   ```js
   const TOUCH_LED_MAPS = {
       FMAN25: [
           [21,22,23], [18,19,20], [15,16,17], [13,14,15],
           [10,11,12], [7,8,9], [5,6,7], [2,3,4], [0,1,31]
       ],
       TRON: [
           [24,25,47,35,36,37], [26,27,24], [28,29,30],
           [30,31,32], [33,34,24],
           [38,39,24], [40,41,42], [42,43,44], [45,46,47]
       ],
       REACTOR: [
           [24,25,47,35,36,37], [26,27,24], [28,29,30],
           [30,31,32], [33,34,24],
           [38,39,24], [40,41,42], [42,43,44], [45,46,47]
       ],
       CREST: [
           [8,9,10,11,12], [16,17,18], [23,24],
           [28], [31], [35],
           [40,41], [46,47,48], [52,53,54,55,56]
       ],
   };
   ```

2. **Generalize `computeTouchPadPositions()`**: Remove the `currentBadge !== 'FMAN25'` guard. Use `TOUCH_LED_MAPS[currentBadge]` to compute centroids for any badge. For circular badges (TRON/REACTOR), push pads outward from the circle center. For CREST's irregular shape, push from the centroid of all LED positions.

3. **Per-badge shape guide rendering** in `renderFrame()`:
   - **FMAN25**: Draw triangle guides from corner LEDs (existing logic)
   - **TRON**: Draw two concentric circles — inner ring radius from D4–D30 positions, outer ring radius from D31–D80 positions
   - **REACTOR**: Draw two concentric circles — both rings are equal radius but offset, derive from LED positions
   - **CREST**: Draw a convex hull or simplified outline connecting the outermost LED positions (irregular crest/shield shape)

### Phase 4: Badge Selection via CLI & URL

**Files**: `tools/run_viz.sh`, `tools/viz_bridge.py`, `tools/viz_ui/viz.js`

1. **`run_viz.sh`**: Pass `--badge` to `viz_bridge.py`:
   ```bash
   python3 "$SCRIPT_DIR/viz_bridge.py" \
       --badge "$BADGE_TYPE" \
       --qemu-port "$QEMU_VIZ_PORT" \
       ...
   ```

2. **`viz_bridge.py`**: Accept `--badge` argument. When the HTTP handler serves `/` or `/index.html`, redirect to `/index.html?badge=<BADGE>` (or inject a `<script>` tag). Simplest: rewrite the URL.

3. **`viz.js`**: On init, read `?badge=` from `window.location.search` and set `currentBadge` + update the `<select>` dropdown:
   ```js
   const urlBadge = new URLSearchParams(window.location.search).get('badge');
   if (urlBadge && badgeLayouts[urlBadge.toUpperCase()]) {
       currentBadge = urlBadge.toUpperCase();
       badgeSelect.value = currentBadge;
   }
   ```

---

## File Change Summary

| File | Action |
|------|--------|
| `tools/generate_badge_positions.py` | **New** — Multi-format layout parser → JSON generator |
| `tools/viz_ui/badge_pcb_positions.json` | **New** — Generated PCB coordinate + pixel offset data |
| `tools/viz_ui/viz.js` | **Modify** — Generalize LED positioning, touch pads, shape guides; remove hardcoded FMAN25 coords |
| `tools/viz_bridge.py` | **Modify** — Accept `--badge`, propagate to UI via URL param |
| `tools/run_viz.sh` | **Modify** — Pass `--badge` to viz_bridge |

---

## Suggested Implementation Order

1. **Phase 1** — `generate_badge_positions.py` (validates all layout data, produces JSON)
2. **Phase 4** — `--badge` CLI propagation (small, high-value UX improvement)
3. **Phase 2** — Refactor LED positioning + pixel offset mapping (core feature)
4. **Phase 3** — Touch pad overlays + shape guides for all badges (polish)
