import os
import json
import tkinter as tk
from tkinter import ttk, messagebox, simpledialog
from pages.range import Range
from pages.assets_editor import AssetEditor
from pages.batch_asset_editor import BatchAssetEditor


class TrailsPage(ttk.Frame):
    def __init__(self, parent):
        super().__init__(parent)
        self.current_trail_path = None
        self.trail_data = None
        self.trails_dir = None

        self.trail_list = tk.Listbox(self, height=10)
        self.trail_list.pack(side=tk.LEFT, fill=tk.Y, padx=10, pady=10)
        self.trail_list.bind("<<ListboxSelect>>", self._on_select_trail)

        trail_btns = ttk.Frame(self)
        trail_btns.pack(side=tk.LEFT, fill=tk.Y)
        ttk.Button(trail_btns, text="Add Trail", command=self._add_trail).pack(pady=(10, 2))

        self.editor_frame = ttk.Frame(self)
        self.editor_frame.pack_forget()

        self._build_editor()

    def load_data(self, data=None, json_path=None):
        if not json_path:
            return
        base_folder = os.path.dirname(json_path)
        self.trails_dir = os.path.join(base_folder, "trails")
        os.makedirs(self.trails_dir, exist_ok=True)
        self._refresh_trail_list()

    def _build_editor(self):
        self.name_var = tk.StringVar()
        ttk.Label(self.editor_frame, text="Trail Name:").pack(anchor="w")
        name_entry = ttk.Entry(self.editor_frame, textvariable=self.name_var, state="readonly")
        name_entry.pack(fill=tk.X, pady=4)

        self.width_range = Range(self.editor_frame, min_bound=10, max_bound=1000, label="Width")
        self.width_range.pack(fill=tk.X, pady=6)

        self.curve_range = Range(self.editor_frame, min_bound=0, max_bound=8, label="Curvyness", force_fixed=True)
        self.curve_range.pack(fill=tk.X, pady=6)

        self.asset_editor = AssetEditor(
            self.editor_frame,
            get_asset_list=lambda: self.trail_data["assets"],
            set_asset_list=lambda val: self.trail_data.__setitem__("assets", val),
            save_callback=self._save_json
        )
        self.asset_editor.pack(fill=tk.BOTH, expand=True, padx=4, pady=4)

        self.batch_asset_editor = BatchAssetEditor(
            self.editor_frame,
            save_callback=self._save_json
        )
        self.batch_asset_editor.pack(fill=tk.BOTH, expand=True, padx=4, pady=(0, 10))

    def _refresh_trail_list(self):
        self.trail_list.delete(0, tk.END)
        if not self.trails_dir:
            return
        os.makedirs(self.trails_dir, exist_ok=True)
        for file in sorted(os.listdir(self.trails_dir)):
            if file.endswith(".json"):
                try:
                    with open(os.path.join(self.trails_dir, file)) as f:
                        data = json.load(f)
                        self.trail_list.insert(tk.END, data.get("name", file[:-5]))
                except:
                    self.trail_list.insert(tk.END, file[:-5])

    def _on_select_trail(self, event):
        selection = self.trail_list.curselection()
        if not selection or not self.trails_dir:
            return
        name = self.trail_list.get(selection[0])
        path = os.path.join(self.trails_dir, f"{name}.json")
        if not os.path.exists(path):
            return
        try:
            with open(path) as f:
                self.trail_data = json.load(f)
                self.current_trail_path = path
                self._load_editor()
                self.editor_frame.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=10)
        except Exception as e:
            messagebox.showerror("Error", str(e))

    def _load_editor(self):
        self.name_var.set(self.trail_data["name"])
        self.width_range.set(self.trail_data.get("min_width", 0), self.trail_data.get("max_width", 0))
        self.curve_range.set(*[self.trail_data.get("curvyness", 0)] * 2)

        self.asset_editor.current_path = self.current_trail_path
        self.asset_editor.inherit_state = self.trail_data.get("inherits_map_assets", False)
        self.asset_editor.inherit_var.set(self.asset_editor.inherit_state)
        self.asset_editor.load_assets()

        self.batch_asset_editor.current_path = self.current_trail_path
        self.batch_asset_editor.load(self.trail_data.get("batch_assets", {}))

        self.width_range.var_max.trace_add("write", lambda *_: self._save_json())
        self.curve_range.var_max.trace_add("write", lambda *_: self._save_json())

    def _add_trail(self):
        name = simpledialog.askstring("New Trail", "Enter trail name:")
        if not name:
            return
        name = name.strip()
        safe_name = "".join(c for c in name if c.isalnum() or c in ("_", "-")).rstrip()
        path = os.path.join(self.trails_dir, f"{safe_name}.json")
        if os.path.exists(path):
            messagebox.showerror("Error", f"A trail named '{safe_name}' already exists.")
            return
        data = {
            "name": safe_name,
            "min_width": 100,
            "max_width": 200,
            "curvyness": 50,
            "inherits_map_assets": False,
            "assets": [],
            "batch_assets": {
                "has_batch_assets": False,
                "grid_spacing_min": 100,
                "grid_spacing_max": 100,
                "jitter_min": 0,
                "jitter_max": 0,
                "batch_assets": []
            }
        }
        with open(path, "w") as f:
            json.dump(data, f, indent=2)
        self._refresh_trail_list()

    def _save_json(self):
        if not self.current_trail_path:
            return
        self.trail_data["min_width"], self.trail_data["max_width"] = self.width_range.get()
        self.trail_data["curvyness"] = self.curve_range.get()[0]
        self.trail_data["inherits_map_assets"] = self.asset_editor.inherit_state
        self.trail_data["batch_assets"] = self.batch_asset_editor.save()  # Save batch assets

        try:
            with open(self.current_trail_path, "w") as f:
                json.dump(self.trail_data, f, indent=2)
        except Exception as e:
            messagebox.showerror("Save Failed", str(e))

    @staticmethod
    def get_json_filename():
        return "trails.json"
