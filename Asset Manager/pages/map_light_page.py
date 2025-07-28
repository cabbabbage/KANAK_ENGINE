# === File: pages/map_light_page.py ===
import os
import json
import tkinter as tk
from tkinter import ttk, messagebox, colorchooser
from pages.key_color import ColorKeyCircleEditor
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

class MapLightPage(tk.Frame):
    def __init__(self, parent):
        super().__init__(parent, bg='#1e1e1e')
        self.json_path = None
        self.data = DEFAULTS.copy()
        self._suspend_save = False

        # Title header
        title = tk.Label(
            self, text="Map Lighting", 
            font=("Segoe UI",20,"bold"), fg="#005f73", bg='#1e1e1e'
        )
        title.pack(fill=tk.X, pady=(10,20))

        # Scrollable content
        canvas = tk.Canvas(self, bg='#2a2a2a', highlightthickness=0)
        scroll_frame = tk.Frame(canvas, bg='#2a2a2a')
        window_id = canvas.create_window((0,0), window=scroll_frame, anchor='nw')
        scroll_frame.bind('<Configure>', lambda e: canvas.configure(scrollregion=canvas.bbox('all')))
        canvas.bind('<Configure>', lambda e: canvas.itemconfig(window_id, width=e.width))
        scroll_frame.bind('<Enter>', lambda e: canvas.bind_all('<MouseWheel>', lambda ev: canvas.yview_scroll(int(-1*(ev.delta/120)), 'units')))
        scroll_frame.bind('<Leave>', lambda e: canvas.unbind_all('<MouseWheel>'))
        canvas.pack(fill=tk.BOTH, expand=True)

        # Range widgets
        self.min_opacity_range = Range(
            scroll_frame, min_bound=0, max_bound=255,
            set_min=self.data["min_opacity"], set_max=self.data["min_opacity"],
            force_fixed=True, label="Min Opacity"
        )
        self.min_opacity_range.pack(fill="x", padx=12, pady=4)
        self.min_opacity_range.var_max.trace_add("write", lambda *_: self._on_change("min_opacity", self.min_opacity_range.get_max()))

        self.max_opacity_range = Range(
            scroll_frame, min_bound=0, max_bound=255,
            set_min=self.data["max_opacity"], set_max=self.data["max_opacity"],
            force_fixed=True, label="Max Opacity"
        )
        self.max_opacity_range.pack(fill="x", padx=12, pady=4)
        self.max_opacity_range.var_max.trace_add("write", lambda *_: self._on_change("max_opacity", self.max_opacity_range.get_max()))

        self.radius_range = Range(
            scroll_frame, min_bound=0, max_bound=9999,
            set_min=self.data["radius"], set_max=self.data["radius"],
            force_fixed=True, label="Radius"
        )
        self.radius_range.pack(fill="x", padx=12, pady=4)
        self.radius_range.var_max.trace_add("write", lambda *_: self._on_change("radius", self.radius_range.get_max()))

        self.intensity_range = Range(
            scroll_frame, min_bound=0, max_bound=255,
            set_min=self.data["intensity"], set_max=self.data["intensity"],
            force_fixed=True, label="Intensity"
        )
        self.intensity_range.pack(fill="x", padx=12, pady=4)
        self.intensity_range.var_max.trace_add("write", lambda *_: self._on_change("intensity", self.intensity_range.get_max()))

        self.orbit_radius_range = Range(
            scroll_frame, min_bound=0, max_bound=9999,
            set_min=self.data["orbit_radius"], set_max=self.data["orbit_radius"],
            force_fixed=True, label="Orbit Radius"
        )
        self.orbit_radius_range.pack(fill="x", padx=12, pady=4)
        self.orbit_radius_range.var_max.trace_add("write", lambda *_: self._on_change("orbit_radius", self.orbit_radius_range.get_max()))

        self.update_interval_range = Range(
            scroll_frame, min_bound=1, max_bound=100,
            set_min=self.data["update_interval"], set_max=self.data["update_interval"],
            force_fixed=True, label="Update Interval"
        )
        self.update_interval_range.pack(fill="x", padx=12, pady=4)
        self.update_interval_range.var_max.trace_add("write", lambda *_: self._on_change("update_interval", self.update_interval_range.get_max()))

        # Mult slider
        mult_frame = tk.Frame(scroll_frame, bg='#2a2a2a')
        mult_frame.pack(fill="x", padx=12, pady=4)
        tk.Label(mult_frame, text="Mult", font=("Segoe UI",12), fg='#FFFFFF', bg='#2a2a2a').pack(side="left")
        self.mult_var = tk.DoubleVar(value=self.data.get("mult", DEFAULTS["mult"]))
        mult_scale = tk.Scale(
            mult_frame, from_=0.0, to=1.0, resolution=0.01,
            orient=tk.HORIZONTAL, length=200,
            variable=self.mult_var, bg='#2a2a2a', fg='#FFFFFF', highlightthickness=0
        )
        mult_scale.pack(side="left", padx=6)
        self.mult_var.trace_add("write", lambda *_: self._on_change("mult", round(self.mult_var.get(),2)))

        # Base Color Picker
        color_frame = tk.Frame(scroll_frame, bg='#2a2a2a')
        color_frame.pack(fill="x", padx=12, pady=4)
        tk.Label(color_frame, text="Base Color", font=("Segoe UI",12), fg='#FFFFFF', bg='#2a2a2a').pack(side="left")
        self.base_color_btn = tk.Button(
            color_frame, text="Pick Color", width=12, command=self._pick_base_color,
            bg="#%02x%02x%02x" % tuple(self.data["base_color"][:3]), relief='raised'
        )
        self.base_color_btn.pack(side="left", padx=6)

        # Key Colors
        divider = tk.Label(scroll_frame, text="Key Colors", font=("Segoe UI",13,"bold"), fg='#DDDDDD', bg='#2a2a2a')
        divider.pack(anchor='w', padx=12, pady=(16,4))
        self.color_editor = ColorKeyCircleEditor(
            scroll_frame, on_change=self._on_keys_changed,
            width=300, height=300
        )
        self.color_editor.configure(bg='#2a2a2a')
        self.color_editor.pack(pady=4)

    def _pick_base_color(self):
        hex_color = "#%02x%02x%02x" % tuple(self.data.get("base_color",DEFAULTS["base_color"])[:3])
        rgb = colorchooser.askcolor(color=hex_color)[0]
        if rgb:
            self.data["base_color"] = [int(rgb[0]), int(rgb[1]), int(rgb[2]), 255]
            self.base_color_btn.configure(bg="#%02x%02x%02x" % tuple(self.data["base_color"][:3]))
            self._save_json()

    def load_data(self, data=None, json_path=None):
        if not json_path:
            return
        self.json_path = json_path
        self.data = DEFAULTS.copy()
        if isinstance(data, dict):
            self.data.update(data)
        self._suspend_save = True
        # Update widgets
        self.min_opacity_range.set(self.data["min_opacity"], self.data["min_opacity"])
        self.max_opacity_range.set(self.data["max_opacity"], self.data["max_opacity"])
        self.radius_range.set(self.data["radius"], self.data["radius"])
        self.intensity_range.set(self.data["intensity"], self.data["intensity"])
        self.orbit_radius_range.set(self.data["orbit_radius"], self.data["orbit_radius"])
        self.update_interval_range.set(self.data["update_interval"], self.data["update_interval"])
        self.mult_var.set(self.data.get("mult",DEFAULTS["mult"]))
        self.base_color_btn.configure(bg="#%02x%02x%02x" % tuple(self.data["base_color"][:3]))
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
