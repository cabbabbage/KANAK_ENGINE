# === File: pages/lighting.py ===
import os
import json
import tkinter as tk
from tkinter import ttk, messagebox, colorchooser
from pages.range import Range
from pages.button import BlueButton

class LightingPage(ttk.Frame):
    def __init__(self, parent):
        super().__init__(parent)
        self.asset_path = None
        self.light_color = (255, 255, 255)

        self.FONT = ('Segoe UI', 14)
        self.BOLD = ('Segoe UI', 18, 'bold')
        self.COLOR_PRIMARY = "#ca6702"

        ttk.Style(self).configure('Lighting.TCheckbutton', font=self.FONT)
        ttk.Label(self, text="Lighting Settings", font=self.BOLD, foreground=self.COLOR_PRIMARY)\
            .pack(anchor='w', padx=12, pady=(10, 14))

        self.has_light_var = tk.BooleanVar()
        ttk.Checkbutton(self, text="Has Light Source", variable=self.has_light_var,
                        command=self._toggle_light_settings,
                        style='Lighting.TCheckbutton').pack(anchor='w', padx=20, pady=4)

        # Sub-widgets container
        self.options_frame = ttk.Frame(self)
        self.options_frame.pack(fill=tk.X, padx=12, pady=8)

        self.intensity = Range(self.options_frame, min_bound=0, max_bound=100, label="Light Intensity")
        self.intensity.pack(fill=tk.X, pady=6)

        self.radius = Range(self.options_frame, min_bound=10, max_bound=2000, label="Radius (px)")
        self.radius.pack(fill=tk.X, pady=6)

        self.falloff = Range(self.options_frame, min_bound=0, max_bound=100, label="Falloff (%)")
        self.falloff.pack(fill=tk.X, pady=6)

        self.jitter_min = Range(self.options_frame, min_bound=0, max_bound=20, label="Jitter Min (px)")
        self.jitter_min.set_fixed()
        self.jitter_min.pack(fill=tk.X, pady=6)

        self.jitter_max = Range(self.options_frame, min_bound=0, max_bound=20, label="Jitter Max (px)")
        self.jitter_max.set_fixed()
        self.jitter_max.pack(fill=tk.X, pady=6)

        self.flicker_var = tk.BooleanVar()
        self.flicker_cb = ttk.Checkbutton(self.options_frame, text="Flicker", variable=self.flicker_var,
                                          style='Lighting.TCheckbutton')
        self.flicker_cb.pack(anchor='w', pady=6)

        self.color_preview = tk.Label(self.options_frame, text="Pick Light Color", bg="#ffffff",
                                      width=20, height=2, relief='raised')
        self.color_preview.pack(pady=10)
        self.color_preview.bind("<Button-1>", self._choose_color)

        BlueButton(self, "Save", command=self.save, x=0, y=0)

    def _choose_color(self, event=None):
        rgb, hex_color = colorchooser.askcolor(initialcolor=self.color_preview['bg'])
        if rgb:
            self.light_color = tuple(map(int, rgb))
            self.color_preview.config(bg=hex_color)

    def _toggle_light_settings(self):
        state = 'normal' if self.has_light_var.get() else 'disabled'
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
        self.color_preview.configure(state=state)

    def save(self):
        if not self.asset_path:
            messagebox.showerror("Error", "No asset selected.")
            return

        with open(self.asset_path, 'r') as f:
            data = json.load(f)

        if self.has_light_var.get():
            data["lighting_info"] = {
                "has_light_source": True,
                "light_intensity": self.intensity.get_min(),
                "light_color": list(self.light_color),
                "radius": self.radius.get_min(),
                "fall_off": self.falloff.get_min(),
                "jitter_min": self.jitter_min.get_min(),
                "jitter_max": self.jitter_max.get_min(),
                "flicker": self.flicker_var.get()
            }

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

        else:
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

        with open(self.asset_path, 'w') as f:
            json.dump(data, f, indent=4)

        messagebox.showinfo("Saved", "Lighting settings saved. Shading disabled.")


    def load(self, path):
        self.asset_path = path
        if not os.path.isfile(path):
            return

        with open(path, 'r') as f:
            data = json.load(f)

        lighting = data.get("lighting_info", {})

        self.has_light_var.set(lighting.get("has_light_source", False))
        self.intensity.set(lighting.get("light_intensity", 100), lighting.get("light_intensity", 100))
        self.radius.set(lighting.get("radius", 0), lighting.get("radius", 0))
        self.falloff.set(lighting.get("fall_off", 100), lighting.get("fall_off", 100))
        self.jitter_min.set(lighting.get("jitter_min", 0), lighting.get("jitter_min", 0))
        self.jitter_max.set(lighting.get("jitter_max", 0), lighting.get("jitter_max", 0))
        self.flicker_var.set(lighting.get("flicker", False))

        if "light_color" in lighting:
            r, g, b = lighting["light_color"]
            self.light_color = (r, g, b)
            hex_color = "#%02x%02x%02x" % (r, g, b)
            self.color_preview.config(bg=hex_color)

        self._toggle_light_settings()

