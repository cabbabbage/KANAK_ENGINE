# === File: pages/lighting.py ===
import os
import json
import tkinter as tk
from tkinter import ttk, colorchooser, messagebox
from pages.range import Range

class LightSourceFrame(ttk.LabelFrame):
    def __init__(self, parent, index, data=None, on_delete=None):
        super().__init__(parent, text=f"Light Source {index + 1}", style='LS.TLabelframe')
        self.configure(padding=(10, 10))
        self.index = index
        self._on_delete = on_delete
        self.light_color = (255, 255, 255)

        # Variables
        self.flicker_var = tk.BooleanVar(value=False)

        # Delete button
        del_btn = tk.Button(
            self, text="X", bg="#D9534F", fg="white",
            font=("Segoe UI", 12, "bold"), width=3, padx=4,
            command=self._delete_self
        )
        del_btn.pack(anchor='ne')

        # Section header
        sec = ttk.Label(
            self, text="Light Properties", font=("Segoe UI", 13, "bold"),
            foreground="#DDDDDD", background='#2a2a2a'
        )
        sec.pack(anchor='w', pady=(10, 4))

        # Sliders
        self.intensity = Range(self, min_bound=0, max_bound=100, label="Light Intensity")
        self.radius = Range(self, min_bound=10, max_bound=2000, label="Radius (px)")
        self.falloff = Range(self, min_bound=0, max_bound=100, label="Falloff (%)")
        self.jitter_min = Range(self, min_bound=0, max_bound=20, label="Jitter Min (px)")
        self.jitter_max = Range(self, min_bound=0, max_bound=20, label="Jitter Max (px)")
        self.jitter_min.set_fixed()
        self.jitter_max.set_fixed()
        self.offset_x = Range(self, min_bound=-2000, max_bound=2000, label="Offset X")
        self.offset_y = Range(self, min_bound=-2000, max_bound=2000, label="Offset Y")
        self.offset_x.set_fixed()
        self.offset_y.set_fixed()

        # Initialize default values so sliders appear immediately
        for rng in (
            self.intensity, self.radius, self.falloff,
            self.jitter_min, self.jitter_max,
            self.offset_x, self.offset_y
        ):
            rng.set(rng.min_bound, rng.min_bound)

        # Pack sliders
        for widget in (
            self.intensity, self.radius, self.falloff,
            self.jitter_min, self.jitter_max,
            self.offset_x, self.offset_y
        ):
            widget.pack(fill=tk.X, padx=10, pady=4)

        # Flicker & color picker
        flick_cb = ttk.Checkbutton(self, text="Flicker", variable=self.flicker_var)
        flick_cb.pack(anchor='w', padx=10, pady=4)

        self.color_preview = tk.Label(
            self, text="Pick Light Color", bg="#FFFFFF",
            font=("Segoe UI", 12), width=20, height=2, relief='raised'
        )
        self.color_preview.pack(padx=10, pady=4)
        self.color_preview.bind("<Button-1>", self._choose_color)

        # Preview canvas
        self.canvas = tk.Canvas(self, width=256, height=256, bg='black')
        self.canvas.pack(fill=tk.BOTH, expand=True, padx=10, pady=(4, 12))
        self._load_base_image()
        self.update_preview()

        # Autosave bindings
        self._bind_autosave()

        if data:
            self.load_data(data)
            self.update_preview()

    def _bind_autosave(self):
        for rng in (
            self.intensity, self.radius, self.falloff,
            self.jitter_min, self.jitter_max,
            self.offset_x, self.offset_y
        ):
            rng.var_min.trace_add("write", lambda *a: self._autosave())
            rng.var_max.trace_add("write", lambda *a: self._autosave())
        self.flicker_var.trace_add("write", lambda *a: self._autosave())

    def _autosave(self):
        root = self.master.master
        if getattr(root, '_loaded', False):
            root.save()

    def _load_base_image(self):
        import PIL.Image, PIL.ImageTk
        try:
            asset_folder = os.path.dirname(self.master.master.asset_path or '')
            image_path = os.path.join(asset_folder, "default", "0.png")
            if not os.path.exists(image_path):
                self.tk_image = None
                return
            img = PIL.Image.open(image_path).convert("RGBA")
            img.thumbnail((200, 200), PIL.Image.ANTIALIAS)
            self.tk_image = PIL.ImageTk.PhotoImage(img)
            self.canvas.create_image(128, 220, image=self.tk_image)
        except Exception as e:
            print(f"[Preview] Failed to load base image: {e}")
            self.tk_image = None

    def update_preview(self):
        self.canvas.delete("light")
        if not getattr(self, 'tk_image', None):
            return
        ox = self.offset_x.get_min()
        oy = self.offset_y.get_min()
        radius = self.radius.get_min()
        cx, cy = 128 + ox, 220 - oy
        self.canvas.create_oval(
            cx - radius, cy - radius, cx + radius, cy + radius,
            outline="yellow", width=2, tags="light"
        )

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
        self.falloff.set(data.get("fall_off", 100), data.get("fall_off", 100))
        self.jitter_min.set(data.get("jitter_min", 0), data.get("jitter_min", 0))
        self.jitter_max.set(data.get("jitter_max", 0), data.get("jitter_max", 0))
        self.offset_x.set(data.get("offset_x", 0), data.get("offset_x", 0))
        self.offset_y.set(data.get("offset_y", 0), data.get("offset_y", 0))
        self.flicker_var.set(data.get("flicker", False))
        if data.get("light_color") and isinstance(data["light_color"], list) and len(data["light_color"]) == 3:
            r, g, b = data["light_color"]
            self.light_color = (r, g, b)
            self.color_preview.config(bg="#%02x%02x%02x" % (r, g, b))

    def get_data(self):
        return {
            "has_light_source": True,
            "light_intensity": self.intensity.get_min(),
            "light_color": list(self.light_color),
            "radius": self.radius.get_min(),
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

        # Configure styles now
        style = ttk.Style(self)
        style.configure('LS.TLabelframe', background='#2a2a2a', borderwidth=0)
        style.configure('LS.TLabelframe.Label', background='#2a2a2a', font=('Segoe UI', 13, 'bold'), foreground='#DDDDDD')

        # Page background
        self.configure(bg='#1e1e1e')

        # Title header
        title = tk.Label(
            self, text="Lighting Settings",
            font=("Segoe UI", 20, "bold"),
            fg="#005f73", bg='#1e1e1e'
        )
        title.pack(fill=tk.X, pady=(10, 20))

        # Scrollable area
        self.canvas = tk.Canvas(self, bg='#2a2a2a', highlightthickness=0)
        self.scroll_frame = tk.Frame(self.canvas, bg='#2a2a2a')
        self._window_id = self.canvas.create_window((0, 0), window=self.scroll_frame, anchor='nw')
        self.scroll_frame.bind('<Configure>', lambda e: self.canvas.configure(scrollregion=self.canvas.bbox('all')))
        self.canvas.bind('<Configure>', lambda e: self.canvas.itemconfig(self._window_id, width=e.width))
        scroll_handler = lambda evt: self.canvas.yview_scroll(int(-1 * (evt.delta / 120)), 'units')
        self.scroll_frame.bind('<Enter>', lambda e: self.canvas.bind_all('<MouseWheel>', scroll_handler))
        self.scroll_frame.bind('<Leave>', lambda e: self.canvas.unbind_all('<MouseWheel>'))
        self.canvas.pack(fill=tk.BOTH, expand=True)

        # "Add New Light Source" button
        self.add_btn = tk.Button(
            self.scroll_frame, text="Add New Light Source",
            bg="#28a745", fg="white",
            font=("Segoe UI", 13, "bold"),
            width=18, padx=8, pady=4,
            command=lambda: [self._add_light_source(), self._autosave()]
        )
        self.add_btn.pack(pady=(0, 10))

        self._loaded = True

    def _add_light_source(self, data=None):
        idx = len(self.source_frames)
        frame = LightSourceFrame(
            self.scroll_frame, idx, data,
            on_delete=lambda i=idx: self._delete_source(i)
        )
        frame.pack(fill=tk.X, padx=10, pady=4)
        self.source_frames.append(frame)

    def _delete_source(self, idx):
        if 0 <= idx < len(self.source_frames):
            self.source_frames[idx].destroy()
            self.source_frames.pop(idx)
            for i, frm in enumerate(self.source_frames):
                frm.index = i
                frm.configure(text=f"Light Source {i+1}")
                frm._on_delete = lambda j=i: self._delete_source(j)
        self._autosave()

    def _autosave(self):
        if getattr(self, '_loaded', False):
            self.save()

    def load(self, path):
        self.asset_path = path
        self._loaded = False
        # Clear existing frames
        for f in self.source_frames:
            f.destroy()
        self.source_frames.clear()
        # Load JSON if path valid
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
        # Add frames for existing lights only
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
