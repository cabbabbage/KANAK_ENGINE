import os
import tkinter as tk
from tkinter import ttk, messagebox
from PIL import Image, ImageTk
import numpy as np
from pages.draw_mode_panel import DrawModePanel
from pages.mask_mode_panel import MaskModePanel


def union_mask_from_frames(frames):
    arrs = [np.array(frame.split()[-1]) > 0 for frame in frames]
    union = np.logical_or.reduce(arrs)
    return union.astype(np.uint8) * 255


class BoundaryConfigurator(tk.Toplevel):
    def __init__(self, master, base_folder, callback):
        super().__init__(master)
        self.title("Boundary Configurator")
        self.callback = callback
        self.state('zoomed')

        # Load PNG frames
        files = sorted(f for f in os.listdir(base_folder) if f.lower().endswith('.png'))
        self.frames = [Image.open(os.path.join(base_folder, f)).convert('RGBA') for f in files]
        if not self.frames:
            self.destroy()
            return

        self.orig_w, self.orig_h = self.frames[0].size

        # Compute bottom-center anchor of opaque region
        mask_full = union_mask_from_frames(self.frames)
        arr = np.array(mask_full) > 0
        rows, cols = np.nonzero(arr)
        if rows.size:
            min_c, max_c = cols.min(), cols.max()
            max_r = rows.max()
        else:
            min_c, max_c, max_r = 0, self.orig_w - 1, self.orig_h - 1
        anchor_x = (min_c + max_c) / 2.0
        anchor_y = float(max_r)
        self.anchor = (anchor_x, anchor_y)

        # Zoom setup
        self.scale = 0.2
        self.zoom_var = tk.DoubleVar(value=self.scale)

        self.mode_frame = None
        self.current_mode = None

        self._build_ui()

    def _build_ui(self):
        self.grid_columnconfigure(0, weight=1)
        self.grid_rowconfigure(1, weight=1)

        ctrl = ttk.Frame(self)
        ctrl.grid(row=0, column=0, sticky='ew', pady=5)
        for mode in ('draw', 'mask'):
            ttk.Button(ctrl, text=mode.title(), command=lambda m=mode: self.select_mode(m))\
                .pack(side='left', padx=10)
        ttk.Label(ctrl, text="Zoom:").pack(side='left', padx=(20, 0))
        tk.Scale(ctrl, from_=0.05, to=2.0, resolution=0.05,
                 variable=self.zoom_var, orient='horizontal',
                 command=self._on_zoom_change, length=300).pack(side='left', padx=5)

        self.mode_container = tk.Frame(self)
        self.mode_container.grid(row=1, column=0, sticky='nsew')

        ttk.Button(self, text="Next", command=self.finish).grid(row=2, column=0, pady=10)

        self.select_mode('draw')

    def _on_zoom_change(self, _=None):
        self.scale = self.zoom_var.get()
        if self.mode_frame and hasattr(self.mode_frame, 'update_zoom'):
            self.mode_frame.update_zoom(self.scale)

    def select_mode(self, mode):
        if self.current_mode == mode:
            return
        if self.mode_frame:
            self.mode_frame.destroy()

        if mode == 'draw':
            self.mode_frame = DrawModePanel(self.mode_container, self.frames, self.scale, self.anchor)
        elif mode == 'mask':
            self.mode_frame = MaskModePanel(self.mode_container, self.frames, self.scale, self.anchor)
        else:
            raise ValueError("Unknown mode: " + mode)

        self.mode_frame.pack(fill='both', expand=True)
        self.current_mode = mode

    def finish(self):
        if not self.mode_frame:
            messagebox.showerror("Error", "No mode active")
            return

        try:
            raw_points = self.mode_frame.get_points()
        except Exception as e:
            messagebox.showerror("Error", f"Failed to extract points: {e}")
            return

        if not raw_points:
            messagebox.showerror("Error", "No boundary points found")
            return

        simplified = self._rdp(raw_points, epsilon=2.0)

        data = {
            "points": simplified,
            "anchor": "bottom_center",
            "anchor_point_in_image": [int(round(self.anchor[0])), int(round(self.anchor[1]))],
            "original_dimensions": [self.orig_w, self.orig_h],
            "type": self.current_mode
        }


        try:
            self.callback(data)
            if hasattr(self.master, "save"):
                self.master.save()
        except Exception as e:
            messagebox.showerror("Error", f"Failed to save: {e}")
        finally:
            self.destroy()

    def _rdp(self, pts, epsilon=2.0):
        if len(pts) < 3:
            return list(pts)
        a, b = pts[0], pts[-1]

        def point_line_dist(p):
            num = (p[0] - a[0])*(b[0] - a[0]) + (p[1] - a[1])*(b[1] - a[1])
            den = (b[0] - a[0])**2 + (b[1] - a[1])**2 + 1e-9
            t = max(0, min(1, num / den))
            proj = (a[0] + t*(b[0] - a[0]), a[1] + t*(b[1] - a[1]))
            return ((p[0] - proj[0])**2 + (p[1] - proj[1])**2)**0.5

        max_d, idx = 0, -1
        for i, p in enumerate(pts[1:-1], start=1):
            d = point_line_dist(p)
            if d > max_d:
                max_d, idx = d, i
        if max_d <= epsilon:
            return [a, b]
        left = self._rdp(pts[:idx + 1], epsilon)
        right = self._rdp(pts[idx:], epsilon)
        return left[:-1] + right
