import os
import json
import tkinter as tk
from tkinter import ttk, messagebox
from pages.button import BlueButton
from pages.range import Range
from pages.area_ui import AreaUI


class SpacingThresholdPage(ttk.Frame):
    def __init__(self, parent):
        super().__init__(parent)

        FONT = ('Segoe UI', 14)
        FONT_BOLD = ('Segoe UI', 18, 'bold')
        MAIN_COLOR = "#005f73"
        SECONDARY_COLOR = "#ee9b00"

        self.asset_path = None
        self.area_file = "spacing_area.json"
        self.has_spacing_var = tk.BooleanVar(value=True)

        style = ttk.Style(self)
        style.configure('Main.TButton', font=FONT, padding=6,
                        background=MAIN_COLOR, foreground='black')
        style.map('Main.TButton', background=[('active', SECONDARY_COLOR)])
        style.configure('Large.TLabel', font=FONT)
        style.configure('LargeBold.TLabel', font=FONT_BOLD,
                        foreground=SECONDARY_COLOR)

        for c in range(3):
            self.columnconfigure(c, weight=1)

        ttk.Label(self, text="Spacing Threshold", style='LargeBold.TLabel')\
            .grid(row=0, column=0, columnspan=3, pady=(10, 20))

        # Has spacing checkbox
        ttk.Checkbutton(self, text="Has Spacing", variable=self.has_spacing_var,
                        command=self._on_toggle, style='Large.TCheckbutton')\
            .grid(row=1, column=0, columnspan=3, sticky="w", padx=12, pady=6)

        ttk.Label(self, text="Forbidden Spawn Region:", style='Large.TLabel')\
            .grid(row=2, column=0, sticky='e', padx=12, pady=6)

        self.area_ui = AreaUI(self, self.area_file, "Spacing Area")
        self.area_ui.grid(row=3, column=0, columnspan=3, padx=12, pady=6)

        ttk.Label(self, text="Min Distance From Same Type:", style='Large.TLabel')\
            .grid(row=4, column=0, sticky='e', padx=12, pady=6)

        self.min_dist_range = Range(self, min_bound=0, max_bound=2000,
                                    set_min=0, set_max=0, force_fixed=True)
        self.min_dist_range.grid(row=4, column=1, columnspan=2,
                                 sticky='we', padx=12, pady=6)

        BlueButton(self, "Save", command=self.save, x=0, y=0)

    def _on_toggle(self):
        has_spacing = self.has_spacing_var.get()
        state = "normal" if has_spacing else "disabled"
        self.area_ui.btn_configure.config(state=state)
        self.area_ui.area_label.config(
            text="(none)" if not has_spacing else (
                "(configured)" if self.area_ui.area_data else "(none)"
            )
        )

        # ✅ Fixed here: use existing Range methods
        if has_spacing:
            self.min_dist_range.enable()
        else:
            self.min_dist_range.disable()

        if has_spacing and self.area_ui.area_data:
            self.area_ui._draw_preview()
        else:
            self.area_ui.preview_canvas.delete("all")

    def load(self, info_path):
        self.asset_path = info_path
        if not info_path:
            return

        asset_dir = os.path.dirname(info_path)

        try:
            with open(info_path, 'r') as f:
                data = json.load(f)
        except Exception as e:
            messagebox.showerror("Error", f"Could not load info.json:\n{e}")
            return

        has_spacing = data.get("has_spacing", True)
        self.has_spacing_var.set(has_spacing)

        spacing_area_file = data.get("spacing_area", self.area_file)
        spacing_json_path = os.path.join(asset_dir, spacing_area_file)
        self.area_ui.json_path = spacing_json_path

        frames_path = os.path.join(asset_dir, "default")
        if os.path.isdir(frames_path):
            self.area_ui.frames_source = frames_path

        self.area_ui._load_area_json()
        self.area_ui._draw_preview()

        dist = data.get("min_same_type_distance", 0)
        self.min_dist_range.set(dist, dist)

        self._on_toggle()

    def save(self):
        if not self.asset_path:
            messagebox.showerror("Error", "No asset selected.")
            return

        has_spacing = self.has_spacing_var.get()

        if has_spacing and not self.area_ui.area_data:
            messagebox.showerror("Error", "Draw a forbidden region before saving.")
            return

        asset_dir = os.path.dirname(self.asset_path)
        spacing_json_path = os.path.join(asset_dir, self.area_file)

        with open(self.asset_path, 'r') as f:
            info = json.load(f)

        info["has_spacing"] = has_spacing
        if has_spacing:
            with open(spacing_json_path, 'w') as f:
                json.dump(self.area_ui.area_data, f, indent=2)

            info["spacing_area"] = self.area_file
            min_val, max_val = self.min_dist_range.get()
            info["min_same_type_distance"] = min_val if min_val == max_val else [min_val, max_val]
        else:
            info["spacing_area"] = None
            info["min_same_type_distance"] = 0

        with open(self.asset_path, 'w') as f:
            json.dump(info, f, indent=2)

        messagebox.showinfo("Saved", "Spacing threshold saved.")
