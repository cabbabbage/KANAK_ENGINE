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

        # ─── Top bar with Inherit, Add Asset, and Generate buttons ───────────
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

        # ─── Scrolling container (vertical only) ─────────────────────────────
        container = ttk.Frame(self)
        container.pack(fill=tk.BOTH, expand=True)

        self.asset_canvas = tk.Canvas(container)
        self.asset_scrollbar_y = ttk.Scrollbar(container, orient="vertical", command=self.asset_canvas.yview)
        self.asset_canvas.configure(yscrollcommand=self.asset_scrollbar_y.set)

        self.asset_frame = ttk.Frame(self.asset_canvas)
        self.asset_canvas.create_window((0, 0), window=self.asset_frame, anchor="nw")
        self.asset_frame.bind(
            "<Configure>",
            lambda e: self.asset_canvas.configure(scrollregion=self.asset_canvas.bbox("all"))
        )
        self.asset_canvas.bind_all("<MouseWheel>", self._on_mousewheel)

        self.asset_canvas.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        self.asset_scrollbar_y.pack(side=tk.RIGHT, fill=tk.Y)

    
    
    
    
    def _on_mousewheel(self, event):
        self.asset_canvas.yview_scroll(int(-1 * (event.delta / 120)), "units")

    def load_assets(self):
        for widget in self.asset_frame.winfo_children():
            widget.destroy()
        self.asset_frames.clear()

        for asset in self.get_asset_list():
            self._create_asset_widget(asset)

        # Immediately save the loaded state to synchronize with UI
        self.save_assets()



    def _create_asset_widget(self, asset):
        frame = ttk.Frame(self.asset_frame, relief="ridge", borderwidth=2, padding=5)
        # Make each asset frame stretch to the full width
        frame.pack(fill=tk.X, expand=True, padx=10, pady=4)


        # Remember the original dict so untouched keys persist
        frame.asset_data = asset

        # For selection highlighting
        frame.asset_name = asset["name"]
        frame.inherited = asset.get("inherited", False)

        def on_click(event):
            if self.selected_frame:
                self.selected_frame.config(style="TFrame")
            frame.config(style="Selected.TFrame")
            self.selected_frame = frame

        frame.bind("<Button-1>", on_click)

        # ─── Image or Tag Icon ─────────────────────────────────────────────────────
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

        # ─── Content Area ─────────────────────────────────────────────────────────
        content = ttk.Frame(frame)
        content.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        ttk.Label(content, text=asset["name"], font=("Segoe UI", 11, "bold")).pack(anchor="w")

        # Quantity Range
        range_widget = Range(
            content,
            min_bound=0, max_bound=2000,
            set_min=asset.get("min_number", 0),
            set_max=asset.get("max_number", 0)
        )
        range_widget.pack(fill=tk.X, pady=2)
        frame.range = range_widget  # ← ensure get_assets can always find it

        # Bind quantity sliders to save
        range_widget.var_min.trace_add("write", lambda *_: self.save_assets())
        range_widget.var_max.trace_add("write", lambda *_: self.save_assets())
        range_widget.var_random.trace_add("write", lambda *_: self.save_assets())

        # ─── Positioning Controls ─────────────────────────────────────────────────
        if self.positioning:
            # Overlap / Spacing toggles
            frame.check_overlap_var = tk.BooleanVar(value=asset.get("check_overlap", False))
            frame.check_min_spacing_var = tk.BooleanVar(value=asset.get("check_min_spacing", False))
            ttk.Checkbutton(content, text="Check Overlap", variable=frame.check_overlap_var,
                            command=self.save_assets).pack(anchor="w", pady=(6, 0))
            ttk.Checkbutton(content, text="Check Min Spacing", variable=frame.check_min_spacing_var,
                            command=self.save_assets).pack(anchor="w", pady=(0, 6))

            # Position dropdown
            position_var = tk.StringVar(value=asset.get("position", "Random"))
            frame.position_var = position_var

            def update_quantity_logic():
                pos = position_var.get()
                if pos in ("Center", "Distributed", "Exact Position"):
                    frame.range.set(1, 1)
                    frame.range.disable()
                else:
                    frame.range.enable()

            def update_position_options():
                # Clear old option widgets
                for child in option_container.winfo_children():
                    child.destroy()
                frame.position_options.clear()

                sel = position_var.get()
                if sel == "Perimeter":
                    opts = ttk.Frame(option_container); opts.pack(fill=tk.X)
                    frame.position_options["border_shift"] = Range(
                        opts, label="Border Shift (%)", min_bound=0, max_bound=200,
                        set_min=asset.get("border_shift_min", 100),
                        set_max=asset.get("border_shift_max", 100)
                    )
                    frame.position_options["border_shift"].pack(fill=tk.X, pady=2)
                    frame.position_options["sector_center"] = Range(
                        opts, label="Active Sector Center", min_bound=0, max_bound=360,
                        set_min=asset.get("sector_center_min", 0),
                        set_max=asset.get("sector_center_max", 0)
                    )
                    frame.position_options["sector_center"].pack(fill=tk.X, pady=2)
                    frame.position_options["sector_range"] = Range(
                        opts, label="Sector Range", min_bound=0, max_bound=360,
                        set_min=asset.get("sector_range_min", 360),
                        set_max=asset.get("sector_range_max", 360)
                    )
                    frame.position_options["sector_range"].pack(fill=tk.X, pady=2)

                elif sel == "Distributed":
                    opts = ttk.Frame(option_container); opts.pack(fill=tk.X)
                    frame.position_options["grid_spacing"] = Range(
                        opts, label="Grid Spacing", min_bound=0, max_bound=400,
                        set_min=asset.get("grid_spacing_min", 100),
                        set_max=asset.get("grid_spacing_max", 100)
                    )
                    frame.position_options["grid_spacing"].pack(fill=tk.X, pady=2)
                    frame.position_options["jitter"] = Range(
                        opts, label="Jitter", min_bound=0, max_bound=200,
                        set_min=asset.get("jitter_min", 0),
                        set_max=asset.get("jitter_max", 0)
                    )
                    frame.position_options["jitter"].pack(fill=tk.X, pady=2)
                    frame.position_options["empty_grid_spaces"] = Range(
                        opts, label="Empty Grid Spaces", min_bound=0, max_bound=100,
                        set_min=asset.get("empty_grid_spaces_min", 0),
                        set_max=asset.get("empty_grid_spaces_max", 0)
                    )
                    frame.position_options["empty_grid_spaces"].pack(fill=tk.X, pady=2)

                elif sel == "Exact Position":
                    opts = ttk.Frame(option_container); opts.pack(fill=tk.X)
                    rwx = Range(
                        opts, label="X Position (%)", min_bound=0, max_bound=100,
                        set_min=asset.get("ep_x_min", 50),
                        set_max=asset.get("ep_x_max", 50),
                        force_fixed=True
                    )
                    rwx.pack(fill=tk.X, pady=2)
                    frame.position_options["ep_x"] = rwx

                    rwy = Range(
                        opts, label="Y Position (%)", min_bound=0, max_bound=100,
                        set_min=asset.get("ep_y_min", 50),
                        set_max=asset.get("ep_y_max", 50),
                        force_fixed=True
                    )
                    rwy.pack(fill=tk.X, pady=2)
                    frame.position_options["ep_y"] = rwy

            # Dropdown widget
            position_dropdown = ttk.Combobox(
                content, textvariable=position_var,
                values=["Random", "Center", "Perimeter", "Entrance", "Distributed", "Exact Position", "Intersection"],
                state="readonly"
            )
            position_dropdown.pack(fill=tk.X, pady=(0, 4))
            position_dropdown.bind("<<ComboboxSelected>>", lambda *_: (
                update_position_options(),
                update_quantity_logic(),
                self.save_assets()
            ))

            # Container for per-type controls
            option_container = ttk.Frame(content); option_container.pack(fill=tk.X)
            frame.position_options = {}
            update_position_options()
            update_quantity_logic()

            # Bind *all* nested Ranges to save on change
            for rw in frame.position_options.values():
                rw.var_min.trace_add("write", lambda *_: self.save_assets())
                rw.var_max.trace_add("write", lambda *_: self.save_assets())
                if hasattr(rw, "var_random"):
                    rw.var_random.trace_add("write", lambda *_: self.save_assets())

        # ─── Delete Button ────────────────────────────────────────────────────────
        ttk.Button(frame, text="Delete", width=10,
                   command=lambda f=frame: self._delete_asset(f)).pack(side=tk.RIGHT, padx=8)

        # Finally, keep track of this frame
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
            # 1) Start with a *copy* of the original dict
            data = f.asset_data.copy()
            # 2) Overwrite just the fields we actually have controls for:
            data["min_number"] = f.range.get_min()
            data["max_number"] = f.range.get_max()
            if self.positioning and hasattr(f, "position_var"):
                data["position"] = f.position_var.get()
            # Keep any existing exact_position if you had one:
            data["exact_position"] = data.get("exact_position", None)
            data["inherited"] = getattr(f, "inherited", data.get("inherited", False))
            data["check_overlap"] = f.check_overlap_var.get() if hasattr(f, "check_overlap_var") else False
            data["check_min_spacing"] = f.check_min_spacing_var.get() if hasattr(f, "check_min_spacing_var") else False
            data["tag"] = getattr(f, "tag", data.get("tag", False))
            # 3) Overwrite any position-option ranges
            for key, rw in getattr(f, "position_options", {}).items():
                data[f"{key}_min"] = rw.get_min()
                data[f"{key}_max"] = rw.get_max()
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
