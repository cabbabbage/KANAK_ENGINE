import os
from PIL import Image, ImageEnhance
import cv2
import numpy as np
from scipy.interpolate import RegularGridInterpolator

def is_numbered_png(filename):
    return filename.endswith('.png') and filename[:-4].isdigit()

def get_image_paths(folder):
    files = os.listdir(folder)
    numbered_pngs = [f for f in files if is_numbered_png(f)]
    return sorted(
        [os.path.join(folder, f) for f in numbered_pngs],
        key=lambda x: int(os.path.basename(x)[:-4])
    )

def get_crop_bounds(image_paths):
    images = [Image.open(path).convert("RGBA") for path in image_paths]
    width, height = images[0].size
    top_crop = bottom_crop = left_crop = right_crop = 0

    for y in range(height):
        if all(all(img.getpixel((x, y))[3] == 0 for x in range(width)) for img in images):
            top_crop += 1
        else:
            break

    for y in range(height - 1, -1, -1):
        if all(all(img.getpixel((x, y))[3] == 0 for x in range(width)) for img in images):
            bottom_crop += 1
        else:
            break

    for x in range(width):
        if all(all(img.getpixel((x, y))[3] == 0 for y in range(height)) for img in images):
            left_crop += 1
        else:
            break

    for x in range(width - 1, -1, -1):
        if all(all(img.getpixel((x, y))[3] == 0 for y in range(height)) for img in images):
            right_crop += 1
        else:
            break

    for img in images:
        img.close()

    return top_crop, bottom_crop, left_crop, right_crop

def load_cube_lut(cube_path):
    lut_data = []
    size = None
    with open(cube_path, 'r') as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            if "LUT_3D_SIZE" in line:
                size = int(line.split()[-1])
            elif len(line.split()) == 3:
                lut_data.append(list(map(float, line.split())))

    if size is None or len(lut_data) != size**3:
        print("[⚠️] Invalid or unsupported LUT file.")
        return None

    lut_array = np.array(lut_data).reshape((size, size, size, 3))
    grid = np.linspace(0, 1, size)
    return RegularGridInterpolator((grid, grid, grid), lut_array, bounds_error=False, fill_value=None)

def apply_effects(image_path, lut=None):
    original = Image.open(image_path).convert("RGBA")
    alpha = original.getchannel("A")
    rgb = original.convert("RGB")
    rgb_np = np.array(rgb).astype(np.float32) / 255.0

    if lut is not None:
        r, g, b = rgb_np[..., 0], rgb_np[..., 1], rgb_np[..., 2]
        coords = np.stack([r, g, b], axis=-1).reshape(-1, 3)
        mapped = lut(coords).reshape(rgb_np.shape)
        mapped = np.clip(mapped, 0, 1)
        blended = (0.7 * rgb_np + 0.3 * mapped)
    else:
        blended = rgb_np

    blended_img = Image.fromarray((blended * 255).astype(np.uint8))
    contrast = ImageEnhance.Contrast(blended_img).enhance(0.90)

    img = cv2.cvtColor(np.array(contrast), cv2.COLOR_RGB2BGR)
    gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
    edges = cv2.Canny(gray, 50, 150)
    kernel = cv2.getStructuringElement(cv2.MORPH_RECT, (1, 1))
    edges = cv2.dilate(edges, kernel)
    img[edges > 0] = (0, 0, 0)

    rgb_result = Image.fromarray(cv2.cvtColor(img, cv2.COLOR_BGR2RGB))
    rgba_result = Image.merge("RGBA", (*rgb_result.split(), alpha))
    rgba_result.save(image_path)
    print(f"[🎨] Effects applied: {image_path}")

def crop_and_save_images(image_paths, crop_top, crop_bottom, crop_left, crop_right, lut):
    for path in image_paths:
        with Image.open(path).convert("RGBA") as img:
            width, height = img.size
            left = crop_left
            top = crop_top
            right = width - crop_right
            bottom = height - crop_bottom

            if left >= right or top >= bottom:
                print(f"[!] Skipping {path}: crop dimensions result in empty image")
                continue

            cropped = img.crop((left, top, right, bottom))
            cropped.save(path)
            print(f"[✓] Cropped and saved: {path}")

        apply_effects(path, lut=lut)

def process_folder(folder, lut):
    image_paths = get_image_paths(folder)
    if not image_paths:
        return

    print(f"\n[📁] Processing folder: {folder}")
    crop_top, crop_bottom, crop_left, crop_right = get_crop_bounds(image_paths)
    print(f"    Crop (T:{crop_top}, B:{crop_bottom}, L:{crop_left}, R:{crop_right})")

    if crop_top == 0 and crop_bottom == 0 and crop_left == 0 and crop_right == 0:
        print("    [→] No cropping needed. Applying effects to all images.")
        for path in image_paths:
            apply_effects(path, lut=lut)
        return

    crop_and_save_images(image_paths, crop_top, crop_bottom, crop_left, crop_right, lut)

def recursive_search(start_dir, lut):
    for root, _, files in os.walk(start_dir):
        if any(is_numbered_png(f) for f in files):
            process_folder(root, lut)

if __name__ == "__main__":
    base_dir = os.path.dirname(os.path.abspath(__file__))
    print(f"[🚀] Starting recursive crop+effects from: {base_dir}")

    cube_lut_path = r"C:\\ProgramData\\Blackmagic Design\\DaVinci Resolve\\Support\\LUT\\Blackmagic Design\\Blackmagic Broadcast Film to Rec2020 PQ v4.cube"
    lut = None
    if os.path.isfile(cube_lut_path):
        print(f"[🎛️] Loading .cube LUT: {cube_lut_path}")
        lut = load_cube_lut(cube_lut_path)
        if lut is not None:
            print("[✅] LUT successfully loaded.")
        else:
            print("[⚠️] Failed to process LUT.")
    else:
        print(f"[ℹ️] No LUT file found at {cube_lut_path}")

    recursive_search(base_dir, lut)
    print("\n[✅] All done.")
