import os
import json
import tkinter as tk
from tkinter import ttk, messagebox
from pages.boundary import BoundaryConfigurator
from pages.assets_editor import AssetEditor
from pages.range import Range

SRC_DIR = "SRC"

class ChildAssetsPage(ttk.Frame):
    def __init__(self, parent):
        super().__init__(parent)
        self.child_frames = []
        self.asset_path = None  # info.json

        self.FONT = ('Segoe UI', 14)
        self.FONT_BOLD = ('Segoe UI', 18, 'bold')

        ttk.Label(self, text="Child Asset Regions", font=self.FONT_BOLD).pack(pady=(10, 10))

        self.container = ttk.Frame(self)
        self.container.pack(fill=tk.BOTH, expand=True)

        self.scroll_canvas = tk.Canvas(self.container)
        self.scroll_frame = ttk.Frame(self.scroll_canvas)
        self.scrollbar = ttk.Scrollbar(self.container, orient="vertical", command=self.scroll_canvas.yview)
        self.scroll_canvas.configure(yscrollcommand=self.scrollbar.set)

        self.scroll_canvas.create_window((0, 0), window=self.scroll_frame, anchor="nw")
        self.scroll_frame.bind("<Configure>", lambda e: self.scroll_canvas.configure(scrollregion=self.scroll_canvas.bbox("all")))

        self.scroll_canvas.pack(side="left", fill="both", expand=True)
        self.scrollbar.pack(side="right", fill="y")

        # Add/save buttons
        btn_frame = ttk.Frame(self)
        btn_frame.pack(fill=tk.X, pady=12)
        ttk.Button(btn_frame, text="Add Region", command=self._add_child_region).pack(side=tk.LEFT, padx=10)
        ttk.Button(btn_frame, text="Save", command=self.save).pack(side=tk.LEFT, padx=10)

    def _add_child_region(self, data=None):
        idx = len(self.child_frames)
        frame = ttk.LabelFrame(self.scroll_frame, text=f"Region {idx+1}", padding=10)
        frame.grid(row=idx, column=0, pady=10, sticky="ew")
        frame.columnconfigure(1, weight=1)

        region_data = data or {}
        json_path = region_data.get("json_path")
        z_offset = region_data.get("z_offset", 0)

        # Z Offset (single value)
        ttk.Label(frame, text="Z Offset:").grid(row=0, column=0, sticky="w")
        z_var = tk.IntVar(value=z_offset)
        z_spin = ttk.Spinbox(frame, from_=-9999, to=9999, textvariable=z_var, width=10)
        z_spin.grid(row=0, column=1, sticky="w", padx=6)

        # Area Editor
        area_data = region_data.get("area")
        area_label = ttk.Label(frame, text="(none)" if not area_data else "Area defined")
        area_label.grid(row=1, column=0, columnspan=2, sticky="w")
        edit_button = tk.Button(frame, text="Edit Area")
        edit_button.grid(row=1, column=2, padx=6)

        # Asset Editor
        asset_list = region_data.get("assets", [])
        asset_editor = AssetEditor(frame, lambda: asset_list, lambda v: asset_list.clear() or asset_list.extend(v), lambda: None)
        asset_editor.grid(row=2, column=0, columnspan=3, sticky="ew", pady=10)
        asset_editor.load_assets()

        entry = {
            "frame": frame,
            "z_var": z_var,
            "json_path": json_path,
            "area_label": area_label,
            "asset_editor": asset_editor,
            "area_data": area_data
        }

        def edit_area():
            if not self.asset_path:
                messagebox.showerror("Error", "No parent asset path defined.")
                return

            base_folder = os.path.join(os.path.dirname(self.asset_path), "default")
            if not os.path.isdir(base_folder):
                messagebox.showerror("Error", f"Parent frame folder does not exist: {base_folder}")
                return

            def save_callback(area):
                entry['area_data'] = area
                entry['area_label'].config(text="Area defined")

            editor = BoundaryConfigurator(self.winfo_toplevel(), base_folder, save_callback)
            editor.grab_set()

        edit_button.config(command=edit_area)
        self.child_frames.append(entry)

    def load(self, info_path):
        self.asset_path = info_path
        for e in self.child_frames:
            e['frame'].destroy()
        self.child_frames.clear()

        try:
            data = json.load(open(info_path))
            for entry in data.get("child_assets", []):
                if isinstance(entry, dict):
                    path = entry.get("json_path")
                else:
                    path = entry
                full_path = os.path.join(os.path.dirname(info_path), path)
                if not os.path.exists(full_path):
                    continue
                with open(full_path) as f:
                    child_data = json.load(f)
                child_data["json_path"] = path
                self._add_child_region(child_data)
        except Exception as e:
            messagebox.showerror("Error", f"Failed to load child assets: {e}")

    def save(self):
        if not self.asset_path:
            return

        base_folder = os.path.dirname(self.asset_path)
        saved_entries = []

        for idx, entry in enumerate(self.child_frames):
            z_offset = entry['z_var'].get()
            area = entry.get("area_data")
            assets = entry['asset_editor'].get_assets()

            filename = f"child_assets_{idx + 1}.json"
            full_path = os.path.join(base_folder, filename)
            data = {
                "z_offset": z_offset,
                "area": area,
                "assets": assets
            }

            try:
                with open(full_path, 'w') as f:
                    json.dump(data, f, indent=2)
                saved_entries.append(filename)
            except Exception as e:
                messagebox.showerror("Error", f"Failed to save {filename}: {e}")

        try:
            with open(self.asset_path) as f:
                parent_data = json.load(f)
        except:
            parent_data = {}

        parent_data['child_assets'] = saved_entries

        try:
            with open(self.asset_path, 'w') as f:
                json.dump(parent_data, f, indent=2)
            messagebox.showinfo("Saved", "Child asset regions saved.")
        except Exception as e:
            messagebox.showerror("Error", f"Failed to update parent info.json: {e}")
