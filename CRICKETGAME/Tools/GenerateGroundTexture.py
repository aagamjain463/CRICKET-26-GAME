"""Generate high-fidelity, deterministic 2048x2048 playing surface texture for CRICKET 26.
Eclipse Oval: 180m x 188m playing field.
"""
import math
import os
import struct

def mix(a, b, t):
    return tuple(x + (y - x) * t for x, y in zip(a, b))

def smooth(a, b, x):
    t = max(0.0, min(1.0, (x - a) / (b - a)))
    return t * t * (3.0 - 2.0 * t)

def clamp(x, lo, hi):
    return max(lo, min(hi, x))

def main():
    size = 2048
    out_dir = os.path.join(os.path.dirname(__file__), '..', 'ArtSource', 'Generated', 'World')
    os.makedirs(out_dir, exist_ok=True)
    out_path = os.path.join(out_dir, 'T_Eclipse_GroundColour.tga')

    data = bytearray()

    for iy in range(size):
        y = (iy / float(size - 1) - 0.5) * 18800.0
        abs_y = abs(y)

        for ix in range(size):
            x = (ix / float(size - 1) - 0.5) * 18000.0
            abs_x = abs(x)

            # ---- Broad Organic Aeration & Micro Grain ----
            broad = math.sin(x * 0.0011 + math.sin(y * 0.0014)) * math.cos(y * 0.0012)
            macro = math.cos(x * 0.00045) * math.sin(y * 0.0005)
            grain = math.sin(x * 0.137 + y * 0.071) * math.sin(y * 0.193 - x * 0.071)

            # ---- Outfield Mower Bands (5.5m wide professional lawn stripes) ----
            # Mower passes cut at a slight broadcast angle (~12 degrees)
            mow_phase = (y + x * 0.21) / 550.0
            mow = math.tanh(math.sin(mow_phase * math.pi) * 3.4)

            # Circular boundary mower pass around the perimeter (radius 5800 - 6600)
            ground_rad = math.sqrt((x / 6800.0) ** 2 + (y / 7400.0) ** 2)
            mow_circ = math.sin(ground_rad * 38.0) * 0.4
            mow_composite = mow * 0.85 + mow_circ * smooth(0.72, 0.95, ground_rad)

            # ---- The Square (13.8m x 24.8m) ----
            # Shaved flat in a single pass; mower stripes stop cleanly at the square edge
            insq = (1.0 - smooth(690.0, 740.0, abs_x)) * (1.0 - smooth(1200.0, 1260.0, abs_y))

            # Outfield tone modulation
            # Lush international broadcast green: deep rye-grass chlorophyll base
            v = 1.0 + 0.115 * mow_composite * (1.0 - insq) + 0.040 * broad + 0.025 * macro + 0.012 * grain
            base_grass = (0.046 * v, 0.165 * v, 0.040 * v)

            # Slight cool-sky sheen on light-facing mower bands
            sheen_tint = (0.058 * v, 0.180 * v, 0.052 * v)
            grass_col = mix(base_grass, sheen_tint, max(0.0, mow * 0.5 * (1.0 - insq)))

            # Bowler run-up wear tracks (drier, slightly worn grass heading into each crease)
            runup_x = smooth(120.0, 30.0, abs_x)
            runup_y = smooth(1100.0, 1350.0, abs_y) * (1.0 - smooth(2400.0, 2900.0, abs_y))
            runup_wear = runup_x * runup_y * 0.38
            grass_col = mix(grass_col, (0.082 * v, 0.152 * v, 0.052 * v), runup_wear)

            # Perimeter boundary track (subtle foot-traffic ring inside the rope)
            boundary_track = smooth(0.94, 0.985, ground_rad) * (1.0 - smooth(1.005, 1.03, ground_rad))
            grass_col = mix(grass_col, (0.072 * v, 0.142 * v, 0.048 * v), boundary_track * 0.32)

            # ---- Square Turf Color (Paler, closely shaved turf) ----
            square_turf = (0.088 * v, 0.182 * v, 0.056 * v)
            col = mix(grass_col, square_turf, insq * 0.78)

            # Adjacent unused pitch strips (3.05m each, varied rolling ages)
            for s in (-2, -1, 1, 2):
                d = abs(x - s * 305.0)
                if d < 155.0:
                    strip_mask = (1.0 - smooth(128.0, 150.0, d)) * (1.0 - smooth(1080.0, 1190.0, abs_y))
                    age = 0.85 + 0.14 * float((abs(s) * 7 + 3) % 4)
                    col = mix(col, (0.122 * v * age, 0.185 * v * age, 0.068 * v * age), strip_mask * 0.62)

            # ---- Active Center Pitch Strip (3.05m x 22.5m) ----
            edge = abs_x + grain * 2.8
            pitch_mask = (1.0 - smooth(144.0, 153.0, edge)) * (1.0 - smooth(1155.0, 1195.0, abs_y + grain * 3.5))

            if pitch_mask > 0.0:
                # Granular clay base with subtle warm golden loam tone
                dry_factor = 1.0 + 0.07 * broad + 0.035 * grain + 0.018 * math.sin(y * 0.038 + x * 0.024)
                clay_base = (0.268 * dry_factor, 0.208 * dry_factor, 0.132 * dry_factor)

                # Central firm driving corridor (compacted clay, slightly richer)
                corridor = smooth(55.0, 15.0, abs_x) * (1.0 - smooth(820.0, 960.0, abs_y))
                clay_base = mix(clay_base, (0.245 * dry_factor, 0.188 * dry_factor, 0.118 * dry_factor), corridor * 0.45)

                # Good-length corridor dry landing patches (chalky light patches where ball pitches repeatedly)
                good_length = smooth(350.0, 520.0, abs_y) * (1.0 - smooth(780.0, 920.0, abs_y)) * smooth(65.0, 10.0, abs_x)
                clay_base = mix(clay_base, (0.295 * dry_factor, 0.235 * dry_factor, 0.155 * dry_factor), good_length * 0.55)

                # Bowling and Batting Footmark Abrasions (spike gouges and compacted dark damp earth)
                wear = 0.0
                for end_side in [-1.0, 1.0]:
                    wy = end_side * 1050.0
                    # Bowler front-foot plant gouges
                    for fx, fy, rx, ry in [(-32.0, wy, 42.0, 78.0), (28.0, wy - end_side * 60.0, 32.0, 55.0)]:
                        dist_sq = ((x - fx) / rx) ** 2 + ((y - fy) / ry) ** 2
                        wear = max(wear, (1.0 - smooth(0.04, 1.6, dist_sq)) * 0.85)

                    # Batter guard scuff & bat-tapping marks at popping crease
                    by = end_side * 884.0
                    for bx, by_off, rx, ry in [(-18.0, by + end_side * 15.0, 38.0, 42.0), (12.0, by, 25.0, 35.0)]:
                        dist_sq = ((x - bx) / rx) ** 2 + ((y - by_off) / ry) ** 2
                        wear = max(wear, (1.0 - smooth(0.05, 1.7, dist_sq)) * 0.72)

                dark_soil = (0.165, 0.135, 0.082)
                soil = mix(clay_base, dark_soil, wear)

                # Sparse grass fringe along pitch margins
                coverage = smooth(108.0, 149.0, abs_x) * (0.26 + 0.12 * math.sin(y * 0.14))
                soil = mix(soil, (0.125, 0.175, 0.060), coverage)

                # ---- Embedded Painted Crease Markings ----
                # Popping Crease: y = ±884 cm, length 366 cm (x: -183 to +183), line width 5.0 cm
                # Bowling Crease: y = ±1006 cm, length 264 cm (x: -132 to +132), line width 5.0 cm
                # Return Creases: x = ±132 cm, y from ±884 back past ±1006 to ±1150
                crease_intensity = 0.0
                for end in [-1.0, 1.0]:
                    py = end * 884.0
                    by = end * 1006.0

                    # Popping crease line
                    if abs(y - py) <= 3.2 and abs_x <= 183.0:
                        dist_line = abs(y - py)
                        edge_fade = smooth(3.2, 1.8, dist_line) * (1.0 - smooth(178.0, 183.0, abs_x))
                        # Worn line: bat tap abrasions scratch through paint
                        spike_scratch = 0.75 + 0.25 * math.sin(x * 1.8 + y * 2.1)
                        crease_intensity = max(crease_intensity, edge_fade * spike_scratch)

                    # Bowling crease line
                    if abs(y - by) <= 3.0 and abs_x <= 132.0:
                        dist_line = abs(y - by)
                        edge_fade = smooth(3.0, 1.5, dist_line) * (1.0 - smooth(128.0, 132.0, abs_x))
                        crease_intensity = max(crease_intensity, edge_fade * 0.9)

                    # Return creases
                    if abs(abs_x - 132.0) <= 2.8 and (y * end) >= 880.0 and (y * end) <= 1180.0:
                        dist_line = abs(abs_x - 132.0)
                        edge_fade = smooth(2.8, 1.4, dist_line)
                        crease_intensity = max(crease_intensity, edge_fade * 0.88)

                if crease_intensity > 0.0:
                    paint_col = (0.76, 0.78, 0.74)
                    soil = mix(soil, paint_col, crease_intensity * 0.92)

                col = mix(col, soil, pitch_mask)

            # Outer boundary perimeter blend
            col = mix(col, (0.075 * v, 0.155 * v, 0.055 * v), smooth(0.975, 0.995, ground_rad) * 0.45)
            col = mix(col, (0.068, 0.098, 0.048), smooth(1.010, 1.025, ground_rad))

            # Pack 24-bit BGR
            r = int(clamp(col[0] * 255.0, 0.0, 255.0))
            g = int(clamp(col[1] * 255.0, 0.0, 255.0))
            b = int(clamp(col[2] * 255.0, 0.0, 255.0))
            data.extend((b, g, r))

    with open(out_path, 'wb') as f:
        # Uncompressed 24-bit TrueColor TGA
        f.write(struct.pack('<BBBHHBHHHHBB', 0, 0, 2, 0, 0, 0, 0, 0, size, size, 24, 32))
        f.write(data)

    print(f"C26_GROUND_TEXTURE generated successfully: {out_path} ({len(data)} bytes)")

if __name__ == '__main__':
    main()
