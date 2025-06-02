# pages/spacing.py
from pages.button import BlueButton  
from pages.range import Range

import os
import json
import tkinter as tk
from tkinter import ttk, messagebox
from pages.boundary import BoundaryConfigurator
from PIL import Image, ImageTk, ImageDraw

class SpacingThresholdPage(ttk.Frame):
    def __init__(self, parent):
        super().__init__(parent)

        FONT            = ('Segoe UI', 14)
        FONT_BOLD       = ('Segoe UI', 18, 'bold')
        MAIN_COLOR      = "#005f73"
        SECONDARY_COLOR = "#ee9b00"

        self.asset_path     = None
        self.area_geo       = None
        self.frames_source  = None

        style = ttk.Style(self)
        style.configure('Main.TButton', font=FONT, padding=6,
                        background=MAIN_COLOR, foreground='black')
        style.map('Main.TButton',
                  background=[('active', SECONDARY_COLOR)])
        style.configure('Large.TLabel',     font=FONT)
        style.configure('LargeBold.TLabel', font=FONT_BOLD,
                        foreground=SECONDARY_COLOR)

        for c in range(3):
            self.columnconfigure(c, weight=1)

        ttk.Label(self, text="Spacing Threshold", style='LargeBold.TLabel')\
            .grid(row=0, column=0, columnspan=3, pady=(10,20))

        ttk.Label(self, text="Forbidden Spawn Region:", style='Large.TLabel')\
            .grid(row=1, column=0, sticky='e', padx=12, pady=6)
        self.btn_draw = BlueButton(self, "Draw…", command=self._configure_area, x=50, y=10)

        # Minimum same-type distance using Range
        ttk.Label(self, text="Min Distance From Same Type:", style='Large.TLabel')\
            .grid(row=4, column=0, sticky='e', padx=12, pady=6)
        self.min_dist_range = Range(self, min_bound=0, max_bound=200, set_min=0, set_max=0, force_fixed=True)
        self.min_dist_range.grid(row=4, column=1, columnspan=2, sticky='we', padx=12, pady=6)

        self.preview = tk.Canvas(self, bg='black', bd=2, relief='sunken')
        self.preview.grid(row=5, column=0, columnspan=3,
                          padx=12, pady=12)

        BlueButton(self, "Save", command=self.save, x=0, y=0)

    def _configure_area(self):
        if not self.frames_source:
            messagebox.showerror("Error", "Select frames folder in Basic Info first.")
            return
        BoundaryConfigurator(self,
                             base_folder=self.frames_source,
                             callback=self._on_boundary)

    def _on_boundary(self, geo):
        if isinstance(geo, list):
            geo = {'type': 'mask', 'points': geo}
        self.area_geo = geo
        self._draw_preview()

    def _draw_preview(self):
        if not self.asset_path or not self.area_geo:
            return

        asset_dir = os.path.dirname(self.asset_path)
        p = os.path.join(asset_dir, 'default', '0.png')
        if not os.path.isfile(p):
            return

        img = Image.open(p).convert('RGBA')
        scale = 0.5
        disp = (int(img.width * scale), int(img.height * scale))
        base = img.resize(disp, Image.LANCZOS)

        overlay = Image.new('RGBA', disp, (0, 0, 0, 0))
        draw = ImageDraw.Draw(overlay)
        geo = self.area_geo

        if geo['type'] == 'circle':
            cx = int(geo['x'] * scale)
            cy = int(geo['y'] * scale)
            rw = int((geo['w'] * scale) / 2)
            rh = int((geo['h'] * scale) / 2)
            draw.ellipse((cx - rw, cy - rh, cx + rw, cy + rh), fill=(255, 0, 0, 128))
        else:
            anchor_x, anchor_y = geo.get('original_anchor', [0, 0])
            for ox, oy in geo.get('points', []):
                abs_x = ox + anchor_x
                abs_y = oy + anchor_y
                dx = int(abs_x * scale)
                dy = int(abs_y * scale)
                if 0 <= dx < disp[0] and 0 <= dy < disp[1]:
                    overlay.putpixel((dx, dy), (255, 0, 0, 128))

        comp = Image.alpha_composite(base, overlay)
        self._tk = ImageTk.PhotoImage(comp)
        self.preview.config(width=disp[0], height=disp[1])
        self.preview.delete("all")
        self.preview.create_image(0, 0, anchor='nw', image=self._tk)

    def load(self, info_path):
        self.asset_path = info_path
        if not info_path:
            return

        asset_dir = os.path.dirname(info_path)

        try:
            with open(info_path, 'r') as f:
                data = json.load(f)
        except Exception as e:
            messagebox.showerror("Error", f"Could not load info.json:\n{e}")
            return

        self.frames_source = os.path.join(asset_dir, "default")
        if not os.path.isdir(self.frames_source):
            print(f"[Warning] Default frame folder missing: {self.frames_source}")
            self.frames_source = None

        fn = data.get('spacing_area')
        if fn:
            full = os.path.join(asset_dir, fn)
            if os.path.isfile(full):
                try:
                    with open(full, 'r') as f:
                        self.area_geo = json.load(f)
                except Exception:
                    self.area_geo = None

        # Load range
        dist = data.get('min_same_type_distance', 0)
        self.min_dist_range.set(dist, dist)


        self._draw_preview()

    def save(self):
        if not self.asset_path:
            messagebox.showerror("Error", "No asset selected.")
            return

        if not self.area_geo:
            messagebox.showerror("Error", "Draw a forbidden region before saving.")
            return

        asset_dir = os.path.dirname(self.asset_path)
        fn = "spacing_area.json"
        with open(os.path.join(asset_dir, fn), 'w') as f:
            json.dump(self.area_geo, f, indent=2)

        with open(self.asset_path, 'r') as f:
            info = json.load(f)
        info['spacing_area'] = fn

        min_val, max_val = self.min_dist_range.get()
        if min_val == max_val:
            info['min_same_type_distance'] = min_val
        else:
            info['min_same_type_distance'] = [min_val, max_val]

        with open(self.asset_path, 'w') as f:
            json.dump(info, f, indent=2)

        messagebox.showinfo("Saved", "Spacing threshold saved.")