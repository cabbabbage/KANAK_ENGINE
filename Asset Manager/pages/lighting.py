# === File: pages/lighting.py ===
import os
import json
import tkinter as tk
from tkinter import ttk, messagebox, colorchooser
from pages.range import Range

class LightSourceFrame(ttk.Frame):
    def __init__(self, parent, index, data=None, on_delete=None):
        super().__init__(parent, style="LS.TFrame")
        self.index = index
        self._on_delete = on_delete
        self.light_color = (255, 255, 255)

        self.flicker_var = tk.BooleanVar(value=False)

        del_btn = tk.Button(
            self, text="X", bg="#D9534F", fg="white",
            font=("Segoe UI", 12, "bold"), width=3, padx=4,
            command=self._delete_self
        )
        del_btn.pack(anchor="ne", padx=4, pady=4)

        section_title = ttk.Label(
            self, text=f"Light Source {index + 1}",
            font=("Segoe UI", 13, "bold"),
            foreground="#DDDDDD", background="#2a2a2a"
        )
        section_title.pack(anchor="w", padx=10, pady=(4, 10))

        self.intensity = Range(self, min_bound=0, max_bound=255, label="Light Intensity")
        self.radius = Range(self, min_bound=10, max_bound=2000, label="Radius (px)")
        self.orbit_radius = Range(self, min_bound=0, max_bound=2000, label="Orbit Radius (px)")
        self.falloff = Range(self, min_bound=0, max_bound=100, label="Falloff (%)")
        self.jitter_min = Range(self, min_bound=0, max_bound=20, label="Jitter Min (px)")
        self.jitter_max = Range(self, min_bound=0, max_bound=20, label="Jitter Max (px)")
        self.offset_x = Range(self, min_bound=-2000, max_bound=2000, label="Offset X")
        self.offset_y = Range(self, min_bound=-2000, max_bound=2000, label="Offset Y")

        for rng in (self.jitter_min, self.jitter_max, self.offset_x, self.offset_y):
            rng.set_fixed()

        if data is None:
            self.intensity.set(100, 100)
            self.radius.set(300, 300)
            self.orbit_radius.set(0, 0)
            self.falloff.set(80, 80)
            self.jitter_min.set(0, 0)
            self.jitter_max.set(0, 0)
            self.offset_x.set(0, 0)
            self.offset_y.set(0, 0)

        for rng in (
            self.intensity, self.radius, self.orbit_radius, self.falloff,
            self.jitter_min, self.jitter_max, self.offset_x, self.offset_y
        ):
            rng.pack(fill=tk.X, padx=10, pady=4)


        flick_cb = ttk.Checkbutton(
            self, text="Flicker", variable=self.flicker_var,
            style='TCheckbutton'
        )
        flick_cb.pack(anchor='w', padx=10, pady=4)

        self.color_preview = tk.Label(
            self, text="Pick Light Color", bg="#FFFFFF",
            font=("Segoe UI", 12), width=20, height=2, relief='raised'
        )
        self.color_preview.pack(padx=10, pady=4)
        self.color_preview.bind("<Button-1>", self._choose_color)

        self._bind_autosave()

        if data:
            self.load_data(data)

    def _bind_autosave(self):
        for rng in (
            self.intensity, self.radius, self.orbit_radius, self.falloff,
            self.jitter_min, self.jitter_max, self.offset_x, self.offset_y
        ):
            rng.var_min.trace_add("write", lambda *a: self._autosave())
            rng.var_max.trace_add("write", lambda *a: self._autosave())
        self.flicker_var.trace_add("write", lambda *a: self._autosave())

    def _autosave(self):
        root = self.master
        while root and not hasattr(root, 'save'):
            root = root.master
        if root and getattr(root, '_loaded', False):
            root.save()

    def _choose_color(self, event=None):
        rgb, hex_color = colorchooser.askcolor(initialcolor=self.color_preview['bg'])
        if rgb:
            self.light_color = tuple(map(int, rgb))
            self.color_preview.config(bg=hex_color)
            self._autosave()

    def _delete_self(self):
        if self._on_delete:
            self._on_delete(self.index)

    def load_data(self, data):
        self.intensity.set(data.get("light_intensity", 100), data.get("light_intensity", 100))
        self.radius.set(data.get("radius", 100), data.get("radius", 100))
        self.orbit_radius.set(data.get("orbit_radius", 0), data.get("orbit_radius", 0))
        self.falloff.set(data.get("fall_off", 100), data.get("fall_off", 100))
        self.jitter_min.set(data.get("jitter_min", 0), data.get("jitter_min", 0))
        self.jitter_max.set(data.get("jitter_max", 0), data.get("jitter_max", 0))
        self.offset_x.set(data.get("offset_x", 0), data.get("offset_x", 0))
        self.offset_y.set(data.get("offset_y", 0), data.get("offset_y", 0))
        self.flicker_var.set(data.get("flicker", False))
        if isinstance(data.get("light_color"), list) and len(data["light_color"]) == 3:
            r, g, b = data["light_color"]
            self.light_color = (r, g, b)
            self.color_preview.config(bg="#%02x%02x%02x" % (r, g, b))

    def get_data(self):
        return {
            "has_light_source": True,
            "light_intensity": self.intensity.get_min(),
            "light_color": list(self.light_color),
            "radius": self.radius.get_min(),
            "orbit_radius": self.orbit_radius.get_min(),
            "fall_off": self.falloff.get_min(),
            "jitter_min": self.jitter_min.get_min(),
            "jitter_max": self.jitter_max.get_max(),
            "offset_x": self.offset_x.get_min(),
            "offset_y": self.offset_y.get_min(),
            "flicker": self.flicker_var.get()
        }


