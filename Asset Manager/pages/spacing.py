# pages/spacing.py

import os
import json
import tkinter as tk
from tkinter import ttk, messagebox
from pages.boundary import BoundaryConfigurator
from PIL import Image, ImageTk, ImageDraw

class SpacingThresholdPage(ttk.Frame):
    """
    Define an area within which child assets may NOT spawn.
    Draw the region, preview it over the asset's default/0.png,
    save the geometry to spacing_area.json and record just that path.
    """

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

        # layout
        for c in range(3):
            self.columnconfigure(c, weight=1)

        ttk.Label(self, text="Spacing Threshold", style='LargeBold.TLabel')\
            .grid(row=0, column=0, columnspan=3, pady=(10,20))

        ttk.Label(self, text="Forbidden Spawn Region:", style='Large.TLabel')\
            .grid(row=1, column=0, sticky='e', padx=12, pady=6)
        self.btn_draw = ttk.Button(self, text="Draw…",
                                   command=self._configure_area,
                                   style='Main.TButton')
        self.btn_draw.grid(row=1, column=1, sticky='w', padx=12, pady=6)
        self.lbl_status = ttk.Label(self, text="(none)", style='Large.TLabel')
        self.lbl_status.grid(row=1, column=2, sticky='w', padx=12, pady=6)

        # preview canvas
        self.preview = tk.Canvas(self, bg='black', bd=2, relief='sunken')
        self.preview.grid(row=2, column=0, columnspan=3,
                          padx=12, pady=12)

        ttk.Button(self, text="Save",
                   command=self.save,
                   style='Main.TButton')\
            .grid(row=3, column=0, columnspan=3,
                  pady=(0,10))

    def _configure_area(self):
        """Launch BoundaryConfigurator over the asset's default frames."""
        if not self.frames_source:
            messagebox.showerror("Error",
                "Select frames folder in Basic Info first.")
            return
        BoundaryConfigurator(self,
                             base_folder=self.frames_source,
                             callback=self._on_boundary)

    def _on_boundary(self, geo):
        """Normalize and store the boundary, then preview it."""
        if isinstance(geo, list):
            geo = {'type': 'mask', 'points': geo}
        self.area_geo = geo
        self.lbl_status.config(text="(configured)")
        self._draw_preview()

    def _draw_preview(self):
        """Draw the forbidden region over default/0.png at 50% scale."""
        if not self.asset_path or not self.area_geo:
            return

        asset_dir = os.path.dirname(self.asset_path)
        p = os.path.join(asset_dir, 'default', '0.png')
        if not os.path.isfile(p):
            return

        img = Image.open(p).convert('RGBA')
        scale = 0.5
        disp = (int(img.width*scale), int(img.height*scale))
        base = img.resize(disp, Image.LANCZOS)

        overlay = Image.new('RGBA', disp, (0,0,0,0))
        draw = ImageDraw.Draw(overlay)
        geo = self.area_geo

        if geo['type'] == 'circle':
            cx = int(geo['x']*scale)
            cy = int(geo['y']*scale)
            rw = int(geo['w']*scale)//2
            rh = int(geo['h']*scale)//2
            draw.ellipse((cx-rw, cy-rh, cx+rw, cy+rh),
                         fill=(255,0,0,128))
        else:
            for x,y in geo.get('points', []):
                dx, dy = int(x*scale), int(y*scale)
                if 0<=dx<disp[0] and 0<=dy<disp[1]:
                    overlay.putpixel((dx,dy), (255,0,0,128))

        comp = Image.alpha_composite(base, overlay)
        self._tk = ImageTk.PhotoImage(comp)
        self.preview.config(width=disp[0], height=disp[1])
        self.preview.delete("all")
        self.preview.create_image(0,0,anchor='nw',image=self._tk)

    def load(self, info_path):
        """Load info.json, set up frames_source & existing spacing_area."""
        self.asset_path = info_path
        if not info_path:
            return

        with open(info_path, 'r') as f:
            data = json.load(f)

        # pull default frames folder from BasicInfoPage
        default_anim = data.get('default_animation', {}).get('frames_path','')
        asset_dir = os.path.dirname(info_path)
        folder = os.path.join(asset_dir, default_anim)
        if os.path.isdir(folder):
            self.frames_source = folder

        # load existing spacing_area.json, if any
        fn = data.get('spacing_area')
        if fn:
            full = os.path.join(asset_dir, fn)
            if os.path.isfile(full):
                self.area_geo = json.load(open(full,'r'))
                self.lbl_status.config(text="(configured)")
            else:
                self.area_geo = None
                self.lbl_status.config(text="(none)")
        else:
            self.area_geo = None
            self.lbl_status.config(text="(none)")

        self._draw_preview()

    def save(self):
        """Write out spacing_area.json and path into info.json."""
        if not self.asset_path:
            messagebox.showerror("Error","No asset selected.")
            return

        if not self.area_geo:
            messagebox.showerror("Error",
                "Draw a forbidden region before saving.")
            return

        asset_dir = os.path.dirname(self.asset_path)
        fn = "spacing_area.json"
        with open(os.path.join(asset_dir, fn), 'w') as f:
            json.dump(self.area_geo, f, indent=2)

        # update info.json
        with open(self.asset_path, 'r') as f:
            info = json.load(f)
        info['spacing_area'] = fn
        with open(self.asset_path, 'w') as f:
            json.dump(info, f, indent=2)

        messagebox.showinfo("Saved", "Spacing threshold saved.")
