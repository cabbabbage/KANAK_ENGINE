# pages/passa.py

import os
import json
import tkinter as tk
from tkinter import ttk, filedialog, messagebox
from pages.boundary import BoundaryConfigurator
from pages.button import BlueButton
from PIL import Image, ImageTk, ImageDraw

class PassabilityPage(ttk.Frame):
    def __init__(self, parent):
        super().__init__(parent)

        FONT             = ('Segoe UI', 14)
        FONT_BOLD        = ('Segoe UI', 18, 'bold')
        MAIN_COLOR       = "#005f73"
        SECONDARY_COLOR  = "#ee9b00"

        self.asset_path      = None
        self.frames_source   = None
        self.impassable_area  = None
        self.is_passable_var = tk.BooleanVar(value=True)

        for c in range(3):
            self.columnconfigure(c, weight=1)

        ttk.Label(self, text="Passability Settings", style='LargeBold.TLabel')\
            .grid(row=0, column=0, columnspan=3, pady=(10,20), padx=12)

        ttk.Checkbutton(self,
                        text="Is Passable",
                        variable=self.is_passable_var,
                        command=self._on_toggle,
                        style='Large.TCheckbutton')\
            .grid(row=1, column=0, columnspan=2,
                  sticky="w", padx=12, pady=6)

        ttk.Label(self, text="Impassable Area:", style='Large.TLabel')\
            .grid(row=2, column=0, sticky="e", padx=12, pady=6)

        self.btn_configure = BlueButton(self, "Configure Area", command=self._configure_area, x=50, y=10)


        self.area_label = ttk.Label(self, text="(none)", style='Large.TLabel')
        self.area_label.grid(row=2, column=2, sticky="w",
                             padx=12, pady=6)

        self.preview_canvas = tk.Canvas(self,
                                        bg='black',
                                        bd=2,
                                        relief='sunken')
        self.preview_canvas.grid(row=3, column=0,
                                 columnspan=3,
                                 pady=12, padx=12)

        BlueButton(self,
                   "Save",
                   command=self.save,
                   x=0, y=0)

        self._on_toggle()

    def _on_toggle(self):
        if self.is_passable_var.get():
            self.btn_configure.state(['disabled'])
            self.area_label.config(text="(unrestricted)")
            self.preview_canvas.delete("all")
        else:
            self.btn_configure.state(['normal'])
            self.area_label.config(
                text="(configured)" if self.impassable_area else "(none)"
            )
            if self.impassable_area:
                self._draw_preview()

    def _configure_area(self):
        if not self.frames_source:
            messagebox.showerror(
                "Error",
                "Select frames folder in Basic Info first."
            )
            return
        BoundaryConfigurator(
            self,
            base_folder=self.frames_source,
            callback=self._boundary_callback
        )

    def _boundary_callback(self, geo):
        if isinstance(geo, list):
            geo = {'type': 'mask', 'points': geo}
        self.impassable_area = geo
        self.area_label.config(text="(configured)")
        self._draw_preview()

    def _draw_preview(self):
        if not self.asset_path or not self.impassable_area:
            return
        asset_dir = os.path.dirname(self.asset_path)
        img_path = os.path.join(asset_dir, 'default', '0.png')
        if not os.path.isfile(img_path):
            return

        img = Image.open(img_path).convert('RGBA')
        scale = 0.5
        disp = (int(img.width*scale), int(img.height*scale))
        base = img.resize(disp, Image.LANCZOS)

        overlay = Image.new('RGBA', disp, (0,0,0,0))
        draw = ImageDraw.Draw(overlay)
        geo = self.impassable_area

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
        self.tk_preview = ImageTk.PhotoImage(comp)
        self.preview_canvas.config(width=disp[0], height=disp[1])
        self.preview_canvas.delete("all")
        self.preview_canvas.create_image(0, 0, anchor='nw', image=self.tk_preview)

    def load(self, info_path):
        self.asset_path = info_path
        if not info_path:
            return

        asset_dir = os.path.dirname(info_path)
        if not os.path.exists(info_path):
            open(info_path, 'w').close()

        try:
            with open(info_path, 'r') as f:
                data = json.load(f)
        except json.JSONDecodeError:
            messagebox.showerror("Error", f"Malformed JSON: {info_path}")
            return

        changed = False
        for k, dft in (('is_passable', True),
                    ('impassable_area', None)):
            if k not in data:
                data[k] = dft
                changed = True
        if changed:
            with open(info_path, 'w') as f:
                json.dump(data, f, indent=4)

        self.is_passable_var.set(data['is_passable'])

        area_file = data.get('impassable_area')
        if area_file:
            full = os.path.join(asset_dir, area_file)
            if os.path.isfile(full):
                try:
                    with open(full, 'r') as f:
                        self.impassable_area = json.load(f)
                except Exception:
                    self.impassable_area = None
            else:
                self.impassable_area = None
        else:
            self.impassable_area = None

        self.frames_source = os.path.join(asset_dir, "default")
        if not os.path.isdir(self.frames_source):
            print(f"[Warning] Default frame folder missing: {self.frames_source}")
            self.frames_source = None

        print("[DEBUG] frames_source =", self.frames_source)
        self._on_toggle()

    def save(self):
        if not self.asset_path:
            messagebox.showerror("Error", "No asset selected.")
            return
        asset_dir = os.path.dirname(self.asset_path)
        info = json.load(open(self.asset_path,'r'))

        if not self.is_passable_var.get():
            if not self.impassable_area:
                messagebox.showerror(
                    "Error", "Configure an impassable area first."
                )
                return
            area_file = "impassable_area.json"
            with open(os.path.join(asset_dir, area_file), 'w') as f:
                json.dump(self.impassable_area, f, indent=4)
            info['impassable_area'] = area_file
        else:
            old = info.get('impassable_area')
            if old:
                try: os.remove(os.path.join(asset_dir, old))
                except: pass
            info['impassable_area'] = None

        info['is_passable'] = self.is_passable_var.get()

        with open(self.asset_path,'w') as f:
            json.dump(info, f, indent=4)

        messagebox.showinfo("Saved", "Passability settings saved.")
