# === File: pages/shading.py ===
import os
import json
import tkinter as tk
from tkinter import ttk, messagebox
from pages.range import Range
from pages.button import BlueButton

class ShadingPage(ttk.Frame):
    def __init__(self, parent):
        super().__init__(parent)
        self.asset_path = None

        self.FONT = ('Segoe UI', 14)
        self.BOLD = ('Segoe UI', 18, 'bold')
        self.COLOR_ACCENT = "#005f73"

        ttk.Style(self).configure('Shading.TCheckbutton', font=self.FONT)

        ttk.Label(self, text="Shading Settings", font=self.BOLD, foreground=self.COLOR_ACCENT)\
            .pack(anchor='w', padx=12, pady=(10, 14))

        self.has_shading_var = tk.BooleanVar()
        ttk.Checkbutton(self, text="Has Shading", variable=self.has_shading_var,
                        command=self._toggle_ui_state,
                        style='Shading.TCheckbutton')\
            .pack(anchor='w', padx=20, pady=4)

        self.options_frame = ttk.Frame(self)
        self.options_frame.pack(fill=tk.X, padx=12, pady=8)

        # Base Shadow
        self.base_shadow_var = tk.BooleanVar()
        ttk.Checkbutton(self.options_frame, text="Has Base Shadow", variable=self.base_shadow_var,
                        style='Shading.TCheckbutton')\
            .pack(anchor='w', pady=4)
        self.base_intensity = Range(self.options_frame, min_bound=0, max_bound=100, label="Base Shadow Intensity")
        self.base_intensity.set_fixed()
        self.base_intensity.pack(fill=tk.X, pady=4)

        # Gradient Shadow
        self.gradient_shadow_var = tk.BooleanVar()
        ttk.Checkbutton(self.options_frame, text="Has Gradient Shadow", variable=self.gradient_shadow_var,
                        style='Shading.TCheckbutton')\
            .pack(anchor='w', pady=4)
        self.gradient_count_var = tk.IntVar()
        ttk.Label(self.options_frame, text="Number of Gradient Shadows (1–3)", font=self.FONT)\
            .pack(anchor='w')
        ttk.Spinbox(self.options_frame, from_=1, to=3, textvariable=self.gradient_count_var, width=5)\
            .pack(anchor='w', pady=(0, 6))
        self.gradient_intensity = Range(self.options_frame, min_bound=0, max_bound=100, label="Gradient Shadow Intensity")
        self.gradient_intensity.set_fixed()
        self.gradient_intensity.pack(fill=tk.X, pady=4)

        # Casted Shadow
        self.cast_shadow_var = tk.BooleanVar()
        ttk.Checkbutton(self.options_frame, text="Has Casted Shadow", variable=self.cast_shadow_var,
                        style='Shading.TCheckbutton')\
            .pack(anchor='w', pady=4)
        self.cast_count_var = tk.IntVar()
        ttk.Label(self.options_frame, text="Number of Casted Shadows (1–3)", font=self.FONT)\
            .pack(anchor='w')
        ttk.Spinbox(self.options_frame, from_=1, to=3, textvariable=self.cast_count_var, width=5)\
            .pack(anchor='w', pady=(0, 6))
        self.cast_intensity = Range(self.options_frame, min_bound=0, max_bound=100, label="Cast Shadow Intensity")
        self.cast_intensity.set_fixed()
        self.cast_intensity.pack(fill=tk.X, pady=4)

        BlueButton(self, "Save", command=self.save, x=0, y=0)

    def _toggle_ui_state(self):
        state = 'normal' if self.has_shading_var.get() else 'disabled'
        for child in self.options_frame.winfo_children():
            try:
                child.configure(state=state)
            except:
                pass
            for grand in child.winfo_children():
                try:
                    grand.configure(state=state)
                except:
                    pass

    def load(self, path):
        self.asset_path = path
        if not os.path.isfile(path):
            return

        with open(path, 'r') as f:
            data = json.load(f)

        shading = data.get("shading_info", {})

        self.has_shading_var.set(shading.get("has_shading", False))
        self.base_shadow_var.set(shading.get("has_base_shadow", False))
        self.base_intensity.set(shading.get("base_shadow_intensity", 100),
                                shading.get("base_shadow_intensity", 100))

        self.gradient_shadow_var.set(shading.get("has_gradient_shadow", False))
        self.gradient_count_var.set(shading.get("number_of_gradient_shadows", 1))
        self.gradient_intensity.set(shading.get("gradient_shadow_intensity", 100),
                                    shading.get("gradient_shadow_intensity", 100))

        self.cast_shadow_var.set(shading.get("has_casted_shadows", False))
        self.cast_count_var.set(shading.get("number_of_casted_shadows", 1))
        self.cast_intensity.set(shading.get("cast_shadow_intensity", 100),
                                shading.get("cast_shadow_intensity", 100))

        self._toggle_ui_state()

    def save(self):
        if not self.asset_path:
            messagebox.showerror("Error", "No asset selected.")
            return

        with open(self.asset_path, 'r') as f:
            data = json.load(f)

        if self.has_shading_var.get():
            data["shading_info"] = {
                "has_shading": True,
                "has_base_shadow": self.base_shadow_var.get(),
                "base_shadow_intensity": self.base_intensity.get_min(),
                "has_gradient_shadow": self.gradient_shadow_var.get(),
                "number_of_gradient_shadows": self.gradient_count_var.get(),
                "gradient_shadow_intensity": self.gradient_intensity.get_min(),
                "has_casted_shadows": self.cast_shadow_var.get(),
                "number_of_casted_shadows": self.cast_count_var.get(),
                "cast_shadow_intensity": self.cast_intensity.get_min()
            }

            data["lighting_info"] = {
                "has_light_source": False,
                "light_intensity": None,
                "light_color": None,
                "radius": None,
                "fall_off": None,
                "jitter_min": None,
                "jitter_max": None,
                "flicker": None
            }

        else:
            data["shading_info"] = {
                "has_shading": False,
                "has_base_shadow": None,
                "base_shadow_intensity": None,
                "has_gradient_shadow": None,
                "number_of_gradient_shadows": None,
                "gradient_shadow_intensity": None,
                "has_casted_shadows": None,
                "number_of_casted_shadows": None,
                "cast_shadow_intensity": None
            }

        with open(self.asset_path, 'w') as f:
            json.dump(data, f, indent=4)

        messagebox.showinfo("Saved", "Shading settings saved. Lighting disabled.")
