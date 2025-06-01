import os
import json
import shutil
import tkinter as tk
from tkinter import ttk, messagebox
from pages.AnimationConfig import AnimationEditor

class BasicInfoPage(ttk.Frame):
    def __init__(self, parent, on_rename_callback=None):
        super().__init__(parent)
        self.asset_path = None
        self.on_rename = on_rename_callback

        self.duplicatable_var = tk.BooleanVar(value=False)
        self.name_var = tk.StringVar()
        self.type_var = tk.StringVar()

        self.dup_min_var = tk.IntVar(value=30)
        self.dup_max_var = tk.IntVar(value=60)
        self.min_depth_var = tk.IntVar(value=0)
        self.max_depth_var = tk.IntVar(value=2)

        font_large = ("Segoe UI", 14)
        style = ttk.Style()
        style.configure("Big.TButton", font=("Segoe UI", 14), background="#ffcc00")
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

        # Duplication timing
        ttk.Label(self, text="Duplication Interval (s):", font=font_large).grid(row=3, column=0, sticky="w", padx=16)
        ttk.Label(self, text="Min:", font=font_large).grid(row=3, column=1, sticky="e")
        ttk.Spinbox(self, from_=30, to=9999, textvariable=self.dup_min_var, width=5).grid(row=3, column=2, sticky="w")
        ttk.Label(self, text="Max:", font=font_large).grid(row=3, column=3, sticky="e")
        ttk.Spinbox(self, from_=30, to=9999, textvariable=self.dup_max_var, width=5).grid(row=3, column=4, sticky="w")

        # Child depth limits
        ttk.Label(self, text="Child Depth:", font=font_large).grid(row=4, column=0, sticky="w", padx=16)
        ttk.Label(self, text="Min:", font=font_large).grid(row=4, column=1, sticky="e")
        ttk.Spinbox(self, from_=0, to=100, textvariable=self.min_depth_var, width=5).grid(row=4, column=2, sticky="w")
        ttk.Label(self, text="Max:", font=font_large).grid(row=4, column=3, sticky="e")
        ttk.Spinbox(self, from_=0, to=100, textvariable=self.max_depth_var, width=5).grid(row=4, column=4, sticky="w")

        self.anim_editor = AnimationEditor(self)
        self.anim_editor.grid(row=5, column=0, columnspan=5, padx=20, pady=20, sticky="nsew")

        ttk.Button(self, text="Save", command=self.save, style="Big.TButton")\
            .grid(row=6, column=0, columnspan=5, pady=(20, 10), padx=16, ipadx=10, ipady=6)

    def load(self, info_path):
        self.asset_path = info_path
        if not info_path or not os.path.exists(info_path):
            return
        with open(info_path, "r") as f:
            data = json.load(f)

        self.name_var.set(data.get("asset_name", ""))
        types = data.get("types", ["Player", "Object", "Background", "Enemy"])
        self.type_var.set(data.get("asset_type", types[0]))
        self.duplicatable_var.set(data.get("duplicatable", False))

        self.dup_min_var.set(max(30, data.get("duplication_interval_min", 30)))
        self.dup_max_var.set(max(self.dup_min_var.get(), data.get("duplication_interval_max", 60)))
        self.min_depth_var.set(max(0, data.get("min_child_depth", 0)))
        self.max_depth_var.set(max(self.min_depth_var.get(), data.get("max_child_depth", 2)))

        menu = self.type_menu["menu"]
        menu.delete(0, "end")
        for t in types:
            menu.add_command(label=t, command=lambda v=t: self.type_var.set(v))

        anim_data = data.get("animations", {}).get("default", {})
        asset_folder = os.path.dirname(info_path)
        self.anim_editor.load("default", anim_data, asset_folder)

    def save(self):
        if not self.asset_path:
            messagebox.showerror("Error", "No asset selected.")
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
                messagebox.showerror("Error", f"Folder '{new_name}' already exists.")
                return
            os.rename(old_folder, new_folder)
            self.asset_path = os.path.join(new_folder, "info.json")
            if self.on_rename:
                self.on_rename(old_name, new_name)

        data["asset_name"] = new_name
        data["asset_type"] = self.type_var.get()
        data["duplicatable"] = self.duplicatable_var.get()

        data["duplication_interval_min"] = max(30, abs(self.dup_min_var.get()))
        data["duplication_interval_max"] = max(data["duplication_interval_min"], abs(self.dup_max_var.get()))
        data["min_child_depth"] = max(0, abs(self.min_depth_var.get()))
        data["max_child_depth"] = max(data["min_child_depth"], abs(self.max_depth_var.get()))

        data["animations"] = data.get("animations", {})
        data["animations"]["default"] = self.anim_editor.save()
        data["available_animations"] = list(data["animations"].keys())

        with open(self.asset_path, "w") as f:
            json.dump(data, f, indent=4)

        messagebox.showinfo("Saved", "Basic info saved.")
