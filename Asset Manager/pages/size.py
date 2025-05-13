# pages/size.py

import os
import json
import tkinter as tk
from tkinter import ttk, messagebox
from PIL import Image, ImageTk

class SizePage(ttk.Frame):
    def __init__(self, parent):
        super().__init__(parent)

        # Theme & fonts
        self.FONT            = ('Segoe UI', 14)
        self.FONT_BOLD       = ('Segoe UI', 18, 'bold')
        self.MAIN_COLOR      = "#005f73"
        self.SECONDARY_COLOR = "#ee9b00"

        # State & images
        self.asset_path    = None
        self._orig_img     = None
        self._fit_img      = None

        # rename base_pct to scale_percentage, var_pct to variability_percentage
        self.scale_percentage_var       = tk.DoubleVar(value=100)   # 10–150%
        self.variability_percentage_var = tk.DoubleVar(value=0)     # 0–20%

        # Styles
        style = ttk.Style(self)
        style.configure('Main.TButton', font=self.FONT, padding=6,
                        background=self.MAIN_COLOR, foreground='black')
        style.map('Main.TButton', background=[('active', self.SECONDARY_COLOR)])
        style.configure('Large.TLabel',     font=self.FONT)
        style.configure('LargeBold.TLabel', font=self.FONT_BOLD,
                        foreground=self.SECONDARY_COLOR)

        # Layout columns
        for c in range(4):
            self.columnconfigure(c, weight=1)

        # Title
        ttk.Label(self, text="Size Settings", style='LargeBold.TLabel') \
            .grid(row=0, column=0, columnspan=4, pady=(10,20), padx=12)

        # Preview (16:9)
        self.CANVAS_W, self.CANVAS_H = 480, 270
        self.preview = tk.Canvas(self, width=self.CANVAS_W, height=self.CANVAS_H,
                                 bg='black', bd=2, relief='sunken')
        self.preview.grid(row=1, column=0, columnspan=4, padx=12, pady=6)

        # Scale Slider
        ttk.Label(self, text="Scale (%)", style='Large.TLabel') \
            .grid(row=2, column=0, sticky="e", padx=12, pady=6)
        ttk.Scale(self, from_=1, to=150, variable=self.scale_percentage_var,
                  orient='horizontal', command=lambda _v: self._rescale()) \
            .grid(row=2, column=1, columnspan=3, sticky="we", padx=12, pady=6)

        # Variability Slider
        ttk.Label(self, text="Size Variability (%)", style='Large.TLabel') \
            .grid(row=3, column=0, sticky="e", padx=12, pady=6)
        ttk.Scale(self, from_=0, to=20, variable=self.variability_percentage_var,
                  orient='horizontal', command=lambda _v: self._rescale()) \
            .grid(row=3, column=1, columnspan=3, sticky="we", padx=12, pady=6)

        # Min/Max previews
        ttk.Label(self, text="Min Size Preview", style='Large.TLabel') \
            .grid(row=4, column=0, pady=(10,0))
        ttk.Label(self, text="Max Size Preview", style='Large.TLabel') \
            .grid(row=4, column=2, pady=(10,0))

        self.preview_min = tk.Canvas(self, width=self.CANVAS_W//2, height=self.CANVAS_H//2,
                                     bg='black', bd=1, relief='sunken')
        self.preview_min.grid(row=5, column=0, columnspan=2, padx=6, pady=6)

        self.preview_max = tk.Canvas(self, width=self.CANVAS_W//2, height=self.CANVAS_H//2,
                                     bg='black', bd=1, relief='sunken')
        self.preview_max.grid(row=5, column=2, columnspan=2, padx=6, pady=6)

        # Save Button
        ttk.Button(self, text="Save", command=self.save, style='Main.TButton') \
            .grid(row=6, column=0, columnspan=4, pady=(20,10), padx=12)

    def load(self, info_path):
        """Load JSON and default image, then prepare fitted preview."""
        self.asset_path = info_path
        if not info_path:
            return

        if not os.path.exists(info_path):
            with open(info_path, 'w') as f:
                json.dump({}, f)

        with open(info_path, 'r') as f:
            data = json.load(f)

        ss = data.get("size_settings", {})
        self.scale_percentage_var.set(ss.get("scale_percentage", 100))
        self.variability_percentage_var.set(ss.get("variability_percentage", 0))

        base_dir = os.path.dirname(info_path)
        img_p = os.path.join(base_dir, "default", "0.png")
        if os.path.isfile(img_p):
            self._orig_img = Image.open(img_p).convert('RGBA')
        else:
            self._orig_img = None

        if self._orig_img:
            bw, bh = self._orig_img.size
            fit_scale = min(self.CANVAS_W/bw, self.CANVAS_H/bh)
            disp = (int(bw*fit_scale), int(bh*fit_scale))
            self._fit_img = self._orig_img.resize(disp, Image.LANCZOS)
        else:
            self._fit_img = None

        self._rescale()

    def _rescale(self):
        """Update main preview and min/max previews."""
        self.preview.delete("all")
        if not self._fit_img:
            return

        base_pct = self.scale_percentage_var.get()
        var_pct  = self.variability_percentage_var.get()

        # main preview
        scale = base_pct/100.0
        self._draw_to_canvas(self.preview, scale)

        # min preview: (base - var)
        min_scale = max(0, (base_pct - var_pct)/100.0)
        self._draw_to_canvas(self.preview_min, min_scale)

        # max preview: (base + var)
        max_scale = (base_pct + var_pct)/100.0
        self._draw_to_canvas(self.preview_max, max_scale)

    def _draw_to_canvas(self, canvas, scale):
        canvas.delete("all")
        w, h = self._fit_img.size
        sw, sh = int(w * scale), int(h * scale)
        if sw<=0 or sh<=0:
            return
        scaled = self._fit_img.resize((sw, sh), Image.LANCZOS)
        cw = int(canvas['width']); ch = int(canvas['height'])
        x = (cw - sw)//2; y = (ch - sh)//2
        tk_img = ImageTk.PhotoImage(scaled)
        canvas.create_image(x, y, anchor='nw', image=tk_img)
        setattr(self, f'_tk_{id(canvas)}', tk_img)  # keep ref

    def save(self):
        """Write size settings back to JSON and rescale folder if needed."""
        if not self.asset_path:
            messagebox.showerror("Error", "No asset selected.")
            return

        base_pct = self.scale_percentage_var.get()
        scale = base_pct / 100.0

        # load JSON
        with open(self.asset_path, 'r') as f:
            data = json.load(f)

        last = data.get('last_scaled_to')
        # nothing to do if already scaled to this percentage
        if last == base_pct:
            # still update z_threshold and size_settings
            self._update_json_fields(data, scale=False)
            with open(self.asset_path, 'w') as f:
                json.dump(data, f, indent=4)
            messagebox.showinfo("Saved", "Size settings saved (no rescale needed).")
            return

        # update JSON fields, mark for rescaling
        self._update_json_fields(data, scale=True)
        with open(self.asset_path, 'w') as f:
            json.dump(data, f, indent=4)

        # if scale changed, run full rescale of images & geometry
        asset_dir = os.path.dirname(self.asset_path)
        self._rescale_images(asset_dir, scale)
        self._rescale_geometry_jsons(asset_dir, scale)

        messagebox.showinfo("Saved", "Size settings updated and content rescaled.")

    def _update_json_fields(self, data, scale: bool):
        """Update z_threshold, size_settings, last_scaled_to."""
        # set z_threshold if missing or null
        if data.get('z_threshold') is None:
            try:
                img = Image.open(os.path.join(os.path.dirname(self.asset_path),
                                              'default','0.png'))
                z_val = int(img.height * 4 / 5)
            except:
                z_val = 0
            data['z_threshold'] = z_val

        # update size_settings
        data['size_settings'] = {
            'scale_percentage':       self.scale_percentage_var.get(),
            'variability_percentage': self.variability_percentage_var.get(),
        }

        # record last_scaled_to only if we’re actually going to rescale
        if scale:
            data['last_scaled_to'] = self.scale_percentage_var.get()

    def _rescale_images(self, folder: str, scale: float):
        """Recursively resize every image in the asset folder."""
        exts = ('.png','.jpg','.jpeg','.gif')
        for root, _, files in os.walk(folder):
            for fn in files:
                if fn.lower().endswith(exts):
                    full = os.path.join(root, fn)
                    try:
                        img = Image.open(full)
                        w,h = img.size
                        nw, nh = int(round(w*scale)), int(round(h*scale))
                        if (nw,nh) != (w,h):
                            img.resize((nw,nh), Image.LANCZOS).save(full)
                    except:
                        pass

    def _rescale_geometry_jsons(self, folder: str, scale: float):
        """Scale every [x,y] in any .json (except info.json) in top folder."""
        for fn in os.listdir(folder):
            if not fn.lower().endswith('.json') or fn=='info.json':
                continue
            full = os.path.join(folder, fn)
            try:
                with open(full,'r') as f:
                    data = json.load(f)
                scaled = self._scale_geometry(data, scale)
                with open(full,'w') as f:
                    json.dump(scaled, f, indent=2)
            except:
                pass

    def _scale_geometry(self, obj, scale):
        """Recursively walk and scale any [x,y] lists."""
        if isinstance(obj, list):
            if len(obj)==2 and all(isinstance(c,(int,float)) for c in obj):
                return [int(round(obj[0]*scale)), int(round(obj[1]*scale))]
            return [self._scale_geometry(e, scale) for e in obj]
        if isinstance(obj, dict):
            return {k: self._scale_geometry(v, scale) for k,v in obj.items()}
        return obj
