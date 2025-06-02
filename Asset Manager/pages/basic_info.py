import os
import json
import shutil
import tkinter as tk
from tkinter import ttk, messagebox
from pages.AnimationConfig import AnimationEditor
from pages.range import Range

class BasicInfoPage(ttk.Frame):
    def __init__(self, parent, on_rename_callback=None):
        super().__init__(parent)
        self.asset_path = None
        self.on_rename = on_rename_callback
        self._suppress_save = False
        self._trace_callbacks = []

        self.duplicatable_var = tk.BooleanVar(value=False)
        self.name_var = tk.StringVar()
        self.type_var = tk.StringVar()

        font_large = ("Segoe UI", 14)
        style = ttk.Style()
        style.configure("Big.TButton", font=("Segoe UI", 14))
        style.configure("Big.TEntry", font=font_large)
        style.configure("Big.TMenubutton", font=font_large)
        style.configure("Big.TCheckbutton", font=font_large)

        ttk.Label(self, text="Asset Name:", font=font_large).grid(row=0, column=0, sticky="w", padx=16, pady=10)
        self.name_entry = ttk.Entry(self, textvariable=self.name_var, width=40, style="Big.TEntry")
        self.name_entry.grid(row=0, column=1, columnspan=2, sticky="w", padx=16, pady=10)

        ttk.Label(self, text="Asset Type:", font=font_large).grid(row=1, column=0, sticky="w", padx=16, pady=10)
        self.type_menu = ttk.OptionMenu(self, self.type_var, "")
        self.type_menu.config(style="Big.TMenubutton", width=30)
        self.type_menu.grid(row=1, column=1, columnspan=2, sticky="w", padx=16, pady=10)

        ttk.Checkbutton(self,
                        text="Duplicatable Asset",
                        variable=self.duplicatable_var,
                        style="Big.TCheckbutton")\
            .grid(row=2, column=0, columnspan=3, sticky="w", padx=16, pady=6)

        self.dup_range = Range(self, min_bound=30, max_bound=9999, set_min=30, set_max=60, label="Duplication Interval (s)")
        self.dup_range.grid(row=3, column=0, columnspan=5, sticky="ew", padx=16)

        self.depth_range = Range(self, min_bound=0, max_bound=100, set_min=0, set_max=2, label="Child Depth")
        self.depth_range.grid(row=4, column=0, columnspan=5, sticky="ew", padx=16)

        self.anim_editor = AnimationEditor(self)
        self.anim_editor.grid(row=5, column=0, columnspan=5, padx=20, pady=20, sticky="nsew")

        save_btn = tk.Button(self, text="Save", command=self.save, bg="#007BFF", fg="white",
                             font=("Segoe UI", 12, "bold"))
        save_btn.grid(row=6, column=0, columnspan=5, pady=(10, 20), padx=10, sticky="s")

    def load(self, info_path):
        self.asset_path = info_path
        if not info_path or not os.path.exists(info_path):
            return

        with open(info_path, "r") as f:
            data = json.load(f)

        self._suppress_save = True

        self.name_var.set(data.get("asset_name", ""))
        types = data.get("types", ["Player", "Object", "Background", "Enemy"])
        self.type_var.set(data.get("asset_type", types[0]))
        self.duplicatable_var.set(data.get("duplicatable", False))

        self.dup_range.set(
            max(30, data.get("duplication_interval_min", 30)),
            max(30, data.get("duplication_interval_max", 60))
        )
        self.depth_range.set(
            max(0, data.get("min_child_depth", 0)),
            max(0, data.get("max_child_depth", 2))
        )

        menu = self.type_menu["menu"]
        menu.delete(0, "end")
        for t in types:
            menu.add_command(label=t, command=lambda v=t: self.type_var.set(v))

        anim_data = data.get("animations", {}).get("default", {})
        asset_folder = os.path.dirname(info_path)
        self.anim_editor.load("default", anim_data, asset_folder)

        self._suppress_save = False

    def save(self):
        if not self.asset_path:
            return

        with open(self.asset_path, "r") as f:
            data = json.load(f)

        old_name = os.path.basename(os.path.dirname(self.asset_path))
        new_name = self.name_var.get().strip()
        asset_root = os.path.dirname(os.path.dirname(self.asset_path))
        old_folder = os.path.join(asset_root, old_name)
        new_folder = os.path.join(asset_root, new_name)

        if new_name and new_name != old_name:
            if os.path.exists(new_folder):
                return
            os.rename(old_folder, new_folder)
            self.asset_path = os.path.join(new_folder, "info.json")
            if self.on_rename:
                self.on_rename(old_name, new_name)

        data["asset_name"] = new_name
        data["asset_type"] = self.type_var.get()
        data["duplicatable"] = self.duplicatable_var.get()
        data["duplication_interval_min"], data["duplication_interval_max"] = self.dup_range.get()
        data["min_child_depth"], data["max_child_depth"] = self.depth_range.get()

        data["animations"] = data.get("animations", {})
        data["animations"]["default"] = self.anim_editor.save()
        data["available_animations"] = list(data["animations"].keys())

        with open(self.asset_path, "w") as f:
            json.dump(data, f, indent=4)