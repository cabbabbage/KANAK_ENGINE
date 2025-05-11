# pages/passa.py

import os
import json
import tkinter as tk
from tkinter import ttk, filedialog, messagebox
from pages.boundary import BoundaryConfigurator
from PIL import Image, ImageTk, ImageDraw

class PassabilityPage(ttk.Frame):
    def __init__(self, parent):
        super().__init__(parent)

        # — Theme & fonts —
        FONT             = ('Segoe UI', 14)
        FONT_BOLD        = ('Segoe UI', 18, 'bold')
        MAIN_COLOR       = "#005f73"  # teal
        SECONDARY_COLOR  = "#ee9b00"  # amber

        # — State vars —
        self.asset_path      = None
        self.frames_source   = None
        self.collision_area  = None
        self.is_passable_var = tk.BooleanVar(value=True)
        self.z_threshold     = None

        W = 20  # widget width

        # — Styles —
        style = ttk.Style(self)
        # Configure‐area button style (MAIN_COLOR background, white text)
        style.configure('Main.TButton',
                        font=FONT, padding=6,
                        background=MAIN_COLOR,
                        foreground='black')
        style.map('Main.TButton',
                  background=[('active', '!disabled', MAIN_COLOR)])
        # Save button style (SECONDARY_COLOR background, white text)
        style.configure('Secondary.TButton',
                        font=FONT, padding=6,
                        background=SECONDARY_COLOR,
                        foreground='black')
        style.map('Secondary.TButton',
                  background=[('active', '!disabled', SECONDARY_COLOR)])
        # Labels
        style.configure('Large.TLabel',     font=FONT)
        style.configure('LargeBold.TLabel',
                        font=FONT_BOLD,
                        foreground=SECONDARY_COLOR)
        style.configure('Large.TCheckbutton', font=FONT)

        # — Layout columns —
        for c in range(3):
            self.columnconfigure(c, weight=1)

        # — Title —
        ttk.Label(self, text="Passability Settings", style='LargeBold.TLabel')\
            .grid(row=0, column=0, columnspan=3, pady=(10,20), padx=12)

        # — Is Passable? —
        ttk.Checkbutton(self,
                        text="Is Passable",
                        variable=self.is_passable_var,
                        command=self._on_toggle,
                        style='Large.TCheckbutton')\
            .grid(row=1, column=0, columnspan=2,
                  sticky="w", padx=12, pady=6)

        # — Configure Impassable Boundary —
        ttk.Label(self, text="Impassable Area:", style='Large.TLabel')\
            .grid(row=2, column=0, sticky="e", padx=12, pady=6)
        self.btn_configure = ttk.Button(self,
                                        text="Configure Area",
                                        command=self._configure_area,
                                        style='Main.TButton',
                                        width=W)
        self.btn_configure.grid(row=2, column=1, sticky="w",
                                padx=12, pady=6)

        self.area_label = ttk.Label(self, text="(none)", style='Large.TLabel')
        self.area_label.grid(row=2, column=2, sticky="w",
                             padx=12, pady=6)

        # — Preview Canvas —
        self.preview_canvas = tk.Canvas(self,
                                        bg='black',
                                        bd=2,
                                        relief='sunken')
        self.preview_canvas.grid(row=3, column=0,
                                 columnspan=3,
                                 pady=12, padx=12)

        # — Save Button —
        ttk.Button(self,
                   text="Save",
                   command=self.save,
                   style='Secondary.TButton',
                   width=W)\
            .grid(row=4, column=0, columnspan=3,
                  pady=(20,10), padx=12)

        # initially toggle controls
        self._on_toggle()


    def _on_toggle(self):
        """Enable or disable the Configure button & preview."""
        if self.is_passable_var.get():
            self.btn_configure.state(('disabled',))
            self.area_label.config(text="(unrestricted)")
            self.preview_canvas.delete("all")
        else:
            self.btn_configure.state(('!disabled',))
            self.area_label.config(
                text="(configured)" if self.collision_area else "(none)"
            )
            if self.collision_area:
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
        """Receive geometry (list or dict), normalize, compute z-threshold."""
        if isinstance(geo, list):
            geo = {'type': 'mask', 'points': geo}
        self.collision_area = geo

        if geo['type'] == 'circle':
            self.z_threshold = geo['y']
        else:
            ys = [p[1] for p in geo.get('points', [])]
            self.z_threshold = sum(ys)//len(ys) if ys else None

        self.area_label.config(text="(configured)")
        self._draw_preview()


    def _draw_preview(self):
        """Overlay the impassable shape on default/0.png at 50% scale."""
        if not self.asset_path or not self.collision_area:
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
        geo = self.collision_area

        if geo['type'] == 'circle':
            cx = int(geo['x']*scale)
            cy = int(geo['y']*scale)
            rw = int(geo['w']*scale)//2
            rh = int(geo['h']*scale)//2
            draw.ellipse((cx-rw, cy-rh, cx+rw, cy+rh),
                         fill=(255,0,0,128))

        else:  # mask
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
        """Load JSON and repopulate UI."""
        self.asset_path = info_path
        if not info_path:
            return
        asset_dir = os.path.dirname(info_path)
        if not os.path.exists(info_path):
            open(info_path,'w').close()
        data = json.load(open(info_path,'r'))

        # ensure keys
        changed = False
        for k,dft in (('is_passable', True),
                      ('collision_area', None),
                      ('z_threshold', None)):
            if k not in data:
                data[k] = dft
                changed = True
        if changed:
            with open(info_path,'w') as f:
                json.dump(data, f, indent=4)

        # populate
        self.is_passable_var.set(data['is_passable'])
        area_file = data.get('collision_area')
        if area_file:
            full = os.path.join(asset_dir, area_file)
            if os.path.isfile(full):
                self.collision_area = json.load(open(full,'r'))
            else:
                self.collision_area = None
        else:
            self.collision_area = None

        # get frames folder from Basic Info
        default_anim = data.get('default_animation', {}).get('on_start','')
        folder = os.path.join(asset_dir, default_anim)
        if os.path.isdir(folder):
            self.frames_source = folder

        self._on_toggle()


    def save(self):
        """Write is_passable, impassable_area.json, and z_threshold."""
        if not self.asset_path:
            messagebox.showerror("Error", "No asset selected.")
            return
        asset_dir = os.path.dirname(self.asset_path)
        info = json.load(open(self.asset_path,'r'))

        # impassable area file
        if not self.is_passable_var.get():
            if not self.collision_area:
                messagebox.showerror(
                    "Error", "Configure an impassable area first."
                )
                return
            area_file = "impassable_area.json"
            with open(os.path.join(asset_dir, area_file), 'w') as f:
                json.dump(self.collision_area, f, indent=4)
            info['collision_area'] = area_file
        else:
            # remove old file
            old = info.get('collision_area')
            if old:
                try: os.remove(os.path.join(asset_dir, old))
                except: pass
            info['collision_area'] = None

        # always store passable + z_threshold
        info['is_passable'] = self.is_passable_var.get()
        info['z_threshold']  = self.z_threshold

        with open(self.asset_path,'w') as f:
            json.dump(info, f, indent=4)

        messagebox.showinfo("Saved", "Passability settings saved.")
