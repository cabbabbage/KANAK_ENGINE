import os
import json
import tkinter as tk
from tkinter import ttk, messagebox
from PIL import Image, ImageTk
from pages.range import Range
from pages.search import AssetSearchWindow

SRC_DIR = "SRC"

class AssetEditor(ttk.Frame):
    def __init__(self, parent, get_asset_list, set_asset_list, save_callback, positioning=False, current_path=None):
        super().__init__(parent)
        self.get_asset_list = get_asset_list
        self.set_asset_list = set_asset_list
        self.save_callback = save_callback
        self.asset_frames = []
        self.positioning = positioning
        self.current_path = current_path
        self.inherit_var = tk.BooleanVar(value=False)

        top_bar = ttk.Frame(self)
        top_bar.pack(fill=tk.X, pady=4)

        if not (self.current_path and self.current_path.endswith("map_assets.json")):
            inherit_check = ttk.Checkbutton(
                top_bar, text="Inherit Asset",
                variable=self.inherit_var,
                command=self._handle_inherit_toggle
            )
            inherit_check.pack(side=tk.LEFT, padx=6)

        add_btn = tk.Button(
            top_bar, text="Add Asset", bg="#007BFF", fg="white",
            font=("Segoe UI", 11, "bold"), command=self._add_asset_dialog
        )
        add_btn.pack(side=tk.LEFT, padx=4)

        self.asset_canvas = tk.Canvas(self)
        self.asset_scrollbar = ttk.Scrollbar(self, orient="vertical", command=self.asset_canvas.yview)
        self.asset_canvas.configure(yscrollcommand=self.asset_scrollbar.set)
        self.asset_frame = ttk.Frame(self.asset_canvas)
        self.asset_canvas.create_window((0, 0), window=self.asset_frame, anchor="nw")
        self.asset_frame.bind("<Configure>", lambda e: self.asset_canvas.configure(scrollregion=self.asset_canvas.bbox("all")))
        self.asset_canvas.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        self.asset_scrollbar.pack(side=tk.RIGHT, fill=tk.Y)

    def load_assets(self):
        for widget in self.asset_frame.winfo_children():
            widget.destroy()
        self.asset_frames.clear()

        for asset in self.get_asset_list():
            self._create_asset_widget(asset)

        # Inherit if needed on load
        if self.inherit_var.get():
            self._inherit_assets()

    def _create_asset_widget(self, asset):
        frame = ttk.Frame(self.asset_frame, relief="ridge", borderwidth=2, padding=5)
        frame.pack(fill=tk.X, padx=10, pady=4)
        frame.asset_name = asset["name"]
        frame.inherited = asset.get("inherited", False)

        image_path = os.path.join(SRC_DIR, asset["name"], "default", "0.png")
        try:
            img = Image.open(image_path)
            img.thumbnail((64, 64))
            photo = ImageTk.PhotoImage(img)
            img_label = tk.Label(frame, image=photo)
            img_label.image = photo
            img_label.pack(side=tk.LEFT, padx=6)
        except Exception:
            pass

        content = ttk.Frame(frame)
        content.pack(side=tk.LEFT, fill=tk.X, expand=True)
        ttk.Label(content, text=asset["name"], font=("Segoe UI", 11, "bold")).pack(anchor="w")

        range_widget = Range(content, set_min=asset.get("min_number", 0), set_max=asset.get("max_number", 0))
        range_widget.pack(fill=tk.X, pady=2)
        frame.range = range_widget

        def save_on_change(*_):
            self.save_assets()

        range_widget.var_min.trace_add("write", save_on_change)
        range_widget.var_max.trace_add("write", save_on_change)
        range_widget.var_random.trace_add("write", save_on_change)

        if self.positioning:
            pos_label = ttk.Label(content, text="Position:")
            pos_label.pack(anchor="w", pady=(4, 0))

            position_var = tk.StringVar(value=asset.get("position", "Null"))
            frame.position_var = position_var

            position_dropdown = ttk.Combobox(
                content,
                textvariable=position_var,
                values=["Null", "Center", "Perimeter", "Intersection", "Between Trails"],
                state="readonly"
            )
            position_dropdown.pack(fill=tk.X, pady=(0, 4))
            position_dropdown.bind("<<ComboboxSelected>>", lambda *_: self.save_assets())

        ttk.Button(frame, text="Delete", command=lambda f=frame: self._delete_asset(f)).pack(side=tk.RIGHT)
        self.asset_frames.append(frame)

    def _add_asset_dialog(self):
        window = AssetSearchWindow(self)
        window.wait_window()
        name = getattr(window, "selected_asset", None)
        if name:
            new_asset = {
                "name": name,
                "min_number": 0,
                "max_number": 0,
                "position": None,
                "exact_position": None,
                "inherited": False
            }
            self.get_asset_list().append(new_asset)
            self._create_asset_widget(new_asset)
            self.save_assets()

    def _delete_asset(self, frame):
        if frame in self.asset_frames:
            self.asset_frames.remove(frame)
        self.asset_frames = [f for f in self.asset_frames if f != frame]
        self.get_asset_list()[:] = self.get_assets()
        frame.destroy()
        self.save_assets()

    def get_assets(self):
        assets = []
        for f in self.asset_frames:
            data = {
                "name": f.asset_name,
                "min_number": f.range.get_min(),
                "max_number": f.range.get_max(),
                "position": f.position_var.get() if self.positioning and hasattr(f, "position_var") else None,
                "exact_position": None,
                "inherited": getattr(f, "inherited", False)
            }
            assets.append(data)
        return assets

    def save_assets(self):
        self.set_asset_list(self.get_assets())
        self.save_callback()

    def _handle_inherit_toggle(self):
        if self.inherit_var.get():
            self._inherit_assets()
        else:
            self._remove_inherited_assets()
        self.save_assets()
        self.refresh()

    def _inherit_assets(self):
        if not self.current_path:
            return
        base_dir = os.path.dirname(self.current_path)
        inherit_path = os.path.join(base_dir, "map_assets.json")
        if not os.path.exists(inherit_path):
            return

        try:
            with open(inherit_path, "r") as f:
                inherited_data = json.load(f).get("assets", [])
        except Exception as e:
            messagebox.showerror("Error", f"Failed to load inherited map_assets.json\n\n{e}")
            return

        existing_names = {a["name"] for a in self.get_asset_list()}
        new_assets = []
        for asset in inherited_data:
            if asset["name"] not in existing_names:
                asset["min_number"] = asset.get("min_number", 0)
                asset["max_number"] = asset.get("max_number", 0)
                asset["position"] = asset.get("position", None)
                asset["exact_position"] = asset.get("exact_position", None)
                asset["inherited"] = True
                new_assets.append(asset)

        self.get_asset_list().extend(new_assets)
        for asset in new_assets:
            self._create_asset_widget(asset)

    def _remove_inherited_assets(self):
        remaining = [a for a in self.get_asset_list() if not a.get("inherited", False)]
        self.set_asset_list(remaining)
        self.load_assets()

    def refresh(self):
        self.load_assets()

    def reload(self):
        self.load_assets()
