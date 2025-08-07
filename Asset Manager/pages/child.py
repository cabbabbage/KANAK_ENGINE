# === File: pages/child_assets.py ===
import os
import json
import copy
import tkinter as tk
from tkinter import messagebox
from pages.area_ui import AreaUI
from pages.assets_editor import AssetEditor

class ChildAssetsPage(tk.Frame):
    def __init__(self, parent):
        super().__init__(parent)
        self.asset_path = None
        self.child_frames = []
        self._loaded = False

        # Page background
        self.configure(bg='#1e1e1e')

        # Header with title and Add button
        header = tk.Frame(self, bg='#1e1e1e')
        header.pack(fill=tk.X, pady=(10, 20))
        title = tk.Label(
            header, text="Child Asset Regions",
            font=("Segoe UI", 20, "bold"),
            fg="#005f73", bg='#1e1e1e'
        )
        title.pack(side=tk.LEFT, padx=12)
        add_btn = tk.Button(
            header, text="Add New Region",
            bg="#28a745", fg="white",
            font=("Segoe UI", 13, "bold"), width=20,
            command=self._add_child_region
        )
        add_btn.pack(side=tk.RIGHT, padx=12)

        # Scrollable area
        self.canvas = tk.Canvas(self, bg='#2a2a2a', highlightthickness=0)
        self.scroll_frame = tk.Frame(self.canvas, bg='#2a2a2a')
        window_id = self.canvas.create_window((0, 0), window=self.scroll_frame, anchor='nw')
        # update scrollregion
        self.scroll_frame.bind(
            '<Configure>',
            lambda e: self.canvas.configure(scrollregion=self.canvas.bbox('all'))
        )
        # span full width
        self.canvas.bind(
            '<Configure>',
            lambda e: self.canvas.itemconfig(window_id, width=e.width)
        )
        # mousewheel on hover
        def _scroll(evt):
            self.canvas.yview_scroll(int(-1*(evt.delta/120)), 'units')
        self.scroll_frame.bind('<Enter>', lambda e: self.canvas.bind_all('<MouseWheel>', _scroll))
        self.scroll_frame.bind('<Leave>', lambda e: self.canvas.unbind_all('<MouseWheel>'))
        self.canvas.pack(fill=tk.BOTH, expand=True)

    def _add_child_region(self, entry_data=None):
        idx = len(self.child_frames)
        # Container frame styled dark
        frame = tk.LabelFrame(
            self.scroll_frame,
            text=f"Region {idx+1}",
            bg='#2a2a2a', fg='#DDDDDD', font=("Segoe UI", 13, "bold"),
            bd=1, relief='solid', padx=10, pady=10
        )
        frame.grid(row=idx, column=0, pady=10, sticky="ew", padx=12)
        frame.columnconfigure(1, weight=1)

        json_path = entry_data.get("json_path") if entry_data else None
        z_offset = entry_data.get("z_offset", 0) if entry_data else 0
        area_data = ({k: copy.deepcopy(v) for k, v in entry_data.items()
                      if k not in ("json_path", "z_offset", "assets")}
                     if isinstance(entry_data, dict) else None)

        # Z Offset
        tk.Label(
            frame, text="Z Offset:", bg='#2a2a2a', fg='#FFFFFF', font=("Segoe UI", 12)
        ).grid(row=0, column=0, sticky="w")
        z_var = tk.IntVar(value=z_offset)
        tk.Spinbox(
            frame, from_=-9999, to=9999,
            textvariable=z_var, width=10,
            font=("Segoe UI", 12)
        ).grid(row=0, column=1, sticky="w", padx=6)

        # AreaUI
        region_file = json_path or f"child_assets_{idx + 1}.json"
        full_path = os.path.join(os.path.dirname(self.asset_path or ''), region_file)

        area_ui = AreaUI(
            frame, full_path,
            label_text="Area Region:",
            autosave_callback=self.save
        )
        area_ui.frames_source = os.path.join(os.path.dirname(self.asset_path or ''), "default")
        area_ui.grid(row=1, column=0, columnspan=4, sticky="ew", padx=6, pady=4)
        area_ui._load_area_json()
        area_ui._draw_preview()

        # AssetEditor
        asset_list = entry_data.get("assets", []) if isinstance(entry_data, dict) else []
        asset_editor = AssetEditor(
            frame,
            lambda: asset_list,
            lambda v: (asset_list.clear() or asset_list.extend(v)),
            save_callback=self.save
        )
        asset_editor.grid(row=2, column=0, columnspan=4, sticky="ew", pady=10)
        asset_editor.load_assets()

        # Build entry dict
        entry = {
            "frame": frame,
            "z_var": z_var,
            "json_path": region_file,
            "area_ui": area_ui,
            "asset_editor": asset_editor,
            "area_data": area_data,
            "original_area_data": copy.deepcopy(area_data) if area_data else None,
            "area_edited": False
        }

        # Delete button now references entry safely
        delete_btn = tk.Button(
            frame, text="X",
            command=lambda e=entry: self._delete_region(e),
            bg="#D9534F", fg="white",
            font=("Segoe UI", 12, "bold"), width=3
        )
        delete_btn.grid(row=0, column=3, padx=4)

        self.child_frames.append(entry)
        if self._loaded:
            self.save()

    def _delete_region(self, entry):
        json_path = entry.get("json_path")
        if json_path:
            full_path = os.path.join(os.path.dirname(self.asset_path or ''), json_path)
            try:
                if os.path.isfile(full_path):
                    os.remove(full_path)
            except Exception as e:
                messagebox.showerror("Error", f"Failed to delete file '{full_path}': {e}")

        self.child_frames.remove(entry)
        entry["frame"].destroy()
        self.save()

    def load(self, info_path):
        self.asset_path = info_path
        for e in self.child_frames:
            e['frame'].destroy()
        self.child_frames.clear()

        try:
            with open(info_path, 'r') as f:
                data = json.load(f)
            for item in data.get("child_assets", []):
                path = item.get("json_path") if isinstance(item, dict) else item
                full = os.path.join(os.path.dirname(info_path), path)
                if not os.path.exists(full):
                    continue
                with open(full, 'r') as cf:
                    child = json.load(cf)
                child["json_path"] = path
                self._add_child_region(child)
        except Exception as e:
            messagebox.showerror("Error", f"Failed to load child assets: {e}")

        self._loaded = True

    def save(self):
        if not self.asset_path or not self._loaded:
            return

        base_folder = os.path.dirname(self.asset_path)
        saved = []

        for idx, entry in enumerate(self.child_frames):
            z_offset = entry['z_var'].get()
            assets = entry['asset_editor'].get_assets()
            filename = f"child_assets_{idx + 1}.json"
            full_path = os.path.join(base_folder, filename)

            # ✅ Grab the latest area data from AreaUI
            area_data = entry['area_ui'].area_data

            data = {
                "z_offset": z_offset,
                "assets": assets
            }

            if isinstance(area_data, dict) and area_data:
                data.update(area_data)

            try:
                with open(full_path, 'w') as f:
                    json.dump(data, f, indent=2)
                saved.append(filename)
                entry["json_path"] = filename
            except Exception as e:
                messagebox.showerror("Error", f"Failed to save {filename}: {e}")

        # Write back to parent info.json
        try:
            with open(self.asset_path, 'r') as f:
                parent = json.load(f)
        except:
            parent = {}

        parent['child_assets'] = saved

        try:
            with open(self.asset_path, 'w') as f:
                json.dump(parent, f, indent=2)
        except Exception as e:
            messagebox.showerror("Error", f"Failed to update parent info.json: {e}")
