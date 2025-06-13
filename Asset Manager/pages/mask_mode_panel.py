import tkinter as tk
from tkinter import ttk
from PIL import Image, ImageTk, ImageFilter
import numpy as np


class MaskModePanel(tk.Frame):
    def __init__(self, parent, frames, scale, anchor):
        super().__init__(parent)
        self.frames = frames
        self.scale = scale
        self.anchor = anchor
        self.anchor_x, self.anchor_y = anchor
        self.orig_w, self.orig_h = self.frames[0].size

        self.disp_w = int(self.orig_w * self.scale)
        self.disp_h = int(self.orig_h * self.scale)

        self._build_ui()
        self._prepare_images()

    def _build_ui(self):
        self.canvas = tk.Canvas(self, bg='black')
        self.canvas.pack(fill='both', expand=True)

        self.slider_frame = ttk.Frame(self)
        self.slider_frame.pack(fill='x', pady=6)

        self.sliders = {}
        for key in ('top_pct', 'bottom_pct', 'left_pct', 'right_pct'):
            self._add_slider(key, 0, 100, 0)

    def _add_slider(self, name, mn, mx, val):
        ttk.Label(self.slider_frame, text=name).pack(side='left')
        var = tk.IntVar(value=val)
        slider = tk.Scale(self.slider_frame, from_=mn, to=mx,
                          orient='horizontal', variable=var,
                          command=lambda v: self._refresh_preview())
        slider.pack(side='left', fill='x', expand=True, padx=4)
        self.sliders[name] = slider

    def _prepare_images(self):
        self.mask_arr = self._get_union_mask_arr()
        self.base_img = self.frames[0].resize((self.disp_w, self.disp_h), Image.LANCZOS)
        self.tk_base = ImageTk.PhotoImage(self.base_img)
        self.img_id = self.canvas.create_image(0, 0, anchor='nw', image=self.tk_base)
        self._refresh_preview()

    def _get_union_mask_arr(self):
        alpha_layers = [np.array(frame.split()[-1]) > 0 for frame in self.frames]
        return np.logical_or.reduce(alpha_layers)

    def _refresh_preview(self):
        arr = self.mask_arr.copy()
        h0, w0 = arr.shape
        top = self.sliders['top_pct'].get() / 100.0
        bot = self.sliders['bottom_pct'].get() / 100.0
        left = self.sliders['left_pct'].get() / 100.0
        right = self.sliders['right_pct'].get() / 100.0

        arr[:int(top * h0), :] = False
        arr[int((1 - bot) * h0):, :] = False
        arr[:, :int(left * w0)] = False
        arr[:, int((1 - right) * w0):] = False

        overlay = np.zeros((h0, w0, 4), dtype=np.uint8)
        overlay[arr, 0] = 255
        overlay[arr, 3] = 128
        ov_img = Image.fromarray(overlay, 'RGBA').resize((self.disp_w, self.disp_h), Image.NEAREST)

        comp = self.base_img.copy()
        comp.paste(ov_img, (0, 0), ov_img)
        self.tk_overlay = ImageTk.PhotoImage(comp)
        self.canvas.itemconfig(self.img_id, image=self.tk_overlay)

        self.cropped_mask = arr

    def _extract_edge_pixels(self, mask_arr):
        img = Image.fromarray((mask_arr.astype(np.uint8)) * 255)
        edges = img.filter(ImageFilter.FIND_EDGES)
        arr = np.array(edges) > 0
        pts = list(zip(*np.nonzero(arr)[::-1]))
        return [(int(x), int(y)) for x, y in pts]

    def get_points(self):
        # Scale back to original size
        pts = self._extract_edge_pixels(self.cropped_mask)
        return [
            (x - self.anchor_x, y - self.anchor_y)
            for x, y in pts
        ]

    def update_zoom(self, new_scale):
        self.scale = new_scale
        self.disp_w = int(self.orig_w * self.scale)
        self.disp_h = int(self.orig_h * self.scale)
        self._prepare_images()
