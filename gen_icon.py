#!/usr/bin/env python3
import sys

try:
    with open('src/map.png', 'rb') as f:
        data = f.read()
    
    with open('src/map_png.h', 'w') as out:
        out.write('#pragma once\n\n')
        out.write('static const unsigned char map_png[] = {\n    ')
        hex_bytes = [f'0x{b:02x}' for b in data]
        # Write 16 bytes per line for readability
        for i, h in enumerate(hex_bytes):
            out.write(h)
            if i < len(hex_bytes) - 1:
                out.write(', ')
            if (i + 1) % 16 == 0:
                out.write('\n    ')
        out.write('\n};\n\n')
        out.write(f'static const unsigned int map_png_len = {len(data)};\n')
    
    print(f"Success: wrote {len(data)} bytes to src/map_png.h")

except Exception as e:
    print(f"Error: {e}", file=sys.stderr)
    sys.exit(1)
