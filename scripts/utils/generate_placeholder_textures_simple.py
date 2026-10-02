#!/usr/bin/env python3
"""
Generate placeholder texture PNG files without external dependencies.

Creates simple solid-color PNG files using only Python standard library.
"""

import struct
import zlib
import os

def create_png(width, height, rgb, output_path):
    """Create a simple PNG file with solid color using Python standard library."""

    def write_chunk(chunk_type, data):
        """Write a PNG chunk."""
        chunk = struct.pack('>I', len(data))
        chunk += chunk_type
        chunk += data
        chunk += struct.pack('>I', zlib.crc32(chunk_type + data) & 0xFFFFFFFF)
        return chunk

    # PNG signature
    png_data = b'\x89PNG\r\n\x1a\n'

    # IHDR chunk (image header)
    ihdr = struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0)  # 8-bit RGB, no interlace
    png_data += write_chunk(b'IHDR', ihdr)

    # IDAT chunk (image data)
    raw_data = b''
    for y in range(height):
        # Filter type 0 (None) at start of each scanline
        raw_data += b'\x00'
        # RGB pixels
        for x in range(width):
            raw_data += struct.pack('BBB', rgb[0], rgb[1], rgb[2])

    compressed = zlib.compress(raw_data, 9)
    png_data += write_chunk(b'IDAT', compressed)

    # IEND chunk
    png_data += write_chunk(b'IEND', b'')

    # Write to file
    with open(output_path, 'wb') as f:
        f.write(png_data)

    print(f"Created: {output_path} ({width}x{height})")

def main():
    """Generate all placeholder textures."""
    base_path = "Plugins/GameFeatures/ProjectMenuCore/Content/UI/Textures/Placeholders"

    # Ensure directory exists
    os.makedirs(base_path, exist_ok=True)

    # 1. Menu Background (1920x1080, gray)
    create_png(
        1920, 1080,
        (128, 128, 128),
        os.path.join(base_path, "T_MenuBackground_Placeholder.png")
    )

    # 2. Logo (400x200, gray)
    create_png(
        400, 200,
        (128, 128, 128),
        os.path.join(base_path, "T_Logo_Placeholder.png")
    )

    # 3. Loading Background (1920x1080, blue)
    create_png(
        1920, 1080,
        (64, 96, 128),
        os.path.join(base_path, "T_LoadingBackground_Placeholder.png")
    )

    # 4. Loading Spinner (128x128, light gray for visibility)
    create_png(
        128, 128,
        (192, 192, 192),
        os.path.join(base_path, "T_LoadingSpinner_Placeholder.png")
    )

    # 5. Map Preview Default (256x256, gray)
    create_png(
        256, 256,
        (128, 128, 128),
        os.path.join(base_path, "T_MapPreview_Default.png")
    )

    print(f"\n✅ All 5 placeholder textures generated successfully in {base_path}")

if __name__ == "__main__":
    main()
