"""Hero cricket ball. 7.2 cm diameter regulation sphere with raised stitched seam.

Origin at the centre of the ball. Local frame matches AC26Simulation::Ball.
"""
import math
from c26_build import Builder, material, mesh_from

LEATHER, SEAM, STITCH = 0, 1, 2


def build(name='SM_C26_Ball_Hero', radius=3.6):  # radius in cm (7.2 cm diameter)
    mats = [
        material('M_C26_BallLeather', (0.85, 0.85, 0.80), roughness=0.38, metallic=0.0),
        material('M_C26_BallSeam', (0.95, 0.95, 0.92), roughness=0.55, metallic=0.0),
        material('M_C26_BallStitch', (0.20, 0.20, 0.20), roughness=0.80, metallic=0.0)
    ]
    b = Builder()
    lat_steps = 16
    lon_steps = 32

    # Spherical mesh
    verts = []
    # North pole
    verts.append((0.0, 0.0, radius))
    for i in range(1, lat_steps):
        theta = math.pi * i / lat_steps
        z = radius * math.cos(theta)
        r_xy = radius * math.sin(theta)
        # Check if near equator (the seam)
        is_equator = abs(i - lat_steps // 2) <= 1
        r_current = radius + (0.12 if is_equator else 0.0)
        
        for j in range(lon_steps):
            phi = 2 * math.pi * j / lon_steps
            x = r_current * math.sin(theta) * math.cos(phi)
            y = r_current * math.sin(theta) * math.sin(phi)
            verts.append((x, y, z))
    # South pole
    verts.append((0.0, 0.0, -radius))

    # Faces
    # Top cap
    for j in range(lon_steps):
        next_j = (j + 1) % lon_steps
        v0 = 0
        v1 = 1 + j
        v2 = 1 + next_j
        b.f.append((v0, v1, v2))
        b.m.append(LEATHER)

    # Middle rings
    for i in range(1, lat_steps - 1):
        ring_start = 1 + (i - 1) * lon_steps
        next_ring_start = 1 + i * lon_steps
        is_seam = abs(i - lat_steps // 2) == 0
        for j in range(lon_steps):
            next_j = (j + 1) % lon_steps
            v0 = ring_start + j
            v1 = ring_start + next_j
            v2 = next_ring_start + next_j
            v3 = next_ring_start + j
            b.f.append((v0, v1, v2, v3))
            b.m.append(SEAM if is_seam else LEATHER)

    # Bottom cap
    south_idx = len(verts) - 1
    bottom_ring_start = 1 + (lat_steps - 2) * lon_steps
    for j in range(lon_steps):
        next_j = (j + 1) % lon_steps
        v0 = bottom_ring_start + j
        v1 = south_idx
        v2 = bottom_ring_start + next_j
        b.f.append((v0, v1, v2))
        b.m.append(LEATHER)

    b.v = verts
    return mesh_from(name, b.v, b.f, b.m, mats, smooth_angle=45.0)

if __name__ == '__main__':
    from c26_build import clear_scene, export_fbx
    clear_scene()
    obj = build()
    export_fbx([obj], f'/Users/aagamjain/Desktop/CRICKET-26-GAME/CRICKETGAME/ArtSource/Exports/Equipment/{obj.name}.fbx')
