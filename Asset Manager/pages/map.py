import os
import json
import tkinter as tk
from tkinter import ttk
from PIL import Image, ImageTk

SRC_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "SRC"))
MAP_JSON_PATH = os.path.join(SRC_DIR, "Map.json")

class WorldSettingsPage(ttk.Frame):
    def __init__(self, parent):
        super().__init__(parent)
        self.asset_controls = {}
        self.map_width_var = tk.IntVar()
        self.map_height_var = tk.IntVar()
        self._load_json()
        self._sync_assets()
        self._build_ui()

    def _load_json(self):
        with open(MAP_JSON_PATH, "r") as f:
            self.map_data = json.load(f)
        self.asset_lookup = {}
        for a in self.map_data.get("assets", []):
            if "is_active" not in a:
                a["is_active"] = a.get("max_number", 0) > 0
            self.asset_lookup[a["name"]] = a
        if "perimeter" in self.map_data and len(self.map_data["perimeter"]) >= 3:
            p1, p3 = self.map_data["perimeter"][0], self.map_data["perimeter"][2]
            self.map_width_var.set(p3[0] - p1[0])
            self.map_height_var.set(p3[1] - p1[1])

    def _sync_assets(self):
        for asset_name in sorted(os.listdir(SRC_DIR)):
            asset_path = os.path.join(SRC_DIR, asset_name)
            info_path = os.path.join(asset_path, "info.json")
            if not os.path.isdir(asset_path) or not os.path.isfile(info_path):
                continue
            try:
                with open(info_path) as f:
                    json.load(f)
            except Exception:
                continue
            if asset_name not in self.asset_lookup:
                self.asset_lookup[asset_name] = {
                    "name": asset_name,
                    "min_number": 0,
                    "max_number": 0,
                    "is_active": True
                }
                self.map_data["assets"].append(self.asset_lookup[asset_name])

    def _build_ui(self):
        for widget in self.winfo_children():
            widget.destroy()

        font_large = ("Segoe UI", 12)

        controls_frame = ttk.Frame(self)
        controls_frame.pack(pady=10, fill="x")
        ttk.Label(controls_frame, text="Map Width:").pack(side="left", padx=5)
        ttk.Entry(controls_frame, textvariable=self.map_width_var, width=6).pack(side="left")
        ttk.Label(controls_frame, text="Map Height:").pack(side="left", padx=5)
        ttk.Entry(controls_frame, textvariable=self.map_height_var, width=6).pack(side="left")
        ttk.Button(controls_frame, text="Resize Map", command=self._resize_map).pack(side="left", padx=10)
        ttk.Button(controls_frame, text="Deactivate All", command=self._deactivate_all).pack(side="right", padx=10)

        canvas = tk.Canvas(self)
        scrollbar = ttk.Scrollbar(self, orient="vertical", command=canvas.yview)
        canvas.configure(yscrollcommand=scrollbar.set)
        canvas.pack(side="left", fill="both", expand=True)
        scrollbar.pack(side="right", fill="y")

        canvas.bind_all("<MouseWheel>", lambda e: canvas.yview_scroll(int(-1*(e.delta/120)), "units"))

        self.scroll_frame = ttk.Frame(canvas)
        canvas.create_window((0, 0), window=self.scroll_frame, anchor="nw")
        self.scroll_frame.bind("<Configure>", lambda e: canvas.configure(scrollregion=canvas.bbox("all")))

        self.asset_controls.clear()

        for asset_name in sorted(self.asset_lookup.keys(), key=lambda k: self.asset_lookup[k].get("max_number", 0), reverse=True):
            asset = self.asset_lookup[asset_name]
            asset.setdefault("is_active", asset.get("max_number", 0) > 0)

            asset_frame = ttk.LabelFrame(self.scroll_frame, text=asset_name)
            asset_frame.pack(fill="x", pady=6, padx=12, ipady=4)

            top_row = ttk.Frame(asset_frame)
            top_row.pack(fill="x", padx=4, pady=2)

            img_path = os.path.join(SRC_DIR, asset_name, "default", "0.png")
            try:
                img = Image.open(img_path).convert("RGBA")
                img.thumbnail((40, 40), Image.LANCZOS)
                photo = ImageTk.PhotoImage(img)
                lbl_img = tk.Label(top_row, image=photo)
                lbl_img.image = photo
                lbl_img.pack(side="left", padx=4)
            except:
                tk.Label(top_row, text="🞬", width=4).pack(side="left")

            is_active = tk.BooleanVar(value=asset.get("is_active", True))
            ttk.Checkbutton(top_row, text="Active", variable=is_active, command=self._save_json).pack(side="right", padx=10)

            min_val = tk.IntVar(value=asset.get("min_number", 0))
            max_val = tk.IntVar(value=asset.get("max_number", 0))

            min_label = ttk.Label(asset_frame, text=f"Min: {min_val.get()}")
            max_label = ttk.Label(asset_frame, text=f"Max: {max_val.get()}")
            min_label.pack(anchor="w", padx=12)
            max_label.pack(anchor="w", padx=12)

            slider_frame = ttk.Frame(asset_frame)
            slider_frame.pack(fill="x", padx=12)

            last_min = [min_val.get()]
            last_max = [max_val.get()]

            def make_slider_update(minv, maxv, min_label, max_label, is_min):
                def _update(val):
                    v = int(val)
                    if is_min:
                        if v > maxv.get():
                            min_slider.set(last_min[0])
                            return
                        last_min[0] = v
                        minv.set(v)
                        min_label.config(text=f"Min: {v}")
                    else:
                        if v < minv.get():
                            max_slider.set(last_max[0])
                            return
                        last_max[0] = v
                        maxv.set(v)
                        max_label.config(text=f"Max: {v}")
                return _update

            def on_release():
                if min_val.get() <= max_val.get():
                    self._save_json()

            min_slider = tk.Scale(slider_frame, from_=0, to=1000, orient="horizontal", showvalue=False,
                                  length=1400, command=make_slider_update(min_val, max_val, min_label, max_label, True))
            max_slider = tk.Scale(slider_frame, from_=0, to=1000, orient="horizontal", showvalue=False,
                                  length=1400, command=make_slider_update(min_val, max_val, min_label, max_label, False))

            min_slider.set(min_val.get())
            max_slider.set(max_val.get())

            min_slider.pack(fill="x", pady=2)
            max_slider.pack(fill="x", pady=2)

            min_slider.bind("<ButtonRelease-1>", lambda e: on_release())
            max_slider.bind("<ButtonRelease-1>", lambda e: on_release())

            self.asset_controls[asset_name] = (min_val, max_val, is_active, min_label, max_label)

    def _resize_map(self):
        w = self.map_width_var.get()
        h = self.map_height_var.get()
        if w <= 0 or h <= 0:
            tk.messagebox.showerror("Invalid Size", "Map size must be positive.")
            return
        self.map_data["perimeter"] = [[0, 0], [w, 0], [w, h], [0, h]]
        self._save_json()

    def _deactivate_all(self):
        for name, (_, _, active, _, _) in self.asset_controls.items():
            active.set(False)
        self._save_json()

    def _save_json(self):
        updated_assets = []
        for name, (minv, maxv, active, _, _) in self.asset_controls.items():
            min_val = minv.get()
            max_val = maxv.get()
            updated_assets.append({
                "name": name,
                "min_number": min_val,
                "max_number": max_val,
                "is_active": active.get()
            })
        self.map_data["assets"] = updated_assets
        with open(MAP_JSON_PATH, "w") as f:
            json.dump(self.map_data, f, indent=2)
        print("Map.json updated.")

    def save(self):
        self._save_json()

    def load(self, _=None):
        self._load_json()
        self._sync_assets()
        self._build_ui()
