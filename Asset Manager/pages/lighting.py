# === File: pages/lighting.py ===
import os
import json
import tkinter as tk
from tkinter import ttk, messagebox, colorchooser
from pages.range import Range
from pages.button import BlueButton

class LightSourceFrame(ttk.LabelFrame):
    def __init__(self, parent, index, data=None, on_delete=None):
        super().__init__(parent, text=f"Light Source {index + 1}")
        self.index = index
        self._on_delete = on_delete
        self.light_color = (255, 255, 255)
        self.image = None
        self.image_id = None
        self.radius_id = None

        self.has_light_var = tk.BooleanVar(value=True)
        self.flicker_var = tk.BooleanVar(value=False)

        del_btn = tk.Button(self, text="✕", bg="#cc3333", fg="white", width=2,
                            font=("Segoe UI", 10, "bold"), command=self._delete_self)
        del_btn.place(relx=1.0, x=-30, y=2, anchor="ne")

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

        self.flicker_cb = ttk.Checkbutton(self, text="Flicker", variable=self.flicker_var)
        self.color_preview = tk.Label(self, text="Pick Light Color", bg="#ffffff",
                                      width=20, height=2, relief='raised')
        self.color_preview.bind("<Button-1>", self._choose_color)

        # Preview canvas
        self.canvas = tk.Canvas(self, width=256, height=256, bg='black')
        self.canvas.pack(pady=(4, 12))
        self._load_base_image()

        # Layout
        self.intensity.pack(fill=tk.X, pady=4)
        self.radius.pack(fill=tk.X, pady=4)
        self.falloff.pack(fill=tk.X, pady=4)
        self.jitter_min.pack(fill=tk.X, pady=4)
        self.jitter_max.pack(fill=tk.X, pady=4)
        self.offset_x.pack(fill=tk.X, pady=4)
        self.offset_y.pack(fill=tk.X, pady=4)
        self.flicker_cb.pack(anchor='w', pady=6, padx=4)
        self.color_preview.pack(pady=10)

        if data:
            self.load_data(data)
        self.update_preview()

    def _load_base_image(self):
        import PIL.Image, PIL.ImageTk
        try:
            asset_folder = os.path.dirname(self.master.master.asset_path)
            image_path = os.path.join(asset_folder, "default", "0.png")

            if not os.path.exists(image_path):
                self.tk_image = None
                return

            img = PIL.Image.open(image_path).convert("RGBA")
            img.thumbnail((200, 200), PIL.Image.ANTIALIAS)
            self.tk_image = PIL.ImageTk.PhotoImage(img)
            self.image = self.canvas.create_image(128, 220, image=self.tk_image)
        except Exception as e:
            print(f"[Preview] Failed to load base image: {e}")
            self.tk_image = None


    def update_preview(self):
        self.canvas.delete("light")
        if not self.tk_image:
            return

        ox = self.offset_x.get_min()
        oy = self.offset_y.get_min()
        radius = self.radius.get_min()

        cx = 128 + ox
        cy = 220 - oy

        self.radius_id = self.canvas.create_oval(
            cx - radius, cy - radius, cx + radius, cy + radius,
            outline="yellow", width=2, tags="light"
        )

    def _choose_color(self, event=None):
        rgb, hex_color = colorchooser.askcolor(initialcolor=self.color_preview['bg'])
        if rgb:
            self.light_color = tuple(map(int, rgb))
            self.color_preview.config(bg=hex_color)

    def _delete_self(self):
        if self._on_delete:
            self._on_delete(self.index)

    def update_delete_callback(self, cb):
        self._on_delete = cb

    def load_data(self, data):
        self.intensity.set(data.get("light_intensity", 100), data.get("light_intensity", 100))
        self.radius.set(data.get("radius", 100), data.get("radius", 100))
        self.falloff.set(data.get("fall_off", 100), data.get("fall_off", 100))
        self.jitter_min.set(data.get("jitter_min", 0), data.get("jitter_min", 0))
        self.jitter_max.set(data.get("jitter_max", 0), data.get("jitter_max", 0))
        self.offset_x.set(data.get("offset_x", 0), data.get("offset_x", 0))
        self.offset_y.set(data.get("offset_y", 0), data.get("offset_y", 0))
        self.flicker_var.set(data.get("flicker", False))

        if "light_color" in data and isinstance(data["light_color"], list) and len(data["light_color"]) == 3:
            r, g, b = data["light_color"]
            self.light_color = (r, g, b)
            self.color_preview.config(bg="#%02x%02x%02x" % (r, g, b))

        self.update_preview()

    def get_data(self):
        return {
            "has_light_source": True,
            "light_intensity": self.intensity.get_min(),
            "light_color": list(self.light_color),
            "radius": self.radius.get_min(),
            "fall_off": self.falloff.get_min(),
            "jitter_min": self.jitter_min.get_min(),
            "jitter_max": self.jitter_max.get_min(),
            "offset_x": self.offset_x.get_min(),
            "offset_y": self.offset_y.get_min(),
            "flicker": self.flicker_var.get()
        }




