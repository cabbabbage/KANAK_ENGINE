# === File: pages/child_assets.py ===
import os
import json
import tkinter as tk
from tkinter import ttk, messagebox
from pages.point_config import PointConfigurator
from pages.range import Range
from pages.search import AssetSearchWindow
from PIL import Image, ImageTk

ASSET_DIR = "SRC"

class ScrollableFrame(ttk.Frame):
    def __init__(self, container, *args, **kwargs):
        super().__init__(container, *args, **kwargs)
        canvas = tk.Canvas(self)
        vsb = ttk.Scrollbar(self, orient="vertical", command=canvas.yview)
        self.scrollable_frame = ttk.Frame(canvas)
        self.scrollable_frame.bind(
            "<Configure>",
            lambda e: canvas.configure(scrollregion=canvas.bbox("all"))
        )
        canvas.create_window((0, 0), window=self.scrollable_frame, anchor='nw')
        canvas.configure(yscrollcommand=vsb.set)
        canvas.pack(side='left', fill='both', expand=True)
        vsb.pack(side='right', fill='y')


class ChildAssetsPage(ttk.Frame):
    def __init__(self, parent):
        super().__init__(parent)
        self.FONT = ('Segoe UI', 14)
        self.FONT_BOLD = ('Segoe UI', 18, 'bold')
        self.MAIN_COLOR = "#005f73"
        self.SECONDARY_COLOR = "#ee9b00"

        self.asset_path = None
        self.child_frames = []
        self.asset_names = []

        style = ttk.Style(self)
        style.configure('LargeBold.TLabel', font=self.FONT_BOLD, foreground=self.SECONDARY_COLOR)
        style.configure('Large.TLabel', font=self.FONT)
        style.configure('Large.TCheckbutton', font=self.FONT)

        ttk.Label(self, text="Region-Spawned Children", style='LargeBold.TLabel')\
            .grid(row=0, column=0, columnspan=5, pady=(10, 20), padx=12)

        self.rowconfigure(1, weight=1)
        self.columnconfigure(0, weight=1)

        self.container = ScrollableFrame(self)
        self.container.grid(row=1, column=0, columnspan=5, sticky="nsew", padx=12, pady=(0, 10))

        btn_frame = ttk.Frame(self)
        btn_frame.grid(row=2, column=0, columnspan=5, sticky="ew", pady=(0, 16))
        btn_frame.columnconfigure(0, weight=1)
        btn_frame.columnconfigure(1, weight=1)

        tk.Button(btn_frame, text="Add Child Region", command=self._add_child_row, font=self.FONT, bg="#007BFF", fg="white").grid(row=0, column=0, padx=8, sticky="e")
        tk.Button(btn_frame, text="Save", command=self.save, font=self.FONT, bg="#007BFF", fg="white").grid(row=0, column=1, padx=8, sticky="w")

    def _add_child_row(self, data=None):
        idx = len(self.child_frames)
        frm = ttk.Frame(self.container.scrollable_frame, relief='ridge', padding=12)
        frm.grid(row=idx, column=0, sticky="nsew", pady=8)
        frm.columnconfigure(1, weight=1)
        frm.columnconfigure(4, weight=1)

        ttk.Label(frm, text="Asset:", style='Large.TLabel').grid(row=0, column=0, sticky="w")
        asset_var = tk.StringVar(value=(data or {}).get('asset', ''))
        asset_display = ttk.Label(frm, text=asset_var.get(), style='Large.TLabel')
        asset_display.grid(row=0, column=1, sticky="w", padx=6)
        tk.Button(frm, text="Pick…", command=lambda i=idx: self._pick_asset(i), font=self.FONT, bg="#007BFF", fg="white").grid(row=0, column=2, sticky="w")

        ttk.Label(frm, text="Spawn Point:", style='Large.TLabel').grid(row=1, column=0, sticky="w", pady=4)
        tk.Button(frm, text="Pick Point…", command=lambda i=idx: self._pick_point(i), font=self.FONT, bg="#007BFF", fg="white").grid(row=1, column=1, sticky="w", padx=6)
        area_label = ttk.Label(frm, text="(none)", style='Large.TLabel')
        area_label.grid(row=1, column=2, columnspan=2, sticky="w")

        ttk.Label(frm, text="Z Offset:", style='Large.TLabel').grid(row=2, column=0, sticky="w", pady=4)
        z_var = tk.IntVar(value=(data or {}).get('z_offset', 0))
        ttk.Spinbox(frm, from_=-9999, to=9999, textvariable=z_var, width=8)\
            .grid(row=2, column=1, sticky="w", padx=6)

        count_range = Range(frm, set_min=(data or {}).get('min', 1), set_max=(data or {}).get('max', 1),
                            min_bound=0, max_bound=100, label="Number of Children")
        count_range.grid(row=3, column=0, columnspan=4, sticky="ew", padx=6, pady=4)

        term_var = tk.BooleanVar(value=(data or {}).get('terminate_with_parent', False))
        ttk.Checkbutton(frm, text="Treat as top-level asset", variable=term_var, style='Large.TCheckbutton')\
            .grid(row=4, column=0, columnspan=4, sticky="w", pady=(8, 4))

        cv = tk.Canvas(frm, width=400, height=400, bg='black')
        cv.grid(row=0, column=4, rowspan=5, padx=10, pady=4)

        tk.Button(frm, text="Remove", command=lambda i=idx: self._remove_child_row(i), font=self.FONT, bg="#007BFF", fg="white").grid(row=0, column=3, sticky="e")

        entry = {
            'frame': frm,
            'vars': {
                'asset': asset_var,
                'point_data': data.get('point') if data else None,
                'z_offset': z_var,
                'range': count_range,
                'terminate_with_parent': term_var
            },
            'widgets': {
                'area_label': area_label,
                'asset_display': asset_display
            },
            'canvas': cv,
            'tkimg': None
        }
        self.child_frames.append(entry)

        if data and data.get('point'):
            area_label.config(text=f"({data['point']['x']}, {data['point']['y']}) r={data['point']['radius']}")

        self._update_preview(idx)

    def _pick_asset(self, idx):
        win = AssetSearchWindow(self)
        win.wait_window()
        selected = win.selected_asset
        if selected:
            entry = self.child_frames[idx]
            entry['vars']['asset'].set(selected)
            entry['widgets']['asset_display'].config(text=selected)
            self._update_preview(idx)

    def _pick_point(self, idx):
        entry = self.child_frames[idx]
        default_folder = os.path.join(os.path.dirname(self.asset_path), 'default')
        if not os.path.isdir(default_folder):
            messagebox.showerror("Error", f"No default in {default_folder}")
            return
        win = PointConfigurator(self, base_folder=default_folder)
        win.wait_window()
        result = win.get_selected_point()
        if result:
            entry['vars']['point_data'] = result
            entry['widgets']['area_label'].config(text=f"({result['x']}, {result['y']}) r={result['radius']}")

    def _remove_child_row(self, idx):
        entry = self.child_frames.pop(idx)
        entry['frame'].destroy()
        for i, e in enumerate(self.child_frames):
            e['frame'].grid_configure(row=i)

    def _update_preview(self, idx):
        entry = self.child_frames[idx]
        asset = entry['vars']['asset'].get()
        cv = entry['canvas']
        infof = os.path.join(ASSET_DIR, asset, "info.json")
        imgf = os.path.join(ASSET_DIR, asset, "default", "0.png")
        if not (os.path.isfile(infof) and os.path.isfile(imgf)):
            return
        img = Image.open(imgf).convert('RGBA')
        w, h = img.size
        scale = min(400 / w, 400 / h)
        disp = img.resize((int(w * scale), int(h * scale)), Image.LANCZOS)
        tkimg = ImageTk.PhotoImage(disp)
        cv.delete("all")
        cv.config(width=disp.width, height=disp.height)
        cv.create_image(0, 0, anchor='nw', image=tkimg)
        entry['tkimg'] = tkimg

    def load(self, info_path):
        self.asset_path = info_path
        if not info_path:
            return
        current = os.path.basename(os.path.dirname(info_path))
        self.asset_names = [
            n for n in sorted(os.listdir(ASSET_DIR))
            if n != current and os.path.isdir(os.path.join(ASSET_DIR, n)) and os.path.exists(os.path.join(ASSET_DIR, n, "info.json"))
        ]
        for e in self.child_frames:
            e['frame'].destroy()
        self.child_frames.clear()
        data = json.load(open(info_path, 'r'))
        for cfg in data.get('child_assets', []):
            self._add_child_row(cfg)

    def save(self):
        if not self.asset_path:
            messagebox.showerror("Error", "No asset selected.")
            return

        try:
            prior_data = json.load(open(self.asset_path))
            prior_assets = prior_data.get('child_assets', [])
        except:
            prior_assets = []

        out = []

        for i, entry in enumerate(self.child_frames):
            v = entry['vars']
            point = v.get('point_data')
            min_val, max_val = v['range'].get()
            out.append({
                'asset': v['asset'].get(),
                'point': point,
                'z_offset': v['z_offset'].get(),
                'min': min_val,
                'max': max_val,
                'terminate_with_parent': v['terminate_with_parent'].get()
            })

        data = json.load(open(self.asset_path, 'r'))
        data['child_assets'] = out
        with open(self.asset_path, 'w') as f:
            json.dump(data, f, indent=2)

        messagebox.showinfo("Saved", "Child-assets saved.")
