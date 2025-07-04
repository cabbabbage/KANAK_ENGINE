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
        self._suspend_save = False

        # ─── Left pane: trail list and Add Trail button at bottom ────────────────
        left_frame = ttk.Frame(self)
        left_frame.pack(side=tk.LEFT, fill=tk.Y, padx=10, pady=10)

        self.trail_list = tk.Listbox(left_frame, height=15)
        self.trail_list.pack(side=tk.TOP, fill=tk.BOTH, expand=True)
        self.trail_list.bind("<<ListboxSelect>>", self._on_select_trail)

        add_trail_btn = tk.Button(
            left_frame,
            text="Add Trail",
            bg="#007BFF",
            fg="white",
            font=("Segoe UI", 11, "bold"),
            command=self._add_trail,
            width=15
        )
        add_trail_btn.pack(side=tk.BOTTOM, pady=(10, 0))

        # ─── Editor pane (initially hidden) ───────────────────────────────────
        self.editor_frame = ttk.Frame(self)
        self.editor_frame.pack_forget()
        self._build_editor()

    def load_data(self, data=None, json_path=None):
        if not json_path:
            return
        base = os.path.dirname(json_path)
        self.trails_dir = os.path.join(base, "trails")
        os.makedirs(self.trails_dir, exist_ok=True)
        self._refresh_trail_list()

    def _build_editor(self):
        self.name_var = tk.StringVar()
        ttk.Label(self.editor_frame, text="Trail Name:").pack(anchor="w")
        ttk.Entry(self.editor_frame, textvariable=self.name_var, state="readonly").pack(fill=tk.X, pady=4)

        # Width range
        self.width_range = Range(self.editor_frame, min_bound=10, max_bound=1000, label="Width")
        self.width_range.pack(fill=tk.X, pady=6)
        # Curvyness range
        self.curve_range = Range(self.editor_frame, min_bound=0, max_bound=8,
                                 label="Curvyness", force_fixed=True)
        self.curve_range.pack(fill=tk.X, pady=6)

        # Basic assets editor
        ttk.Label(self.editor_frame, text="Basic Assets", font=("Segoe UI", 11, "bold")).pack(anchor="w", pady=(10,4))
        self.asset_editor = AssetEditor(
            self.editor_frame,
            get_asset_list=lambda: self.trail_data["assets"],
            set_asset_list=lambda v: self.trail_data.__setitem__("assets", v),
            save_callback=self._save_json
        )
        self.asset_editor.pack(fill=tk.BOTH, expand=True, padx=8, pady=(0,20))

        # Batch asset editor
        ttk.Label(self.editor_frame, text="Batch Asset Editor", font=("Segoe UI", 11, "bold")).pack(anchor="w", pady=(0,4))
        self.batch_editor = BatchAssetEditor(self.editor_frame, save_callback=self._save_json)
        self.batch_editor.pack(fill=tk.BOTH, expand=True, padx=8, pady=(0,10))

    def _refresh_trail_list(self):
        self.trail_list.delete(0, tk.END)
        if not self.trails_dir:
            return
        for fn in sorted(os.listdir(self.trails_dir)):
            if fn.endswith(".json"):
                try:
                    with open(os.path.join(self.trails_dir, fn)) as f:
                        name = json.load(f).get("name", fn[:-5])
                except:
                    name = fn[:-5]
                self.trail_list.insert(tk.END, name)

    def _on_select_trail(self, event):
        sel = self.trail_list.curselection()
        if not sel or not self.trails_dir:
            return

        # Save previous trail
        if self.current_trail_path and self.trail_data:
            self._save_json()

        # Load new trail
        name = self.trail_list.get(sel[0])
        path = os.path.join(self.trails_dir, f"{name}.json")
        if not os.path.exists(path):
            return
        try:
            with open(path) as f:
                self.trail_data = json.load(f)
            self.current_trail_path = path

            # Ensure defaults
            if not isinstance(self.trail_data.get("assets"), list):
                self.trail_data["assets"] = []
            if not isinstance(self.trail_data.get("batch_assets"), dict):
                self.trail_data["batch_assets"] = {
                    "has_batch_assets": False,
                    "grid_spacing_min": 100,
                    "grid_spacing_max": 100,
                    "jitter_min": 0,
                    "jitter_max": 0,
                    "batch_assets": []
                }
            if "inherits_map_assets" not in self.trail_data:
                self.trail_data["inherits_map_assets"] = False

            self._load_editor()
            self.editor_frame.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=10)

        except Exception as e:
            messagebox.showerror("Error loading trail", str(e))

    def _load_editor(self):
        # Suspend autosave
        self._suspend_save = True

        # 1) Load batch editor first
        self.batch_editor.load(self.trail_data.get("batch_assets", {}))

        # 2) Populate simple fields
        self.name_var.set(self.trail_data.get("name", ""))
        self.width_range.set(
            self.trail_data.get("min_width", 0),
            self.trail_data.get("max_width", 0)
        )
        self.curve_range.set(
            self.trail_data.get("curvyness", 0),
            self.trail_data.get("curvyness", 0)
        )

        # 3) Load basic AssetEditor
        self.asset_editor.current_path = self.current_trail_path
        self.asset_editor.inherit_state = self.trail_data.get("inherits_map_assets", False)
        self.asset_editor.inherit_var.set(self.asset_editor.inherit_state)
        self.asset_editor.load_assets()

        # Resume autosave
        self._suspend_save = False

    def _add_trail(self):
        name = simpledialog.askstring("New Trail", "Enter trail name:")
        if not name:
            return
        safe = "".join(c for c in name.strip() if c.isalnum() or c in ("_","-"))
        path = os.path.join(self.trails_dir, f"{safe}.json")
        if os.path.exists(path):
            messagebox.showerror("Error", f"Trail '{safe}' already exists.")
            return

        data = {
            "name": safe,
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
        # select and load the new one
        idx = self.trail_list.get(0, tk.END).index(safe)
        self.trail_list.selection_clear(0, tk.END)
        self.trail_list.selection_set(idx)
        self._on_select_trail(None)

    def _save_json(self):
        if self._suspend_save or not self.current_trail_path:
            return
        # pull UI state
        self.trail_data["min_width"], self.trail_data["max_width"] = self.width_range.get()
        self.trail_data["curvyness"] = self.curve_range.get()[0]
        self.trail_data["inherits_map_assets"] = self.asset_editor.inherit_state
        self.trail_data["batch_assets"] = self.batch_editor.save()
        try:
            with open(self.current_trail_path, "w") as f:
                json.dump(self.trail_data, f, indent=2)
        except Exception as e:
            messagebox.showerror("Save Failed", str(e))

    @staticmethod
    def get_json_filename():
        return "trails.json"
