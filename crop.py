import os
from PIL import Image, ImageEnhance, ImageFilter
import cv2
import numpy as np

try:
    from cv2 import ximgproc
except ImportError:
    print("[⚠️] cv2.ximgproc not found. Falling back to basic edge masking.")
    ximgproc = None


def is_numbered_png(filename):
    return filename.endswith('.png') and filename[:-4].isdigit()


def get_image_paths(folder):
    files = os.listdir(folder)
    numbered_pngs = [f for f in files if is_numbered_png(f)]
    return sorted(
        [os.path.join(folder, f) for f in numbered_pngs],
        key=lambda x: int(os.path.basename(x)[:-4])
    )


def apply_effects(image_path):
    # Step 1: Load image and extract alpha
    original = Image.open(image_path).convert("RGBA")
    alpha = original.getchannel("A")
    rgb = original.convert("RGB")
    rgb_np = np.array(rgb)

    # Step 2: Mean-shift smoothing (50% less intense)
    bgr = cv2.cvtColor(rgb_np, cv2.COLOR_RGB2BGR)
    smoothed = cv2.pyrMeanShiftFiltering(bgr, sp=10, sr=20)
    rgb_sm = cv2.cvtColor(smoothed, cv2.COLOR_BGR2RGB)

    # Step 3: Edge detection and thinning
    gray = cv2.cvtColor(rgb_sm, cv2.COLOR_RGB2GRAY)
    edges = cv2.Canny(gray, 75, 160)
    skeleton = ximgproc.thinning(edges) if ximgproc else cv2.bitwise_and(edges, edges)

    # Step 4: Lightly subtract thin edges
    edge_np = np.array(rgb_sm)
    darken = np.array([30, 30, 30])
    edge_np[skeleton > 0] = np.maximum(edge_np[skeleton > 0] - darken, 0)
    img_pil = Image.fromarray(edge_np)

    # Step 5: Contrast boost
    img_pil = ImageEnhance.Contrast(img_pil).enhance(0.8)

    # Step 6: Slight blur
    img_pil = img_pil.filter(ImageFilter.GaussianBlur(radius=0.30))

    # Step 6.5: Clean color grade — warm lights + cool shadows
    arr = np.array(img_pil).astype(np.float32) / 255.0
    r, g, b = arr[..., 0], arr[..., 1], arr[..., 2]

    luminance = 0.299 * r + 0.587 * g + 0.114 * b
    ui_mask = (r > 0.8) & (g < 0.2) & (b < 0.2)
    scene_mask = ~ui_mask

    # Shadows: add coolness
    shadow = (luminance < 0.4) & scene_mask
    r[shadow] *= 0.90
    g[shadow] *= 0.92
    b[shadow] *= 1.05  # boost blue slightly

    # Highlights: add warmth (yellowish push)
    highlight = (luminance > 0.65) & scene_mask
    r[highlight] *= 1.08
    g[highlight] *= 1.05
    b[highlight] *= 0.97

    # Clip values
    arr[..., 0] = np.clip(r, 0, 1)
    arr[..., 1] = np.clip(g, 0, 1)
    arr[..., 2] = np.clip(b, 0, 1)
    img_pil = Image.fromarray((arr * 255).astype(np.uint8))

    # Step 7: Extended bottom shadow gradient
    arr = np.array(img_pil).astype(np.float32) / 255.0
    h, w = arr.shape[:2]

    y0 = int(h * (0.466))   # start gradient 20% higher
    y1 = int(h * (0.933))   # stretch gradient 20% lower

    mask_raw = Image.new('L', (1, h))
    for y in range(h):
        if y < y0:
            val = 255
        elif y > y1:
            val = int((1.0 - 0.7) * 255)
        else:
            frac = (y - y0) / float(y1 - y0)
            val = int((1.0 - 0.7 * frac) * 255)
        mask_raw.putpixel((0, y), val)
    mask_raw = mask_raw.resize((w, h), resample=Image.BILINEAR)

    # Pad and rotate
    pad = int(max(w, h) * 0.5)
    padded_mask = Image.new('L', (w + pad * 2, h + pad * 2), 255)
    padded_mask.paste(mask_raw, (pad, pad))
    rotated = padded_mask.rotate(10, resample=Image.BILINEAR, expand=False)

    # Crop back, favoring bottom-right
    crop_left = pad - int(0.05 * w)
    crop_top = pad - int(0.05 * h)
    mask_cropped = rotated.crop((crop_left, crop_top, crop_left + w, crop_top + h))

    # Apply gradient
    mask_np = np.array(mask_cropped).astype(np.float32) / 255.0
    arr *= mask_np[..., None]
    img_pil = Image.fromarray((arr * 255).astype(np.uint8))

    # Step 8: Reapply alpha and save
    final = Image.merge("RGBA", (*img_pil.split(), alpha))
    final.save(image_path)
    print(f"[🎨] Effects applied: {image_path}")


def process_folder(folder):
    image_paths = get_image_paths(folder)
    if not image_paths:
        return
    print(f"\n[📁] Processing folder: {folder}")
    for path in image_paths:
        apply_effects(path)


def recursive_search(start_dir):
    for root, _, files in os.walk(start_dir):
        if any(is_numbered_png(f) for f in files):
            process_folder(root)


if __name__ == '__main__':
    base_dir = os.path.abspath(os.path.dirname(__file__))
    print(f"[🚀] Starting cartoon effect pass from: {base_dir}")
    recursive_search(base_dir)
    print("\n[✅] All done.")
