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
        frame.pack(fill=tk.X, padx=10, pady=4, ipadx=20)
        frame.asset_name = asset["name"]
        frame.inherited = asset.get("inherited", False)

        def on_click(event):
            if self.selected_frame:
                self.selected_frame.config(style="TFrame")
            frame.config(style="Selected.TFrame")
            self.selected_frame = frame

        frame.bind("<Button-1>", on_click)

        # Image or Tag Symbol
        if asset.get("tag", False):
            tag_label = tk.Label(frame, text="#", font=("Segoe UI", 32, "bold"), width=2, height=1)
            tag_label.pack(side=tk.LEFT, padx=6)
        else:
            image_path = os.path.join(SRC_DIR, asset["name"], "default", "0.png")
            if os.path.isfile(image_path):
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
            frame.check_overlap_var = tk.BooleanVar(value=asset.get("check_overlap", False))
            frame.check_min_spacing_var = tk.BooleanVar(value=asset.get("check_min_spacing", False))

            ttk.Checkbutton(content, text="Check Overlap", variable=frame.check_overlap_var, command=self.save_assets).pack(anchor="w", pady=(6, 0))
            ttk.Checkbutton(content, text="Check Min Spacing", variable=frame.check_min_spacing_var, command=self.save_assets).pack(anchor="w", pady=(0, 6))

            ttk.Label(content, text="Position:").pack(anchor="w")

            position_var = tk.StringVar(value=asset.get("position", "Random"))
            frame.position_var = position_var

            def update_quantity_logic():
                pos = position_var.get()
                if pos in ["Center", "Distributed", "Exact Position"]:
                    frame.range.set(1, 1)
                    frame.range.disable()
                else:
                    frame.range.enable()

            def update_position_options(*_):
                for child in option_container.winfo_children():
                    child.destroy()
                selected = position_var.get()

                if selected == "Perimeter":
                    perimeter_opts = ttk.Frame(option_container)
                    perimeter_opts.pack(fill=tk.X)

                    frame.position_options["border_shift"] = Range(perimeter_opts, label="Border Shift (%)", min_bound=0, max_bound=200, set_min=100, set_max=100); frame.position_options["border_shift"].pack(fill=tk.X, pady=(2, 4))
                    frame.position_options["sector_center"] = Range(perimeter_opts, label="Active Sector Center", min_bound=0, max_bound=360, set_min=0, set_max=0); frame.position_options["sector_center"].pack(fill=tk.X, pady=(2, 4))
                    frame.position_options["sector_range"] = Range(perimeter_opts, label="Sector Range", min_bound=0, max_bound=360, set_min=360, set_max=360); frame.position_options["sector_range"].pack(fill=tk.X, pady=(2, 4))


                elif selected == "Distributed":
                    d = ttk.Frame(option_container)
                    d.pack(fill=tk.X)
                    frame.position_options["grid_spacing"] = Range(d, label="Grid Spacing", min_bound=0, max_bound=400, set_min=100, set_max=100); frame.position_options["grid_spacing"].pack(fill=tk.X)
                    frame.position_options["jitter"] = Range(d, label="Jitter", min_bound=0, max_bound=200, set_min=0, set_max=0); frame.position_options["jitter"].pack(fill=tk.X)
                    frame.position_options["empty_grid_spaces"] = Range(d, label="Empty Grid Spaces", min_bound=0, max_bound=100, set_min=0, set_max=0); frame.position_options["empty_grid_spaces"].pack(fill=tk.X)

                elif selected == "Exact Position":
                    e = ttk.Frame(option_container)
                    e.pack(fill=tk.X)

                    frame.position_options["ep_x"] = Range(
                        e, label="X Position (%)", min_bound=0, max_bound=100, set_min=50, set_max=50
                    )
                    frame.position_options["ep_x"].var_random.set(False)
                    frame.position_options["ep_x"]._draw_sliders()
                    frame.position_options["ep_x"].pack(fill=tk.X)

                    frame.position_options["ep_y"] = Range(
                        e, label="Y Position (%)", min_bound=0, max_bound=100, set_min=50, set_max=50
                    )
                    frame.position_options["ep_y"].var_random.set(False)
                    frame.position_options["ep_y"]._draw_sliders()
                    frame.position_options["ep_y"].pack(fill=tk.X)


            position_dropdown = ttk.Combobox(content, textvariable=position_var, values=["Random", "Center", "Perimeter", "Entrance", "Distributed", "Exact Position", "Intersection"], state="readonly")
            position_dropdown.pack(fill=tk.X, pady=(0, 4))
            position_dropdown.bind("<<ComboboxSelected>>", lambda *_: [update_position_options(), update_quantity_logic(), self.save_assets()])

            option_container = ttk.Frame(content)
            option_container.pack(fill=tk.X)
            frame.position_options = {}

            update_position_options()
            update_quantity_logic()

        ttk.Button(frame, text="Delete", command=lambda f=frame: self._delete_asset(f), width=10).pack(side=tk.RIGHT, padx=8)
        self.asset_frames.append(frame)

    def _add_asset_dialog(self):
        window = AssetSearchWindow(self)
        window.wait_window()
        result = getattr(window, "selected_result", None)
        if isinstance(result, tuple):
            kind, name = result
            is_tag = (kind == "tag")
        else:
            return  # Invalid result
        if name:
            new_asset = {
                "name": name,
                "min_number": 0,
                "max_number": 0,
                "position": "Random",
                "exact_position": None,
                "tag": is_tag,
                "check_overlap": False,
                "check_min_spacing": False
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
                "min_number": f.range.get_min() if f.range.var_random.get() else f.range.get_min(),
                "max_number": f.range.get_max() if f.range.var_random.get() else f.range.get_min(),
                "position": f.position_var.get() if self.positioning and hasattr(f, "position_var") else "Distributed",
                "exact_position": None,
                "inherited": getattr(f, "inherited", False),
                "check_overlap": f.check_overlap_var.get() if hasattr(f, "check_overlap_var") else False,
                "check_min_spacing": f.check_min_spacing_var.get() if hasattr(f, "check_min_spacing_var") else False,
                "tag": getattr(f, "tag", False)
            }
            for key, rw in f.position_options.items():
                data[key + "_min"] = rw.get_min() if rw.var_random.get() else rw.get_min()
                data[key + "_max"] = rw.get_max() if rw.var_random.get() else rw.get_min()
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
