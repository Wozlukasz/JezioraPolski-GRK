import struct
import math

with open('Assets/strzeszynek-teren-wys.raw', 'rb') as f:
    data = f.read()

width = 8192
height = 8192
max_dist = 400.0

def get_height(x, z):
    u = (x / max_dist) * 0.5 + 0.5
    v = (z / max_dist) * 0.5 + 0.5
    u = max(0.0, min(1.0, u))
    v = max(0.0, min(1.0, v))
    
    px = int(u * (width - 1))
    py = int((1.0 - v) * (height - 1))
    
    idx = (py * width + px) * 2
    if idx >= len(data) - 1: return -1000
    
    val = struct.unpack('<H', data[idx:idx+2])[0]
    normalized = val / 65535.0
    return normalized * 110.0 + 35.0

for radius in range(50, 400, 20):
    for angle in range(0, 360, 10):
        rad = math.radians(angle)
        x = radius * math.cos(rad)
        z = radius * math.sin(rad)
        h = get_height(x, z)
        if 64.0 <= h <= 65.0:
            print(f"Found shore at: x={x:.1f}, z={z:.1f}, h={h:.2f}")
            import sys
            sys.exit(0)