class LightingPage(ttk.Frame):
    def __init__(self, parent):
        super().__init__(parent)
        self.asset_path = None
        self.source_frames = []

        self.FONT = ('Segoe UI', 14)
        self.BOLD = ('Segoe UI', 18, 'bold')
        self.COLOR_PRIMARY = "#ca6702"

        ttk.Style(self).configure('Lighting.TCheckbutton', font=self.FONT)
        ttk.Label(self, text="Lighting Settings", font=self.BOLD, foreground=self.COLOR_PRIMARY)\
            .pack(anchor='w', padx=12, pady=(10, 14))

        self.container = ttk.Frame(self)
        self.container.pack(fill=tk.BOTH, expand=True, padx=16, pady=8)

        self.add_btn = tk.Button(self, text="Add Light Source", bg="#28a745", fg="white",
                                 font=("Segoe UI", 12, "bold"), command=self._add_light_source)
        self.add_btn.pack(pady=(0, 10))

        BlueButton(self, "Save", command=self.save, x=0, y=0)

    def _add_light_source(self, data=None):
        idx = len(self.source_frames)
        frame = LightSourceFrame(self.container, idx, data, on_delete=lambda i=idx: self._delete_source(i))
        frame.pack(fill=tk.X, pady=10, padx=8)
        self.source_frames.append(frame)

    def _delete_source(self, idx):
        if 0 <= idx < len(self.source_frames):
            self.source_frames[idx].destroy()
            self.source_frames.pop(idx)
            for i, frame in enumerate(self.source_frames):
                frame.index = i
                frame.configure(text=f"Light Source {i + 1}")
                frame.update_delete_callback(lambda j=i: self._delete_source(j))


    def load(self, path):
        self.asset_path = path
        if not os.path.isfile(path):
            return

        for frame in self.source_frames:
            frame.destroy()
        self.source_frames.clear()

        try:
            with open(path, 'r') as f:
                data = json.load(f)
        except Exception as e:
            messagebox.showerror("Error", f"Failed to load JSON: {e}")
            return

        lighting = data.get("lighting_info", [])
        if isinstance(lighting, dict):
            lighting = [lighting] if lighting.get("has_light_source") else []
        elif not isinstance(lighting, list):
            lighting = []

        for light in lighting:
            self._add_light_source(light)

        if not self.source_frames:
            self._add_light_source()

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

        lighting_info = []
        for frame in self.source_frames:
            lighting_info.append(frame.get_data())

        data["lighting_info"] = lighting_info

        try:
            with open(self.asset_path, 'w') as f:
                json.dump(data, f, indent=4)
            messagebox.showinfo("Saved", "All lighting sources saved.")
        except Exception as e:
            messagebox.showerror("Save Error", str(e))