class LightingPage(tk.Frame):
    def __init__(self, parent):
        super().__init__(parent)
        self.asset_path = None
        self.source_frames = []
        self._loaded = False

        self.configure(bg="#1e1e1e")

        title = tk.Label(
            self, text="Lighting Settings", font=("Segoe UI", 20, "bold"),
            fg="#005f73", bg="#1e1e1e"
        )
        title.pack(fill=tk.X, pady=(10, 20))

        canvas = tk.Canvas(self, bg="#2a2a2a", highlightthickness=0)
        canvas.pack(fill=tk.BOTH, expand=True)

        self.scroll_frame = tk.Frame(canvas, bg="#2a2a2a")
        self._window_id = canvas.create_window((0, 0), window=self.scroll_frame, anchor='nw')

        canvas.bind("<Configure>", lambda e: canvas.itemconfig(self._window_id, width=e.width))
        self.scroll_frame.bind("<Configure>", lambda e: canvas.configure(scrollregion=canvas.bbox("all")))

        def scroll_handler(evt):
            canvas.yview_scroll(int(-1 * (evt.delta / 120)), "units")

        self.scroll_frame.bind("<Enter>", lambda e: canvas.bind_all("<MouseWheel>", scroll_handler))
        self.scroll_frame.bind("<Leave>", lambda e: canvas.unbind_all("<MouseWheel>"))

        self.canvas = canvas

        add_btn = tk.Button(
            self.scroll_frame, text="Add New Light Source",
            bg="#28a745", fg="white", font=("Segoe UI", 13, "bold"),
            width=20, padx=8, pady=4,
            command=lambda: [self._add_light_source(), self._autosave()]
        )
        add_btn.pack(pady=(0, 10))

        self._loaded = True

    def _add_light_source(self, data=None):
        idx = len(self.source_frames)
        frame = LightSourceFrame(
            self.scroll_frame, idx, data,
            on_delete=lambda i=idx: self._delete_source(i)
        )
        frame.pack(fill=tk.X, padx=10, pady=8)
        self.source_frames.append(frame)

    def _delete_source(self, idx):
        if 0 <= idx < len(self.source_frames):
            self.source_frames[idx].destroy()
            self.source_frames.pop(idx)
            for i, frm in enumerate(self.source_frames):
                frm.index = i
                frm._on_delete = lambda j=i: self._delete_source(j)
        self._autosave()

    def _autosave(self):
        if getattr(self, '_loaded', False):
            self.save()

    def load(self, path):
        self.asset_path = path
        self._loaded = False
        for f in self.source_frames:
            f.destroy()
        self.source_frames.clear()
        if not path or not os.path.isfile(path):
            self._loaded = True
            return
        try:
            with open(path, 'r') as f:
                data = json.load(f)
        except Exception as e:
            messagebox.showerror("Error", f"Failed to load JSON: {e}")
            self._loaded = True
            return
        lighting = data.get("lighting_info", [])
        if isinstance(lighting, dict):
            lighting = [lighting] if lighting.get("has_light_source") else []
        for light in lighting:
            self._add_light_source(light)
        self._loaded = True

    def save(self):
        if not self.asset_path:
            messagebox.showerror("Error", "No asset selected.")
            return
        try:
            with open(self.asset_path, 'r') as f:
                data = json.load(f)
        except Exception as e:
            messagebox.showerror("Error", f"Failed to load JSON: {e}")
            return
        lighting_info = [f.get_data() for f in self.source_frames]
        data["lighting_info"] = lighting_info
        try:
            with open(self.asset_path, 'w') as f:
                json.dump(data, f, indent=4)
        except Exception as e:
            messagebox.showerror("Save Error", str(e))