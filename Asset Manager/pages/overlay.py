# pages/image_overlay.py

import os
import json
import shutil
import tkinter as tk
from tkinter import ttk, filedialog, messagebox
from PIL import Image, ImageTk, ImageChops

class ImageOverlayPage(ttk.Frame):
    def __init__(self, parent):
        super().__init__(parent)

        # — Theme & fonts —
        self.FONT            = ('Segoe UI', 14)
        self.FONT_BOLD       = ('Segoe UI', 18, 'bold')
        self.MAIN_COLOR      = "#005f73"   # teal
        self.SECONDARY_COLOR = "#ee9b00"   # amber

        # — State —
        self.asset_path       = None
        self.overlay_path     = None
        self.overlay_img      = None
        self.overlay_orig_w   = 0
        self.overlay_orig_h   = 0
        self.center           = (0, 0)
        self.offset_x_var     = tk.IntVar(value=0)
        self.offset_y_var     = tk.IntVar(value=0)
        self.scale_var        = tk.DoubleVar(value=100)
        self.rotation_var     = tk.IntVar(value=0)
        self.mode_var         = tk.StringVar(value="normal")
        self.alpha_var        = tk.IntVar(value=100)

        W = 20  # uniform widget width

        # — Styles —
        style = ttk.Style(self)
        style.configure('Main.TButton',
                        font=self.FONT, padding=6,
                        background=self.MAIN_COLOR,
                        foreground='black')
        style.map('Main.TButton',
                  background=[('active', self.SECONDARY_COLOR)])
        style.configure('Secondary.TButton',
                        font=self.FONT, padding=6,
                        background=self.SECONDARY_COLOR,
                        foreground='black')
        style.map('Secondary.TButton',
                  background=[('active', self.MAIN_COLOR)])
        style.configure('Main.TMenubutton',
                        font=self.FONT, padding=6,
                        background='white',
                        foreground=self.SECONDARY_COLOR,
                        borderwidth=2, relief='solid')
        style.map('Main.TMenubutton',
                  background=[('active', 'white')])
        style.configure('Large.TLabel',     font=self.FONT)
        style.configure('LargeBold.TLabel',
                        font=self.FONT_BOLD,
                        foreground=self.SECONDARY_COLOR)

        # — Layout columns evenly —
        for c in range(4):
            self.columnconfigure(c, weight=1)

        # — Title —
        ttk.Label(self, text="Image Overlay", style='LargeBold.TLabel')\
            .grid(row=0, column=0, columnspan=4, pady=(10,20), padx=12)

        # — Select Overlay Image —
        ttk.Button(self, text="Select Overlay Image",
                   command=self._on_select_overlay,
                   style='Main.TButton', width=W)\
            .grid(row=1, column=0, columnspan=2, sticky="w",
                  padx=12, pady=6)
        self.overlay_label = ttk.Label(self, text="(none)", style='Large.TLabel')
        self.overlay_label.grid(row=1, column=2, columnspan=2,
                                sticky="w", padx=12, pady=6)

        # — Position sliders —
        ttk.Label(self, text="Offset X:", style='Large.TLabel')\
            .grid(row=2, column=0, sticky="e", padx=12, pady=6)
        ttk.Scale(self, from_=-200, to=200,
                  variable=self.offset_x_var,
                  orient='horizontal',
                  command=lambda _e: self._update_preview())\
            .grid(row=2, column=1, sticky="we", padx=12, pady=6)
        ttk.Label(self, text="Offset Y:", style='Large.TLabel')\
            .grid(row=2, column=2, sticky="e", padx=12, pady=6)
        ttk.Scale(self, from_=-200, to=200,
                  variable=self.offset_y_var,
                  orient='horizontal',
                  command=lambda _e: self._update_preview())\
            .grid(row=2, column=3, sticky="we", padx=12, pady=6)

        # — Scale & Rotation —
        ttk.Label(self, text="Scale (%):", style='Large.TLabel')\
            .grid(row=3, column=0, sticky="e", padx=12, pady=6)
        ttk.Scale(self, from_=10, to=300,
                  variable=self.scale_var,
                  orient='horizontal',
                  command=lambda _e: self._update_preview())\
            .grid(row=3, column=1, sticky="we", padx=12, pady=6)
        ttk.Label(self, text="Rotation (°):", style='Large.TLabel')\
            .grid(row=3, column=2, sticky="e", padx=12, pady=6)
        ttk.Scale(self, from_=0, to=360,
                  variable=self.rotation_var,
                  orient='horizontal',
                  command=lambda _e: self._update_preview())\
            .grid(row=3, column=3, sticky="we", padx=12, pady=6)

        # — Composite Mode & Transparency —
        ttk.Label(self, text="Mode:", style='Large.TLabel')\
            .grid(row=4, column=0, sticky="e", padx=12, pady=6)
        modes = ['normal','multiply','add']
        self.mode_menu = ttk.OptionMenu(self, self.mode_var, modes[0], *modes,
                                        command=lambda _v: self._update_preview())
        self.mode_menu.config(style='Main.TMenubutton', width=W)
        self.mode_menu.grid(row=4, column=1, sticky="w", padx=12, pady=6)

        ttk.Label(self, text="Transparency (%):", style='Large.TLabel')\
            .grid(row=4, column=2, sticky="e", padx=12, pady=6)
        ttk.Scale(self, from_=0, to=100,
                  variable=self.alpha_var,
                  orient='horizontal',
                  command=lambda _e: self._update_preview())\
            .grid(row=4, column=3, sticky="we", padx=12, pady=6)

        # — Save Button —
        ttk.Button(self, text="Save Overlay",
                   command=self.save,
                   style='Secondary.TButton', width=W+5)\
            .grid(row=5, column=0, columnspan=4,
                  pady=(20,10), padx=12)

        # — Preview Canvas —
        self.preview_canvas = tk.Canvas(self, bg='black', bd=2, relief='sunken')
        self.preview_canvas.grid(row=6, column=0, columnspan=4,
                                 pady=10, padx=12)


    def _on_select_overlay(self):
        """User picks a new overlay file."""
        file = filedialog.askopenfilename(
            filetypes=[("Image","*.png;*.jpg;*.jpeg;*.gif")]
        )
        if not file:
            return
        self._load_overlay(file)


    def _load_overlay(self, file):
        """Load overlay *and* remember path."""
        self.overlay_path = file
        self.overlay_label.config(text=os.path.basename(file))
        img = Image.open(file).convert('RGBA')
        self.overlay_img = img
        self.overlay_orig_w, self.overlay_orig_h = img.size
        self._update_preview()


    def _load_base(self):
        """Load default/0.png at 1/3 size and set asset center."""
        asset_dir = os.path.dirname(self.asset_path)
        p = os.path.join(asset_dir, 'default', '0.png')
        if not os.path.isfile(p):
            return None
        img = Image.open(p).convert('RGBA')
        w,h = img.size
        self.center = (w//2, h//2)
        return img.resize((w//3, h//3), Image.LANCZOS)


    def _update_preview(self):
        base = self._load_base()
        if base is None:
            return
        comp = base.copy()

        if self.overlay_img:
            # transform overlay
            scale = self.scale_var.get()/100.0
            ov = self.overlay_img.resize(
                (int(self.overlay_orig_w*scale),
                 int(self.overlay_orig_h*scale)),
                Image.LANCZOS
            )
            ov = ov.rotate(-self.rotation_var.get(),
                           expand=True, resample=Image.BICUBIC)

            # apply alpha
            alpha = self.alpha_var.get()/100.0
            mask = ov.split()[3].point(lambda v: int(v*alpha))
            ov.putalpha(mask)

            # position
            cx, cy = self.center
            dx = int(cx//3 - ov.width//2 + self.offset_x_var.get())
            dy = int(cy//3 - ov.height//2 + self.offset_y_var.get())

            # composite
            layer = Image.new('RGBA', comp.size)
            layer.paste(ov, (dx, dy), ov)
            mode = self.mode_var.get()
            if mode == 'normal':
                comp = Image.alpha_composite(comp, layer)
            elif mode == 'multiply':
                comp = ImageChops.multiply(comp, layer)
            elif mode == 'add':
                comp = ImageChops.add(comp, layer)

        # draw
        self.tk_preview = ImageTk.PhotoImage(comp)
        w, h = comp.size
        self.preview_canvas.config(width=w, height=h)
        self.preview_canvas.delete("all")
        self.preview_canvas.create_image(0, 0, anchor='nw', image=self.tk_preview)


    def load(self, info_path):
        """Rehydrate from JSON (no more __wrapped__ hacks)."""
        self.asset_path = info_path
        if not info_path:
            return
        if not os.path.exists(info_path):
            with open(info_path, 'w'): pass

        data = json.load(open(info_path, 'r'))
        ov = data.get("overlay", {})

        # if there's a path, load it
        path = ov.get("path")
        if path:
            full = os.path.join(os.path.dirname(info_path), path)
            if os.path.isfile(full):
                self._load_overlay(full)

        # restore controls
        self.offset_x_var.set(ov.get("offset_x", 0))
        self.offset_y_var.set(ov.get("offset_y", 0))
        self.scale_var.set(ov.get("scale_pct", 100))
        self.rotation_var.set(ov.get("rotation", 0))
        self.mode_var.set(ov.get("mode", "normal"))
        self.alpha_var.set(ov.get("alpha_pct", 100))

        self._update_preview()


    def save(self):
        if not self.asset_path:
            messagebox.showerror("Error","No asset selected.")
            return
        asset_dir = os.path.dirname(self.asset_path)

        # copy overlay file if chosen
        overlay_file = None
        if self.overlay_path:
            overlay_file = "overlay.png"
            shutil.copyfile(self.overlay_path,
                            os.path.join(asset_dir, overlay_file))

        # store all settings
        cfg = {
            "path":       overlay_file,
            "offset_x":   self.offset_x_var.get(),
            "offset_y":   self.offset_y_var.get(),
            "scale_pct":  self.scale_var.get(),
            "rotation":   self.rotation_var.get(),
            "mode":       self.mode_var.get(),
            "alpha_pct":  self.alpha_var.get()
        }

        data = json.load(open(self.asset_path,'r'))
        data["overlay"] = cfg
        with open(self.asset_path,'w') as f:
            json.dump(data, f, indent=4)

        messagebox.showinfo("Saved","Overlay settings saved.")
