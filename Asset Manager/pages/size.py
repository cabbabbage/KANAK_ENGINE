import os, json
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

        self.base_pct_var  = tk.DoubleVar(value=100)   # 50–150%
        self.var_pct_var   = tk.DoubleVar(value=0)     # 0–20%

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

        # Base Size Slider
        ttk.Label(self, text="Base Size (%)", style='Large.TLabel') \
            .grid(row=2, column=0, sticky="e", padx=12, pady=6)
        ttk.Scale(self, from_=10, to=150, variable=self.base_pct_var,
                  orient='horizontal', command=lambda _v: self._rescale()) \
            .grid(row=2, column=1, columnspan=3, sticky="we", padx=12, pady=6)

        # Variability Slider (optional, no preview change)
        ttk.Label(self, text="Size Variability (%)", style='Large.TLabel') \
            .grid(row=3, column=0, sticky="e", padx=12, pady=6)
        ttk.Scale(self, from_=0, to=20, variable=self.var_pct_var,
                  orient='horizontal', command=lambda _v: self._rescale()) \
            .grid(row=3, column=1, columnspan=3, sticky="we", padx=12, pady=6)

        # Save Button
        ttk.Button(self, text="Save", command=self.save, style='Main.TButton') \
            .grid(row=4, column=0, columnspan=4, pady=(20,10), padx=12)

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
        self.base_pct_var.set(ss.get("base_pct", 100))
        self.var_pct_var.set(ss.get("var_pct",    0))

        base_dir = os.path.dirname(info_path)
        img_p = os.path.join(base_dir, "default", "0.png")
        if os.path.isfile(img_p):
            self._orig_img = Image.open(img_p).convert('RGBA')
        else:
            self._orig_img = None

        if self._orig_img:
            # fit once into canvas while preserving aspect ratio
            bw, bh = self._orig_img.size
            fit_scale = min(self.CANVAS_W/bw, self.CANVAS_H/bh)
            disp = (int(bw*fit_scale), int(bh*fit_scale))
            self._fit_img = self._orig_img.resize(disp, Image.LANCZOS)
        else:
            self._fit_img = None

        self._rescale()

    def _rescale(self):
        """Scale _fit_img by base_pct and display relative to canvas size."""
        self.preview.delete("all")
        if not self._fit_img:
            return

        # scale by pct
        pct = self.base_pct_var.get() / 100.0
        w, h = self._fit_img.size
        sw, sh = int(w * pct), int(h * pct)
        scaled = self._fit_img.resize((sw, sh), Image.LANCZOS)

        # center on canvas
        x = (self.CANVAS_W - sw) // 2
        y = (self.CANVAS_H - sh) // 2
        tk_img = ImageTk.PhotoImage(scaled)
        self.preview.create_image(x, y, anchor='nw', image=tk_img)
        self._tk_preview = tk_img  # keep reference

    def save(self):
        """Write size settings back to JSON."""
        if not self.asset_path:
            messagebox.showerror("Error", "No asset selected.")
            return

        out = {"base_pct": self.base_pct_var.get(),
               "var_pct":  self.var_pct_var.get()}
        with open(self.asset_path, 'r') as f:
            data = json.load(f)
        data["size_settings"] = out
        with open(self.asset_path, 'w') as f:
            json.dump(data, f, indent=4)

        messagebox.showinfo("Saved", "Size settings saved.")
