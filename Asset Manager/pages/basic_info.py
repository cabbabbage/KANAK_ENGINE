import os
import json
import shutil
import tkinter as tk
from tkinter import ttk, messagebox, simpledialog
from PIL import Image, ImageTk

ASSET_DIR = "SRC"
HARD_CODED_TYPES = ["Player", "Object", "Background", "Enemy", "Map Boundary", "MAP"]


class BasicInfoPage(ttk.Frame):
    def __init__(self, parent, on_rename_callback=None):
        super().__init__(parent)
        self.asset_path = None
        self.on_rename = on_rename_callback

        # Variables
        self.name_var = tk.StringVar()
        self.type_var = tk.StringVar()

        # Styles
        font_large = ("Segoe UI", 14)
        style = ttk.Style(self)
        style.configure("Big.TButton", font=("Segoe UI", 14))
        style.configure("Big.TEntry", font=font_large)
        style.configure("Big.TMenubutton", font=font_large)

        # Asset Name
        ttk.Label(self, text="Asset Name:", font=font_large)\
            .grid(row=0, column=0, sticky="w", padx=16, pady=10)
        self.name_display = ttk.Label(self, textvariable=self.name_var, font=font_large)
        self.name_display.grid(row=0, column=1, columnspan=2, sticky="w", padx=16, pady=10)

        # Asset Type
        ttk.Label(self, text="Asset Type:", font=font_large)\
            .grid(row=1, column=0, sticky="w", padx=16, pady=10)
        self.type_menu = ttk.OptionMenu(self, self.type_var, HARD_CODED_TYPES[0], *HARD_CODED_TYPES)
        self.type_menu.config(style="Big.TMenubutton", width=30)
        self.type_menu.grid(row=1, column=1, columnspan=2, sticky="w", padx=16, pady=10)

        # Preview Image
        self.preview_label = ttk.Label(self)
        self.preview_label.grid(row=2, column=0, columnspan=3, pady=10)

        # Buttons
        btn_frame = ttk.Frame(self)
        btn_frame.grid(row=3, column=0, columnspan=3, pady=(10, 20))
        tk.Button(btn_frame, text="Save", command=self.save,
                  bg="#007BFF", fg="white", font=("Segoe UI", 12, "bold"))\
            .pack(side=tk.LEFT, padx=(10, 5))
        tk.Button(btn_frame, text="Delete Asset", command=self._delete_asset,
                  bg="#D9534F", fg="white", font=("Segoe UI", 12, "bold"))\
            .pack(side=tk.LEFT, padx=(5, 10))
        tk.Button(btn_frame, text="Create Duplicate", command=self._duplicate_asset,
                  bg="#28a745", fg="white", font=("Segoe UI", 12, "bold"))\
            .pack(side=tk.LEFT, padx=(5, 10))

    def load(self, info_path):
        """Load info.json and refresh UI."""
        self.asset_path = info_path
        if not info_path or not os.path.exists(info_path):
            return

        asset_dir = os.path.dirname(info_path)
        # Load JSON
        try:
            with open(info_path, "r") as f:
                data = json.load(f)
        except Exception as e:
            messagebox.showerror("Error", f"Failed to load JSON:\n{e}")
            return

        # Update fields
        self.name_var.set(data.get("asset_name", ""))
        self.type_var.set(data.get("asset_type", HARD_CODED_TYPES[0]))

        # Load preview image if it exists
        preview_path = os.path.join(asset_dir, "default", "0.png")
        if os.path.exists(preview_path):
            try:
                img = Image.open(preview_path)
                img.thumbnail((200, 200))
                self._preview_photo = ImageTk.PhotoImage(img)
                self.preview_label.config(image=self._preview_photo, text="")
            except:
                self.preview_label.config(text="Preview failed", image="")
        else:
            self.preview_label.config(text="No preview available", image="")

    def save(self):
        """Save only name and type back into info.json, preserving other keys."""
        if not self.asset_path:
            messagebox.showerror("Error", "No asset selected.")
            return

        # Load existing data
        try:
            with open(self.asset_path, "r") as f:
                data = json.load(f)
        except Exception as e:
            messagebox.showerror("Error", f"Failed to load JSON for saving:\n{e}")
            return

        # Update JSON
        data["asset_name"] = self.name_var.get().strip()
        data["asset_type"] = self.type_var.get()

        # Write back
        try:
            with open(self.asset_path, "w") as f:
                json.dump(data, f, indent=4)
            messagebox.showinfo("Saved", f"Asset '{data['asset_name']}' saved.")
        except Exception as e:
            messagebox.showerror("Error", f"Failed to save:\n{e}")

    def _delete_asset(self):
        if not self.asset_path:
            return
        asset_folder = os.path.dirname(self.asset_path)
        name = os.path.basename(asset_folder)
        if not messagebox.askyesno("Confirm Delete", f"Delete '{name}'?"):
            return
        try:
            shutil.rmtree(asset_folder)
            messagebox.showinfo("Deleted", f"Asset '{name}' deleted.")
            # Immediately refresh the asset list in the main app
            root = self.winfo_toplevel()
            if hasattr(root, '_refresh_asset_list'):
                root._refresh_asset_list()
            # Clear selection highlight
            if hasattr(root, 'asset_buttons'):
                for btn in root.asset_buttons.values():
                    btn.configure(bg="#f0f0f0")
            return
        except Exception as e:
            messagebox.showerror("Error", f"Could not delete asset:{e}")("Error", f"Could not delete asset:\n{e}")

    def _duplicate_asset(self):
        if not self.asset_path:
            return
        src_folder = os.path.dirname(self.asset_path)
        base = os.path.basename(src_folder)
        new_name = simpledialog.askstring(
            "Duplicate Asset",
            f"Name for copy of '{base}':",
            initialvalue=f"{base}_copy"
        )
        if not new_name or not new_name.strip():
            return
        dst = os.path.join(os.path.dirname(src_folder), new_name.strip())
        if os.path.exists(dst):
            messagebox.showerror("Error", f"'{new_name}' already exists.")
            return
        try:
            shutil.copytree(src_folder, dst)
            # Fix up JSON inside the new folder
            info = os.path.join(dst, "info.json")
            if os.path.exists(info):
                with open(info, "r+") as f:
                    data = json.load(f)
                    data["asset_name"] = new_name.strip()
                    f.seek(0); f.truncate()
                    json.dump(data, f, indent=4)
            messagebox.showinfo("Duplicated", f"Created '{new_name}'.")
            # Immediately refresh & select in main app
            root = self.winfo_toplevel()
            if hasattr(root, '_refresh_asset_list'):
                root._refresh_asset_list()
            if hasattr(root, '_select_asset_by_name'):
                root._select_asset_by_name(new_name.strip())
            return
        except Exception as e:
            messagebox.showerror("Error", f"Duplicate failed:{e}")
