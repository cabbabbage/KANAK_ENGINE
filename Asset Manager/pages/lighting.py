import os
import json
import tkinter as tk
from tkinter import ttk, messagebox, colorchooser
from pages.range import Range
from pages.lighting_controls import LightingControls, LightSourceFrame
from pages.apply_page_settings import ApplyPageSettings

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

        # Shading Header Area
        header = tk.Frame(self.scroll_frame, bg="#2a2a2a")
        header.pack(fill=tk.X, padx=10, pady=10)

        self.has_shading_var = tk.BooleanVar(value=False)
        self.has_shading_var.trace_add("write", lambda *_: self._toggle_shading())

        tk.Checkbutton(
            header, text="Has Shading", variable=self.has_shading_var,
            bg="#2a2a2a", fg="#FFFFFF", font=("Segoe UI", 12), selectcolor="#2a2a2a",
            activebackground="#2a2a2a", activeforeground="#FFFFFF"
        ).pack(anchor="w")

        self.shading_control = LightingControls(header, show_orbit=True)
        self.shading_control.pack(fill=tk.X, pady=(10, 0))

        # Bind autosave for all shading_control sliders and flicker
        for rng in (
            self.shading_control.intensity,
            self.shading_control.radius,
            self.shading_control.orbit_radius,
            self.shading_control.falloff,
            self.shading_control.jitter_min,
            self.shading_control.flare,
            self.shading_control.offset_x,
            self.shading_control.offset_y,
        ):
            rng.var_min.trace_add("write", lambda *_: self._autosave())
            rng.var_max.trace_add("write", lambda *_: self._autosave())
        self.shading_control.flicker_var.trace_add("write", lambda *_: self._autosave())

        add_btn = tk.Button(
            self.scroll_frame, text="Add New Light Source",
            bg="#28a745", fg="white", font=("Segoe UI", 13, "bold"),
            width=20, padx=8, pady=4,
            command=lambda: [self._add_light_source(), self._autosave()]
        )
        add_btn.pack(pady=(10, 10))

        self._apply_btn = ApplyPageSettings(
            self.scroll_frame,
            page_data=lambda: {
                "has_shading": self.has_shading_var.get(),
                "lighting_info": (
                    [self.shading_control.get_data()] if self.has_shading_var.get() else []
                ) + [f.get_data() for f in self.source_frames]
            },
            label="Apply Lighting to Another Asset"
        )

        self._loaded = True

    def _toggle_shading(self):
        if self.has_shading_var.get():
            self.shading_control.pack(fill=tk.X, pady=(10, 0))
        else:
            self.shading_control.pack_forget()
        self._autosave()

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

        # Extract the light with orbit_radius > 0 (if any) for shading_control
        shading_light = None
        normal_lights = []
        for light in lighting:
            if shading_light is None and light.get("orbit_radius", 0) > 0:
                shading_light = light
            else:
                normal_lights.append(light)

        if shading_light:
            self.has_shading_var.set(True)
            self.shading_control.load_data(shading_light)
            self.shading_control.pack(fill=tk.X, pady=(10, 0))
        else:
            self.has_shading_var.set(False)
            self.shading_control.pack_forget()

        for light in normal_lights:
            self._add_light_source(light)

        self._apply_btn.pack(pady=(10, 10))
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

        data["has_shading"] = self.has_shading_var.get()

        lighting_info = [f.get_data() for f in self.source_frames]
        if self.has_shading_var.get():
            lighting_info.insert(0, self.shading_control.get_data())

        data["lighting_info"] = lighting_info

        try:
            with open(self.asset_path, 'w') as f:
                json.dump(data, f, indent=4)
        except Exception as e:
            messagebox.showerror("Save Error", str(e))
