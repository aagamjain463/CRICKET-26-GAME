"""Hero cricket stumps & bails.
Regulation dimensions:
Stumps height: 71.1 cm (28 inches) above ground.
Stump diameter: 3.8 cm (1.5 inches).
Wicket width: 22.86 cm (9 inches) between outer stumps.
Bails length: 11.1 cm (4.375 inches).

Origin at the base of the central stump (Z=0 on the pitch surface).
"""
import math
from c26_build import Builder, material, circle, tube, mesh_from

WOOD, BRASS, PAINT = 0, 1, 2


def build_stump(b, offset_x=0.0, height=71.1, radius=1.9, sides=16):
    # Base spike / foot
    z_levels = [
        (0.0, radius * 0.75, BRASS),
        (2.0, radius, BRASS),
        (65.0, radius, WOOD),
        (69.0, radius * 0.95, WOOD),
        (70.5, radius * 0.85, WOOD),
        (71.1, radius * 0.70, WOOD),
    ]
    
    prev_ring = None
    for z, r, mat in z_levels:
        pts = [(offset_x + r * math.cos(2 * math.pi * j / sides),
                r * math.sin(2 * math.pi * j / sides),
                z) for j in range(sides)]
        base = len(b.v)
        b.v.extend(pts)
        curr_ring = list(range(base, base + sides))
        if prev_ring:
            for j in range(sides):
                nj = (j + 1) % sides
                b.f.append((prev_ring[j], prev_ring[nj], curr_ring[nj], curr_ring[j]))
                b.m.append(mat)
        prev_ring = curr_ring

    # Top cap
    top_center = len(b.v)
    b.v.append((offset_x, 0.0, height))
    for j in range(sides):
        nj = (j + 1) % sides
        b.f.append((prev_ring[j], prev_ring[nj], top_center))
        b.m.append(WOOD)


def build_stumps_set(name='SM_C26_Wicket_Stumps'):
    mats = [
        material('M_C26_StumpWood', (0.78, 0.65, 0.42), roughness=0.45, metallic=0.0),
        material('M_C26_StumpBrass', (0.80, 0.70, 0.25), roughness=0.25, metallic=0.85),
        material('M_C26_StumpPaint', (0.10, 0.12, 0.16), roughness=0.50, metallic=0.0),
    ]
    b = Builder()
    spacing = 11.43  # 4.5 inches each side from centre
    build_stump(b, offset_x=-spacing)  # Off stump
    build_stump(b, offset_x=0.0)       # Middle stump
    build_stump(b, offset_x=spacing)   # Leg stump
    return mesh_from(name, b.v, b.f, b.m, mats, smooth_angle=35.0)


def build_bails(name='SM_C26_Wicket_Bails', sides=12):
    mats = [
        material('M_C26_StumpWood', (0.78, 0.65, 0.42), roughness=0.45, metallic=0.0),
    ]
    b = Builder()
    # Two bails resting on top of stumps at Z=71.1 + 0.8 cm
    z_base = 71.9
    spacing = 11.43
    bail_len = 11.1
    r_barrel = 1.1
    r_spigot = 0.55

    for center_x in [-spacing * 0.5, spacing * 0.5]:
        x_start = center_x - bail_len * 0.5
        x_end = center_x + bail_len * 0.5
        x_steps = [
            (x_start, r_spigot),
            (x_start + 1.8, r_spigot),
            (x_start + 2.4, r_barrel),
            (x_end - 2.4, r_barrel),
            (x_end - 1.8, r_spigot),
            (x_end, r_spigot),
        ]
        prev_ring = None
        for x, r in x_steps:
            pts = [(x,
                    r * math.cos(2 * math.pi * j / sides),
                    z_base + r * math.sin(2 * math.pi * j / sides)) for j in range(sides)]
            base = len(b.v)
            b.v.extend(pts)
            curr_ring = list(range(base, base + sides))
            if prev_ring:
                for j in range(sides):
                    nj = (j + 1) % sides
                    b.f.append((prev_ring[j], prev_ring[nj], curr_ring[nj], curr_ring[j]))
                    b.m.append(WOOD)
            prev_ring = curr_ring
            
    return mesh_from(name, b.v, b.f, b.m, mats, smooth_angle=35.0)


if __name__ == '__main__':
    from c26_build import clear_scene, export_fbx
    clear_scene()
    s = build_stumps_set()
    export_fbx([s], f'/Users/aagamjain/Desktop/CRICKET-26-GAME/CRICKETGAME/ArtSource/Exports/Equipment/{s.name}.fbx')
    clear_scene()
    b = build_bails()
    export_fbx([b], f'/Users/aagamjain/Desktop/CRICKET-26-GAME/CRICKETGAME/ArtSource/Exports/Equipment/{b.name}.fbx')
