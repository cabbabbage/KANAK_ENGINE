import os
import json
import tkinter as tk
from tkinter import ttk, messagebox, colorchooser
from pages.key_color import ColorKeyCircleEditor  # adjust import if needed
from pages.range import Range

DEFAULTS = {
    "min_opacity": 50,
    "max_opacity": 255,
    "radius": 1920,
    "intensity": 255,
    "orbit_radius": 150,
    "update_interval": 2,
    "mult": 0.4,
    "base_color": [255, 255, 255, 255],
    "keys": []
}


class MapLightPage(ttk.Frame):
    def __init__(self, parent):
        super().__init__(parent)
        self.json_path = None
        self.data = DEFAULTS.copy()
        self._suspend_save = False
        self._build_ui()

    def _build_ui(self):
        self.mult_var = tk.DoubleVar()

        container = ttk.Frame(self)
        container.pack(fill="both", expand=True, padx=12, pady=10)

        # Numeric parameters as fixed Range widgets
        self.min_opacity_range = Range(
            container,
            min_bound=0, max_bound=255,
            set_min=self.data["min_opacity"], set_max=self.data["min_opacity"],
            force_fixed=True,
            label="Min Opacity"
        )
        self.min_opacity_range.pack(fill="x", pady=3)
        self.min_opacity_range.var_max.trace_add(
            "write",
            lambda *_: self._on_change("min_opacity", self.min_opacity_range.get_max())
        )

        self.max_opacity_range = Range(
            container,
            min_bound=0, max_bound=255,
            set_min=self.data["max_opacity"], set_max=self.data["max_opacity"],
            force_fixed=True,
            label="Max Opacity"
        )
        self.max_opacity_range.pack(fill="x", pady=3)
        self.max_opacity_range.var_max.trace_add(
            "write",
            lambda *_: self._on_change("max_opacity", self.max_opacity_range.get_max())
        )

        self.radius_range = Range(
            container,
            min_bound=0, max_bound=9999,
            set_min=self.data["radius"], set_max=self.data["radius"],
            force_fixed=True,
            label="Radius"
        )
        self.radius_range.pack(fill="x", pady=3)
        self.radius_range.var_max.trace_add(
            "write",
            lambda *_: self._on_change("radius", self.radius_range.get_max())
        )

        self.intensity_range = Range(
            container,
            min_bound=0, max_bound=255,
            set_min=self.data["intensity"], set_max=self.data["intensity"],
            force_fixed=True,
            label="Intensity"
        )
        self.intensity_range.pack(fill="x", pady=3)
        self.intensity_range.var_max.trace_add(
            "write",
            lambda *_: self._on_change("intensity", self.intensity_range.get_max())
        )

        self.orbit_radius_range = Range(
            container,
            min_bound=0, max_bound=9999,
            set_min=self.data["orbit_radius"], set_max=self.data["orbit_radius"],
            force_fixed=True,
            label="Orbit Radius"
        )
        self.orbit_radius_range.pack(fill="x", pady=3)
        self.orbit_radius_range.var_max.trace_add(
            "write",
            lambda *_: self._on_change("orbit_radius", self.orbit_radius_range.get_max())
        )

        self.update_interval_range = Range(
            container,
            min_bound=1, max_bound=100,
            set_min=self.data["update_interval"], set_max=self.data["update_interval"],
            force_fixed=True,
            label="Update Interval"
        )
        self.update_interval_range.pack(fill="x", pady=3)
        self.update_interval_range.var_max.trace_add(
            "write",
            lambda *_: self._on_change("update_interval", self.update_interval_range.get_max())
        )

        # Mult slider remains as before
        mult_frame = ttk.Frame(container)
        mult_frame.pack(fill="x", pady=3)
        ttk.Label(mult_frame, text="Mult").pack(side="left")
        mult_scale = tk.Scale(
            mult_frame,
            from_=0.0, to=1.0,
            resolution=0.01,
            orient=tk.HORIZONTAL,
            length=160,
            variable=self.mult_var
        )
        mult_scale.pack(side="left", padx=6)
        self.mult_var.trace_add(
            "write",
            lambda *_: self._on_change("mult", round(self.mult_var.get(), 2))
        )

        # Base Color Picker
        color_frame = ttk.Frame(container)
        color_frame.pack(fill="x", pady=3)
        ttk.Label(color_frame, text="Base Color").pack(side="left")
        self.base_color_btn = tk.Button(
            color_frame,
            text="Pick Color",
            width=12,
            command=self._pick_base_color,
            bg="#ffffff",
            relief="raised"
        )
        self.base_color_btn.pack(side="left", padx=6)

        # Key Colors Circle Editor
        divider = ttk.Label(container, text="Key Colors", font=("Segoe UI", 11, "bold"))
        divider.pack(anchor="w", pady=(16, 4))
        self.color_editor = ColorKeyCircleEditor(
            container,
            on_change=self._on_keys_changed,
            width=300,
            height=300
        )
        self.color_editor.pack(pady=(4, 8))

    def _pick_base_color(self):
        initial = tuple(self.data.get("base_color", DEFAULTS["base_color"])[:3])
        hex_color = "#%02x%02x%02x" % initial
        color = colorchooser.askcolor(color=hex_color)[0]
        if color:
            self.data["base_color"] = [int(color[0]), int(color[1]), int(color[2]), 255]
            self._update_base_color_button()
            self._save_json()

    def _update_base_color_button(self):
        c = self.data.get("base_color", DEFAULTS["base_color"])
        self.base_color_btn.configure(bg="#%02x%02x%02x" % tuple(c[:3]))

    def load_data(self, data=None, json_path=None):
        if not json_path:
            return
        self.json_path = json_path
        self.data = DEFAULTS.copy()
        if isinstance(data, dict):
            self.data.update(data)

        self._suspend_save = True

        # Update Range widgets to loaded values
        self.min_opacity_range.set(self.data["min_opacity"], self.data["min_opacity"])
        self.max_opacity_range.set(self.data["max_opacity"], self.data["max_opacity"])
        self.radius_range.set(self.data["radius"], self.data["radius"])
        self.intensity_range.set(self.data["intensity"], self.data["intensity"])
        self.orbit_radius_range.set(self.data["orbit_radius"], self.data["orbit_radius"])
        self.update_interval_range.set(
            self.data["update_interval"], self.data["update_interval"]
        )

        # Mult
        self.mult_var.set(self.data.get("mult", DEFAULTS["mult"]))
        self._update_base_color_button()
        self.color_editor.load_keys(self.data.get("keys", []))

        self._suspend_save = False

    def _on_change(self, key, value):
        if self._suspend_save:
            return
        self.data[key] = value
        self._save_json()

    def _on_keys_changed(self, keys):
        if self._suspend_save:
            return
        self.data["keys"] = keys
        self._save_json()

    def _save_json(self):
        if not self.json_path:
            return
        try:
            with open(self.json_path, "w") as f:
                json.dump(self.data, f, indent=2)
        except Exception as e:
            messagebox.showerror("Save Failed", str(e))

    @staticmethod
    def get_json_filename():
        return "map_light.json"
