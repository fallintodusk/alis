#!/usr/bin/env python3
"""
Generate placeholder texture PNG files (small sizes for fast generation).

Creates small solid-color PNG files using only Python standard library.
These are minimal placeholders - designers can replace with final art.
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

    print(f"Created: {output_path} ({width}x{height}, {len(png_data)} bytes)")

def main():
    """Generate all placeholder textures (small sizes for fast generation)."""
    base_path = "Plugins/GameFeatures/ProjectMenuCore/Content/UI/Textures/Placeholders"

    # Ensure directory exists
    os.makedirs(base_path, exist_ok=True)

    # Use smaller sizes for fast generation - UE can scale them up for placeholders
    # Designers can replace with final high-res art

    # 1. Menu Background (64x64, gray) - scaled from 1920x1080 requirement
    create_png(
        64, 64,
        (128, 128, 128),
        os.path.join(base_path, "T_MenuBackground_Placeholder.png")
    )

    # 2. Logo (64x32, gray) - scaled from 400x200 requirement
    create_png(
        64, 32,
        (128, 128, 128),
        os.path.join(base_path, "T_Logo_Placeholder.png")
    )

    # 3. Loading Background (64x64, blue) - scaled from 1920x1080 requirement
    create_png(
        64, 64,
        (64, 96, 128),
        os.path.join(base_path, "T_LoadingBackground_Placeholder.png")
    )

    # 4. Loading Spinner (64x64, light gray) - scaled from 128x128 requirement
    create_png(
        64, 64,
        (192, 192, 192),
        os.path.join(base_path, "T_LoadingSpinner_Placeholder.png")
    )

    # 5. Map Preview Default (64x64, gray) - scaled from 256x256 requirement
    create_png(
        64, 64,
        (128, 128, 128),
        os.path.join(base_path, "T_MapPreview_Default.png")
    )

    print(f"\n✅ All 5 placeholder textures generated successfully")
    print(f"Location: {base_path}")
    print(f"\nNOTE: These are minimal placeholders (64x64 or similar).")
    print(f"      Designers can replace with final high-resolution art.")

if __name__ == "__main__":
    main()
