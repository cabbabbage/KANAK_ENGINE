import os
import json
import tkinter as tk
from tkinter import ttk, messagebox
from PIL import Image, ImageTk

class SizePage(ttk.Frame):
    def __init__(self, parent):
        super().__init__(parent)
        self.FONT = ('Segoe UI', 14)
        self.FONT_BOLD = ('Segoe UI', 18, 'bold')
        self.MAIN_COLOR = "#005f73"
        self.SECONDARY_COLOR = "#ee9b00"

        self.asset_path = None
        self._orig_img = None
        self._fit_img = None

        # variables
        self.scale_percentage_var = tk.DoubleVar(value=100)
        self.variability_percentage_var = tk.DoubleVar(value=0)
        self.z_threshold_var = tk.DoubleVar(value=0)

        # styles
        style = ttk.Style(self)
        style.configure('Main.TButton', font=self.FONT, padding=6,
                        background=self.MAIN_COLOR, foreground='black')
        style.map('Main.TButton', background=[('active', self.SECONDARY_COLOR)])
        style.configure('Large.TLabel', font=self.FONT)
        style.configure('LargeBold.TLabel', font=self.FONT_BOLD, foreground=self.SECONDARY_COLOR)

        # layout columns
        for c in range(4):
            self.columnconfigure(c, weight=1)

        # title
        ttk.Label(self, text="Size Settings", style='LargeBold.TLabel') \
            .grid(row=0, column=0, columnspan=4, pady=(10, 20), padx=12)

        # main preview canvas
        self.CANVAS_W, self.CANVAS_H = 480, 270
        self.preview = tk.Canvas(self, width=self.CANVAS_W, height=self.CANVAS_H,
                                 bg='black', bd=2, relief='sunken')
        self.preview.grid(row=1, column=0, columnspan=4, padx=12, pady=6)

        # Scale slider
        ttk.Label(self, text="Scale (%)", style='Large.TLabel') \
            .grid(row=2, column=0, sticky="e", padx=12, pady=6)
        ttk.Scale(self, from_=1, to=150, variable=self.scale_percentage_var,
                  orient='horizontal', command=lambda _v: self._rescale()) \
            .grid(row=2, column=1, columnspan=3, sticky="we", padx=12, pady=6)

        # Variability slider
        ttk.Label(self, text="Size Variability (%)", style='Large.TLabel') \
            .grid(row=3, column=0, sticky="e", padx=12, pady=6)
        ttk.Scale(self, from_=0, to=20, variable=self.variability_percentage_var,
                  orient='horizontal', command=lambda _v: self._rescale()) \
            .grid(row=3, column=1, columnspan=3, sticky="we", padx=12, pady=6)

        # Z-threshold slider
        ttk.Label(self, text="Z Threshold (px)", style='Large.TLabel') \
            .grid(row=4, column=0, sticky="e", padx=12, pady=6)
        self.threshold_scale = ttk.Scale(self, from_=0, to=1, variable=self.z_threshold_var,
                                         orient='horizontal', command=lambda _v: self._rescale())
        self.threshold_scale.grid(row=4, column=1, columnspan=3, sticky="we", padx=12, pady=6)

        # min/max size previews
        ttk.Label(self, text="Min Size Preview", style='Large.TLabel') \
            .grid(row=5, column=0, pady=(10, 0))
        ttk.Label(self, text="Max Size Preview", style='Large.TLabel') \
            .grid(row=5, column=2, pady=(10, 0))

        self.preview_min = tk.Canvas(self, width=self.CANVAS_W // 2, height=self.CANVAS_H // 2,
                                     bg='black', bd=1, relief='sunken')
        self.preview_min.grid(row=6, column=0, columnspan=2, padx=6, pady=6)

        self.preview_max = tk.Canvas(self, width=self.CANVAS_W // 2, height=self.CANVAS_H // 2,
                                     bg='black', bd=1, relief='sunken')
        self.preview_max.grid(row=6, column=2, columnspan=2, padx=6, pady=6)

        # Save button
        ttk.Button(self, text="Save", command=self.save, style='Main.TButton') \
            .grid(row=7, column=0, columnspan=4, pady=(20, 10), padx=12)

    def load(self, info_path):
        self.asset_path = info_path
        if not info_path:
            return

        if not os.path.exists(info_path):
            with open(info_path, 'w') as f:
                json.dump({}, f)

        with open(info_path, 'r') as f:
            data = json.load(f)

        # load size settings
        ss = data.get("size_settings", {})
        self.scale_percentage_var.set(ss.get("scale_percentage", 100))
        self.variability_percentage_var.set(ss.get("variability_percentage", 0))

        # load z_threshold
        zt = data.get('z_threshold')

        # load image
        base_dir = os.path.dirname(info_path)
        img_p = os.path.join(base_dir, "default", "0.png")
        if os.path.isfile(img_p):
            self._orig_img = Image.open(img_p).convert('RGBA')
        else:
            self._orig_img = None

        if self._orig_img:
            bw, bh = self._orig_img.size
            # configure threshold slider max
            self.threshold_scale.configure(to=bh)

            # determine initial threshold value in slider units
            if zt is not None and self.scale_percentage_var.get() != 0:
                self.z_threshold_var.set(zt / (self.scale_percentage_var.get() / 100.0))
            else:
                # default to 80% of height
                self.z_threshold_var.set((bh * 4 / 5) / (self.scale_percentage_var.get() / 100.0))

            # prepare fit image
            fit_scale = min(self.CANVAS_W / bw, self.CANVAS_H / bh)
            disp = (int(bw * fit_scale), int(bh * fit_scale))
            self._fit_img = self._orig_img.resize(disp, Image.LANCZOS)
        else:
            self._fit_img = None

        self._rescale()

    def _rescale(self):
        # main preview
        self.preview.delete("all")
        if self._fit_img:
            base_pct = self.scale_percentage_var.get()
            scale = base_pct / 100.0
            self._draw_to_canvas(self.preview, scale)
            self._draw_threshold(self.preview, scale)

        # min/max previews
        self.preview_min.delete("all")
        self.preview_max.delete("all")
        if self._fit_img:
            base_pct = self.scale_percentage_var.get()
            var_pct = self.variability_percentage_var.get()
            min_scale = max(0, (base_pct - var_pct) / 100.0)
            max_scale = (base_pct + var_pct) / 100.0
            self._draw_to_canvas(self.preview_min, min_scale)
            self._draw_to_canvas(self.preview_max, max_scale)

    def _draw_to_canvas(self, canvas, scale):
        canvas.delete("all")
        w, h = self._fit_img.size
        sw, sh = int(w * scale), int(h * scale)
        if sw <= 0 or sh <= 0:
            return
        scaled = self._fit_img.resize((sw, sh), Image.LANCZOS)
        cw = int(canvas['width'])
        ch = int(canvas['height'])
        x = (cw - sw) // 2
        y = (ch - sh) // 2
        tk_img = ImageTk.PhotoImage(scaled)
        canvas.create_image(x, y, anchor='nw', image=tk_img)
        setattr(self, f'_tk_{id(canvas)}', tk_img)

    def _draw_threshold(self, canvas, scale):
        orig_h = getattr(self._orig_img, 'height', 0)
        if orig_h <= 0:
            return
        cw = int(canvas['width'])
        ch = int(canvas['height'])
        threshold_px = self.z_threshold_var.get()
        displayed_h = self._fit_img.height * scale
        y_offset = (ch - displayed_h) / 2

        # flip: 0 at bottom, max at top
        frac = 1.0 - (threshold_px / orig_h)
        line_y = y_offset + displayed_h * frac

        canvas.create_line(0, line_y, cw, line_y, fill='red', width=2)


    def save(self):
        if not self.asset_path:
            messagebox.showerror("Error", "No asset selected.")
            return

        with open(self.asset_path, 'r') as f:
            data = json.load(f)

        # save z_threshold multiplied by scale percent
        base_pct = self.scale_percentage_var.get()
        z_val = int(self.z_threshold_var.get() * (base_pct / 100.0))
        data['z_threshold'] = z_val

        # save size settings
        data['size_settings'] = {
            'scale_percentage': self.scale_percentage_var.get(),
            'variability_percentage': self.variability_percentage_var.get(),
        }

        with open(self.asset_path, 'w') as f:
            json.dump(data, f, indent=4)

        messagebox.showinfo("Saved", "Size and z-threshold settings saved.")
