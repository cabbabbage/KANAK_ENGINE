import os
from PIL import Image, ImageEnhance, ImageFilter
import cv2
import numpy as np

try:
    from cv2 import ximgproc
except ImportError:
    print("[⚠️] cv2.ximgproc not found. Falling back to basic edge masking.")
    ximgproc = None


def is_numbered_bmp(filename):
    return filename.endswith('.bmp') and filename[:-4].isdigit()


def get_image_paths(folder):
    files = os.listdir(folder)
    numbered_bmps = [f for f in files if is_numbered_bmp(f)]
    return sorted(
        [os.path.join(folder, f) for f in numbered_bmps],
        key=lambda x: int(os.path.basename(x)[:-4])
    )


def normalize_strength(strength):
    return np.clip(strength / 100.0, 0.0, 1.0)


def step_1_load_image(image_path):
    img = Image.open(image_path).convert("RGBA")
    alpha = img.getchannel("A")
    rgb_img = img.convert("RGB")
    return rgb_img, alpha


def step_2_smooth_mean_shift(img, strength):
    s = normalize_strength(strength)
    sp = int(1 + 20 * s)
    sr = int(1 + 40 * s)
    np_rgb = np.array(img)
    bgr = cv2.cvtColor(np_rgb, cv2.COLOR_RGB2BGR)
    smoothed = cv2.pyrMeanShiftFiltering(bgr, sp=sp, sr=sr)
    rgb_sm = cv2.cvtColor(smoothed, cv2.COLOR_BGR2RGB)
    return Image.fromarray(rgb_sm)


def step_3_edge_thin(img):
    np_rgb = np.array(img)
    gray = cv2.cvtColor(np_rgb, cv2.COLOR_RGB2GRAY)
    edges = cv2.Canny(gray, 75, 160)
    skeleton = ximgproc.thinning(edges) if ximgproc else cv2.bitwise_and(edges, edges)
    return skeleton


def step_4_subtract_edges(img, skeleton, strength):
    s = normalize_strength(strength)
    darken = np.array([int(30 * s)] * 3)
    arr = np.array(img)
    arr[skeleton > 0] = np.maximum(arr[skeleton > 0] - darken, 0)
    return Image.fromarray(arr)


def step_5_boost_contrast(img, strength):
    s = normalize_strength(strength)
    return ImageEnhance.Contrast(img).enhance(1.0 + 0.4 * s)


def step_6_apply_blur(img, strength):
    s = normalize_strength(strength)
    return img.filter(ImageFilter.GaussianBlur(radius=0.3 * s))


def step_6_5_color_grade(img, strength):
    s = normalize_strength(strength)
    arr = np.array(img).astype(np.float32) / 255.0
    r, g, b = arr[..., 0], arr[..., 1], arr[..., 2]
    luminance = 0.299 * r + 0.587 * g + 0.114 * b

    ui_mask = (r > 0.8) & (g < 0.2) & (b < 0.2)
    scene_mask = ~ui_mask

    shadow = (luminance < 0.4) & scene_mask
    r[shadow] *= 1.0 - 0.1 * s
    g[shadow] *= 1.0 - 0.08 * s
    b[shadow] *= 1.0 + 0.05 * s

    highlight = (luminance > 0.65) & scene_mask
    r[highlight] *= 1.0 + 0.08 * s
    g[highlight] *= 1.0 + 0.05 * s
    b[highlight] *= 1.0 - 0.03 * s

    arr[..., 0] = np.clip(r, 0, 1)
    arr[..., 1] = np.clip(g, 0, 1)
    arr[..., 2] = np.clip(b, 0, 1)
    return Image.fromarray((arr * 255).astype(np.uint8))


def step_7_shadow_gradient(img, strength):
    s = normalize_strength(strength)
    arr = np.array(img).astype(np.float32) / 255.0
    h, w = arr.shape[:2]
    y0 = int(h * (0.466 - 0.1 * s))
    y1 = int(h * (0.933 + 0.1 * s))

    mask_raw = Image.new('L', (1, h))
    for y in range(h):
        if y < y0:
            val = 255
        elif y > y1:
            val = int((1.0 - 0.7 * s) * 255)
        else:
            frac = (y - y0) / float(y1 - y0)
            val = int((1.0 - 0.7 * s * frac) * 255)
        mask_raw.putpixel((0, y), val)

    mask_raw = mask_raw.resize((w, h), resample=Image.BILINEAR)
    pad = int(max(w, h) * 0.5)
    padded_mask = Image.new('L', (w + pad * 2, h + pad * 2), 255)
    padded_mask.paste(mask_raw, (pad, pad))
    rotated = padded_mask.rotate(10, resample=Image.BILINEAR, expand=False)
    crop_left = pad - int(0.05 * w)
    crop_top = pad - int(0.05 * h)
    mask_cropped = rotated.crop((crop_left, crop_top, crop_left + w, crop_top + h))

    mask_np = np.array(mask_cropped).astype(np.float32) / 255.0
    arr *= mask_np[..., None]
    return Image.fromarray((arr * 255).astype(np.uint8))


def step_8_reapply_alpha(img, alpha, save_path):
    final = Image.merge("RGBA", (*img.split(), alpha))
    final.save(save_path)
    print(f"[🎨] Effects applied: {save_path}")


def apply_effects(image_path, strength=100):
    img, alpha = step_1_load_image(image_path)
    img = step_2_smooth_mean_shift(img, strength)
    skeleton = step_3_edge_thin(img)
    img = step_4_subtract_edges(img, skeleton, strength)
    img = step_5_boost_contrast(img, strength)
    img = step_6_apply_blur(img, strength)
    img = step_6_5_color_grade(img, strength)
    img = step_7_shadow_gradient(img, strength)
    step_8_reapply_alpha(img, alpha, image_path)


def process_folder(folder, strength=100):
    image_paths = get_image_paths(folder)
    if not image_paths:
        return
    print(f"\n[📁] Processing folder: {folder}")
    for path in image_paths:
        apply_effects(path, strength)


def recursive_search(start_dir, strength=100):
    for root, _, files in os.walk(start_dir):
        if any(is_numbered_bmp(f) for f in files):
            process_folder(root, strength)


if __name__ == '__main__':
    base_dir = os.path.abspath("cache")
    print(f"[🚀] Starting cartoon effect pass from: {base_dir}")
    recursive_search(base_dir, strength=100)
    print("\n[✅] All done.")
