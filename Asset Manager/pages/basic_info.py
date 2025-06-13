import os
import json
import shutil
import tkinter as tk
from tkinter import ttk, messagebox, simpledialog
from pages.range import Range

ASSET_DIR = "SRC"
HARD_CODED_TYPES = ["Player", "Object", "Background", "Enemy", "Map Boundary"]

class BasicInfoPage(ttk.Frame):
    def __init__(self, parent, on_rename_callback=None):
        super().__init__(parent)
        self.asset_path = None
        self.on_rename = on_rename_callback

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
        self.name_display = ttk.Label(self, textvariable=self.name_var, font=font_large)
        self.name_display.grid(row=0, column=1, columnspan=2, sticky="w", padx=16, pady=10)

        ttk.Label(self, text="Asset Type:", font=font_large).grid(row=1, column=0, sticky="w", padx=16, pady=10)
        self.type_menu = ttk.OptionMenu(self, self.type_var, HARD_CODED_TYPES[0], *HARD_CODED_TYPES)
        self.type_menu.config(style="Big.TMenubutton", width=30)
        self.type_menu.grid(row=1, column=1, columnspan=2, sticky="w", padx=16, pady=10)

        ttk.Checkbutton(self, text="Duplicatable Asset", variable=self.duplicatable_var, style="Big.TCheckbutton").grid(
            row=2, column=0, columnspan=3, sticky="w", padx=16, pady=6
        )

        self.dup_range = Range(self, min_bound=30, max_bound=9999, set_min=30, set_max=60, label="Duplication Interval (s)")
        self.dup_range.grid(row=3, column=0, columnspan=5, sticky="ew", padx=16)

        self.depth_range = Range(self, min_bound=0, max_bound=100, set_min=0, set_max=2, label="Child Depth")
        self.depth_range.grid(row=4, column=0, columnspan=5, sticky="ew", padx=16)

        btn_frame = ttk.Frame(self)
        btn_frame.grid(row=5, column=0, columnspan=5, pady=(10, 20))

        tk.Button(btn_frame, text="Save", command=self.save, bg="#007BFF", fg="white", font=("Segoe UI", 12, "bold")).pack(side=tk.LEFT, padx=(10, 5))
        tk.Button(btn_frame, text="Delete Asset", command=self._delete_asset, bg="#D9534F", fg="white", font=("Segoe UI", 12, "bold")).pack(side=tk.LEFT, padx=(5, 10))
        tk.Button(btn_frame, text="Create Duplicate", command=self._duplicate_asset, bg="#28a745", fg="white", font=("Segoe UI", 12, "bold")).pack(side=tk.LEFT, padx=(5, 10))

    def load(self, info_path):
        self.asset_path = info_path
        if not info_path or not os.path.exists(info_path):
            return

        with open(info_path, "r") as f:
            data = json.load(f)

        self.name_var.set(data.get("asset_name", ""))
        self.type_var.set(data.get("asset_type", HARD_CODED_TYPES[0]))
        self.duplicatable_var.set(data.get("duplicatable", False))

        self.dup_range.set(
            max(30, data.get("duplication_interval_min", 30)),
            max(30, data.get("duplication_interval_max", 60))
        )
        self.depth_range.set(
            max(0, data.get("min_child_depth", 0)),
            max(0, data.get("max_child_depth", 2))
        )

    def save(self):
        if not self.asset_path:
            messagebox.showerror("Error", "No asset path defined.")
            return

        asset_name = self.name_var.get().strip()
        asset_folder = os.path.dirname(self.asset_path)

        data = {
            "asset_name": asset_name,
            "asset_type": self.type_var.get(),
            "duplicatable": self.duplicatable_var.get(),
            "duplication_interval_min": self.dup_range.get()[0],
            "duplication_interval_max": self.dup_range.get()[1],
            "min_child_depth": self.depth_range.get()[0],
            "max_child_depth": self.depth_range.get()[1]
        }

        try:
            with open(os.path.join(asset_folder, "info.json"), "w") as f:
                json.dump(data, f, indent=4)
            print(f"[BasicInfoPage] Saved: {asset_folder}")
            messagebox.showinfo("Saved", f"Asset '{asset_name}' saved successfully.")
        except Exception as e:
            messagebox.showerror("Save Error", str(e))

    def _delete_asset(self):
        if not self.asset_path:
            return

        asset_folder = os.path.dirname(self.asset_path)
        asset_name = os.path.basename(asset_folder)
        confirm = messagebox.askyesno("Confirm Delete", f"Are you sure you want to delete asset '{asset_name}'?")
        if not confirm:
            return

        try:
            shutil.rmtree(asset_folder)
            self.asset_path = None
            self.name_var.set("")
            self.type_var.set("")
            self.duplicatable_var.set(False)
            self.dup_range.set(30, 60)
            self.depth_range.set(0, 2)
            if self.on_rename:
                self.on_rename(asset_name, None)
        except Exception as e:
            messagebox.showerror("Error", f"Failed to delete asset: {e}")

    def _duplicate_asset(self):
        if not self.asset_path:
            return

        base_name = os.path.basename(os.path.dirname(self.asset_path))
        parent_dir = os.path.dirname(os.path.dirname(self.asset_path))

        new_name = simpledialog.askstring(
            "Duplicate Asset",
            f"Enter name for duplicate of '{base_name}':\n(This cannot be undone)",
            initialvalue=f"{base_name}_copy"
        )

        if not new_name or not new_name.strip():
            return

        dst_folder = os.path.join(parent_dir, new_name.strip())
        if os.path.exists(dst_folder):
            messagebox.showerror("Error", f"Folder '{new_name}' already exists.")
            return

        try:
            shutil.copytree(os.path.dirname(self.asset_path), dst_folder)
            info_path = os.path.join(dst_folder, "info.json")
            if os.path.exists(info_path):
                with open(info_path, "r") as f:
                    data = json.load(f)
                data["asset_name"] = new_name
                with open(info_path, "w") as f:
                    json.dump(data, f, indent=4)
            messagebox.showinfo("Duplicate Created", f"Created: {new_name}")
        except Exception as e:
            messagebox.showerror("Error", f"Failed to duplicate asset: {e}")
