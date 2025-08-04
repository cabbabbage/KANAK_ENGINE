import os
import json
import tkinter as tk
from tkinter import ttk, messagebox, colorchooser
from pages.range import Range

class LightingControls(ttk.Frame):
    def __init__(self, parent, data=None, show_orbit=True):
        super().__init__(parent, style="LS.TFrame")
        self.light_color = (255, 255, 255)
        self.flicker_var = tk.BooleanVar(value=False)
        self.show_orbit = show_orbit

        self.intensity = Range(self, min_bound=0, max_bound=255, label="Light Intensity")
        self.radius = Range(self, min_bound=0, max_bound=2000, label="Radius (px)")
        self.orbit_radius = Range(self, min_bound=0, max_bound=2000, label="Orbit Radius (px)")
        self.falloff = Range(self, min_bound=0, max_bound=100, label="Falloff (%)")
        self.jitter_min = Range(self, min_bound=0, max_bound=20, label="Jitter Max (px)")
        self.flare = Range(self, min_bound=0, max_bound=100, label="Flare (px)")
        self.offset_x = Range(self, min_bound=-2000, max_bound=2000, label="Offset X")
        self.offset_y = Range(self, min_bound=-2000, max_bound=2000, label="Offset Y")

        for rng in (self.jitter_min, self.flare, self.offset_x, self.offset_y):
            rng.set_fixed()

        for rng in (
            self.intensity, self.radius, self.falloff,
            self.jitter_min, self.flare, self.offset_x, self.offset_y
        ):
            rng.pack(fill=tk.X, padx=10, pady=4)

        if show_orbit:
            self.orbit_radius.pack(fill=tk.X, padx=10, pady=4)

        ttk.Checkbutton(self, text="Flicker", variable=self.flicker_var, style='TCheckbutton')\
            .pack(anchor='w', padx=10, pady=4)

        self.color_preview = tk.Label(
            self, text="Pick Light Color", bg="#FFFFFF",
            font=("Segoe UI", 12), width=20, height=2, relief='raised'
        )
        self.color_preview.pack(padx=10, pady=4)
        self.color_preview.bind("<Button-1>", self._choose_color)

        if data is None:
            self.intensity.set(100, 100)
            self.radius.set(300, 300)
            self.orbit_radius.set(0, 0)
            self.falloff.set(80, 80)
            self.jitter_min.set(0, 0)
            self.flare.set(0, 0)
            self.offset_x.set(0, 0)
            self.offset_y.set(0, 0)
        else:
            self.load_data(data)

    def _choose_color(self, _=None):
        rgb, hex_color = colorchooser.askcolor(initialcolor=self.color_preview['bg'])
        if rgb:
            self.light_color = tuple(map(int, rgb))
            self.color_preview.config(bg=hex_color)

    def load_data(self, data):
        self.intensity.set(data.get("light_intensity", 100), data.get("light_intensity", 100))
        self.radius.set(data.get("radius", 100), data.get("radius", 100))
        self.orbit_radius.set(data.get("orbit_radius", 0), data.get("orbit_radius", 0))
        self.falloff.set(data.get("fall_off", 100), data.get("fall_off", 100))
        self.jitter_min.set(data.get("jitter_min", 0), data.get("jitter_min", 0))
        self.flare.set(data.get("flare", 0), data.get("flare", 0))
        self.offset_x.set(data.get("offset_x", 0), data.get("offset_x", 0))
        self.offset_y.set(data.get("offset_y", 0), data.get("offset_y", 0))
        self.flicker_var.set(data.get("flicker", False))
        if isinstance(data.get("light_color"), list):
            r, g, b = data["light_color"]
            self.light_color = (r, g, b)
            self.color_preview.config(bg="#%02x%02x%02x" % (r, g, b))

    def get_data(self):
        return {
            "has_light_source": True,
            "light_intensity": self.intensity.get_min(),
            "light_color": list(self.light_color),
            "radius": self.radius.get_min(),
            "orbit_radius": self.orbit_radius.get_min() if self.show_orbit else 0,
            "fall_off": self.falloff.get_min(),
            "jitter_min": self.jitter_min.get_min(),
            "flare": self.flare.get_max(),
            "offset_x": self.offset_x.get_min(),
            "offset_y": self.offset_y.get_min(),
            "flicker": self.flicker_var.get()
        }


class LightSourceFrame(ttk.Frame):
    def __init__(self, parent, index, data=None, on_delete=None):
        super().__init__(parent, style="LS.TFrame")
        self.index = index
        self._on_delete = on_delete

        tk.Button(
            self, text="X", bg="#D9534F", fg="white",
            font=("Segoe UI", 12, "bold"), width=3, padx=4,
            command=self._delete_self
        ).pack(anchor="ne", padx=4, pady=4)

        ttk.Label(
            self, text=f"Light Source {index + 1}",
            font=("Segoe UI", 13, "bold"), foreground="#DDDDDD", background="#2a2a2a"
        ).pack(anchor="w", padx=10, pady=(4, 10))

        self.control = LightingControls(self, data=data, show_orbit=False)
        self.control.pack(fill=tk.X, expand=True)

    def _delete_self(self):
        if self._on_delete:
            self._on_delete(self.index)

    def get_data(self):
        return self.control.get_data()

    def load_data(self, data):
        self.control.load_data(data)
