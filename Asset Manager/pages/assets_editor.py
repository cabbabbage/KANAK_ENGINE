import os
import json
import tkinter as tk
from tkinter import ttk, messagebox
from PIL import Image, ImageTk
from pages.range import Range
from pages.search import AssetSearchWindow
from pages.random_asset_generator import RandomAssetGenerator

SRC_DIR = "SRC"

class AssetEditor(ttk.Frame):
    def __init__(self, parent, get_asset_list, set_asset_list, save_callback, positioning=True, current_path=None):
        super().__init__(parent)
        self.get_asset_list = get_asset_list
        self.set_asset_list = set_asset_list
        self.save_callback = save_callback
        self.asset_frames = []
        self.positioning = positioning
        self.current_path = current_path
        self.inherit_var = tk.BooleanVar(value=False)
        self.inherit_state = False
        self.selected_frame = None

        top_bar = ttk.Frame(self)
        top_bar.pack(fill=tk.X, pady=4, padx=20)

        if not (self.current_path and self.current_path.endswith("map_assets.json")):
            inherit_check = ttk.Checkbutton(
                top_bar, text="Inherit Asset",
                variable=self.inherit_var,
                command=self._handle_inherit_toggle
            )
            inherit_check.pack(side=tk.LEFT, padx=6)

        add_btn = tk.Button(
            top_bar, text="Add Asset", bg="#007BFF", fg="white",
            font=("Segoe UI", 11, "bold"), command=self._add_asset_dialog,
            width=15
        )
        add_btn.pack(side=tk.LEFT, padx=10)

        generate_btn = tk.Button(
            top_bar, text="Generate Random Asset Set", bg="#007BFF", fg="white",
            font=("Segoe UI", 11, "bold"), command=self._open_random_generator,
            width=26
        )
        generate_btn.pack(side=tk.LEFT, padx=10)

        container = ttk.Frame(self)
        container.pack(fill=tk.BOTH, expand=True)

        self.asset_canvas = tk.Canvas(container)
        self.asset_scrollbar_y = ttk.Scrollbar(container, orient="vertical", command=self.asset_canvas.yview)
        self.asset_scrollbar_x = ttk.Scrollbar(container, orient="horizontal", command=self.asset_canvas.xview)
        self.asset_canvas.configure(yscrollcommand=self.asset_scrollbar_y.set, xscrollcommand=self.asset_scrollbar_x.set)

        self.asset_frame = ttk.Frame(self.asset_canvas)
        self.asset_canvas.create_window((0, 0), window=self.asset_frame, anchor="nw")
        self.asset_frame.bind("<Configure>", lambda e: self.asset_canvas.configure(scrollregion=self.asset_canvas.bbox("all")))
        self.asset_canvas.bind_all("<MouseWheel>", self._on_mousewheel)

        self.asset_canvas.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        self.asset_scrollbar_y.pack(side=tk.RIGHT, fill=tk.Y)
        self.asset_scrollbar_x.pack(side=tk.BOTTOM, fill=tk.X)

    def _on_mousewheel(self, event):
        self.asset_canvas.yview_scroll(int(-1 * (event.delta / 120)), "units")

    def load_assets(self):
        for widget in self.asset_frame.winfo_children():
            widget.destroy()
        self.asset_frames.clear()

        for asset in self.get_asset_list():
            self._create_asset_widget(asset)

    def _create_asset_widget(self, asset):
        frame = ttk.Frame(self.asset_frame, relief="ridge", borderwidth=2, padding=5)
        frame.pack(fill=tk.X, padx=10, pady=4, ipadx=20)  # Increased width with ipadx
        frame.asset_name = asset["name"]
        frame.inherited = asset.get("inherited", False)

        def on_click(event):
            if self.selected_frame:
                self.selected_frame.config(style="TFrame")
            frame.config(style="Selected.TFrame")
            self.selected_frame = frame

        frame.bind("<Button-1>", on_click)

        image_path = os.path.join(SRC_DIR, asset["name"], "default", "0.png")
        try:
            img = Image.open(image_path)
            img.thumbnail((64, 64), Image.Resampling.LANCZOS)
            photo = ImageTk.PhotoImage(img)
            img_label = tk.Label(frame, image=photo)
            img_label.image = photo
            img_label.pack(side=tk.LEFT, padx=6)
        except Exception:
            pass

        content = ttk.Frame(frame)
        content.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        ttk.Label(content, text=asset["name"], font=("Segoe UI", 11, "bold")).pack(anchor="w")

        range_widget = Range(content, min_bound=0, max_bound=2000, set_min=asset.get("min_number", 0), set_max=asset.get("max_number", 0))
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
                values=["Null", "Center", "Perimeter", "Intersection", "Distributed"],
                state="readonly"
            )
            position_dropdown.pack(fill=tk.X, pady=(0, 4))
            position_dropdown.bind("<<ComboboxSelected>>", lambda *_: self.save_assets())

        ttk.Button(frame, text="Delete", command=lambda f=frame: self._delete_asset(f), width=10).pack(side=tk.RIGHT, padx=8)
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
                "position": f.position_var.get() if self.positioning and hasattr(f, "position_var") else "Distributed",
                "exact_position": None,
                "inherited": getattr(f, "inherited", False)
            }
            assets.append(data)
        return assets

    def save_assets(self):
        self.set_asset_list(self.get_assets())
        self.save_callback()

    def _handle_inherit_toggle(self):
        self.inherit_state = self.inherit_var.get()
        self.save_callback()

    def refresh(self):
        self.load_assets()

    def reload(self):
        self.load_assets()

    def _open_random_generator(self):
        current_assets = self.get_assets()

        def callback(result):
            if isinstance(result, list):
                self.set_asset_list(result)
                self.refresh()
                self.save_callback()

        RandomAssetGenerator(self, current_assets, callback)
