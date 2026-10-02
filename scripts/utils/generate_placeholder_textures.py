#!/usr/bin/env python3
"""
Generate placeholder texture PNG files for ProjectMenuCore.

Creates 5 placeholder images with appropriate sizes and text overlays.
"""

from PIL import Image, ImageDraw, ImageFont
import os

def create_placeholder(width, height, bg_color, text, output_path):
    """Create a placeholder image with text overlay."""
    img = Image.new('RGB', (width, height), color=bg_color)
    draw = ImageDraw.Draw(img)

    # Try to use a default font, fallback to basic font
    try:
        # Use a reasonable font size based on image dimensions
        font_size = min(width, height) // 15
        font = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", font_size)
    except:
        font = ImageFont.load_default()

    # Calculate text position (centered)
    bbox = draw.textbbox((0, 0), text, font=font)
    text_width = bbox[2] - bbox[0]
    text_height = bbox[3] - bbox[1]
    x = (width - text_width) // 2
    y = (height - text_height) // 2

    # Draw text with shadow for better readability
    shadow_offset = 2
    draw.text((x + shadow_offset, y + shadow_offset), text, fill=(0, 0, 0), font=font)
    draw.text((x, y), text, fill=(255, 255, 255), font=font)

    # Save image
    img.save(output_path, 'PNG')
    print(f"Created: {output_path}")

def create_loading_spinner(size, output_path):
    """Create a circular arrow loading spinner."""
    img = Image.new('RGBA', (size, size), color=(0, 0, 0, 0))
    draw = ImageDraw.Draw(img)

    # Draw circular arrow (simple arc representation)
    padding = size // 8
    draw.arc(
        [(padding, padding), (size - padding, size - padding)],
        start=30,
        end=330,
        fill=(255, 255, 255),
        width=size // 16
    )

    # Draw arrow head (triangle)
    arrow_size = size // 8
    arrow_x = size // 2 + size // 3
    arrow_y = padding
    draw.polygon(
        [(arrow_x, arrow_y),
         (arrow_x - arrow_size, arrow_y + arrow_size),
         (arrow_x + arrow_size, arrow_y + arrow_size)],
        fill=(255, 255, 255)
    )

    img.save(output_path, 'PNG')
    print(f"Created: {output_path}")

def main():
    """Generate all placeholder textures."""
    base_path = "Plugins/GameFeatures/ProjectMenuCore/Content/UI/Textures/Placeholders"

    # Ensure directory exists
    os.makedirs(base_path, exist_ok=True)

    # 1. Menu Background (1920x1080, gray)
    create_placeholder(
        1920, 1080,
        (128, 128, 128),
        "Menu Background",
        os.path.join(base_path, "T_MenuBackground_Placeholder.png")
    )

    # 2. Logo (400x200, gray)
    create_placeholder(
        400, 200,
        (128, 128, 128),
        "Logo",
        os.path.join(base_path, "T_Logo_Placeholder.png")
    )

    # 3. Loading Background (1920x1080, blue)
    create_placeholder(
        1920, 1080,
        (64, 96, 128),
        "Loading...",
        os.path.join(base_path, "T_LoadingBackground_Placeholder.png")
    )

    # 4. Loading Spinner (128x128, circular arrow)
    create_loading_spinner(
        128,
        os.path.join(base_path, "T_LoadingSpinner_Placeholder.png")
    )

    # 5. Map Preview Default (256x256, gray)
    create_placeholder(
        256, 256,
        (128, 128, 128),
        "Map Preview",
        os.path.join(base_path, "T_MapPreview_Default.png")
    )

    print("\n✅ All placeholder textures generated successfully")

if __name__ == "__main__":
    main()
