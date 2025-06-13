# === File: AnimationEditor.py ===
import os
import json
import shutil
import tkinter as tk
from tkinter import ttk, filedialog, messagebox
from PIL import Image, ImageTk
import pandas as pd
from pages.boundary import BoundaryConfigurator
from pages.animation_uploader import AnimationUploader

class AnimationEditor(ttk.LabelFrame):
    def __init__(self, parent):
        super().__init__(parent, text="Animations")
        self.asset_folder = None
        self.trigger_name = None
        self.requires_area = False
        self.top_level_flag = None

        self.frames_path = tk.StringVar()
        self.speed_var = tk.StringVar(value="1.0")
        self.on_end_var = tk.StringVar(value="")
        self.randomize_start_var = tk.BooleanVar(value=False)
        self.audio_path = tk.StringVar()
        self.volume = tk.IntVar(value=50)
        self.event_data_var = tk.StringVar()

        self.preview_canvas = tk.Canvas(self, bg="black", width=320, height=240)
        self.preview_gif = None

        self.triggers_df = pd.read_csv("Asset Manager/triggers.csv")
        self.available_triggers = self.triggers_df["trigger_name"].tolist()

        self._build_ui()

    def _build_ui(self):
        font_large = ("Segoe UI", 14)
        row = 0

        ttk.Label(self, text="Trigger Type:", font=font_large).grid(row=row, column=0, sticky='w', padx=8, pady=4)
        self.trigger_var = tk.StringVar()
        trigger_menu = ttk.OptionMenu(self, self.trigger_var, None, *self.available_triggers, command=self._on_trigger_select)
        trigger_menu.grid(row=row, column=1, sticky='ew', padx=8, pady=4)

        row += 1
        ttk.Label(self, text="Frames Folder:", font=font_large).grid(row=row, column=0, sticky='w', padx=8, pady=4)
        ttk.Entry(self, textvariable=self.frames_path, width=40, font=font_large).grid(row=row, column=1, sticky='ew', padx=8, pady=4)
        ttk.Button(self, text="Browse", command=self._use_animation_uploader).grid(row=row, column=2, sticky='w', padx=8, pady=4)

        row += 1
        ttk.Label(self, text="Speed:", font=font_large).grid(row=row, column=0, sticky='w', padx=8, pady=4)
        speeds = ["0.25", "0.5", "1.0", "1.25", "1.5", "2.0"]
        ttk.OptionMenu(self, self.speed_var, "1.0", *speeds).grid(row=row, column=1, sticky='w', padx=8, pady=4)

        row += 1
        ttk.Label(self, text="On End:", font=font_large).grid(row=row, column=0, sticky='w', padx=8, pady=4)
        self.on_end_menu_var = tk.StringVar(value="")
        self.on_end_menu = ttk.OptionMenu(self, self.on_end_menu_var, "")
        self.on_end_menu.grid(row=row, column=1, sticky='ew', padx=8, pady=4)

        row += 1
        ttk.Label(self, text="Event Data:", font=font_large).grid(row=row, column=0, sticky='w', padx=8, pady=4)
        ttk.Entry(self, textvariable=self.event_data_var, font=font_large).grid(row=row, column=1, sticky='ew', padx=8, pady=4)

        row += 1
        ttk.Checkbutton(self, text="Randomize Start Frame", variable=self.randomize_start_var).grid(row=row, column=0, columnspan=2, sticky='w', padx=8, pady=4)

        row += 1
        ttk.Button(self, text="Add Audio", command=self._select_audio).grid(row=row, column=0, sticky='w', padx=8, pady=4)
        ttk.Scale(self, from_=0, to=100, orient='horizontal', variable=self.volume).grid(row=row, column=1, sticky='ew', padx=8, pady=4)

        row += 1
        self.area_button = ttk.Button(self, text="Configure Area", command=self._on_area_configure)
        self.area_button.grid(row=row, column=0, columnspan=2, sticky='w', padx=8, pady=4)
        self.area_button.grid_remove()

        row += 1
        self.preview_canvas.grid(row=row, column=0, columnspan=3, padx=8, pady=12)

    def _on_trigger_select(self, trigger_name):
        self.trigger_name = trigger_name

        row = self.triggers_df[self.triggers_df["trigger_name"] == trigger_name]
        if not row.empty:
            raw_val = row["requires_area"].values[0]
            self.requires_area = str(raw_val).strip().lower() in ("1", "true")
            self.top_level_flag = row["top_level_flag"].values[0]
        else:
            self.requires_area = False
            self.top_level_flag = None

        if self.requires_area:
            self.area_button.grid()
        else:
            self.area_button.grid_remove()

        self._update_on_end_options()

    def _update_on_end_options(self):
        if not self.asset_folder:
            return

        options = []
        info_path = os.path.join(self.asset_folder, "info.json")
        if os.path.isfile(info_path):
            with open(info_path, "r") as f:
                try:
                    data = json.load(f)
                    keys = list(data.get("animations", {}).keys())

                    if self.trigger_name in keys:
                        keys.remove(self.trigger_name)
                        keys.insert(0, self.trigger_name)  # Put self-reference first

                    if "reverse" not in keys:
                        keys.insert(1, "reverse")  # Explicitly add "reverse" as second option

                    options += keys
                except Exception:
                    pass


        menu = self.on_end_menu["menu"]
        menu.delete(0, "end")
        for opt in options:
            menu.add_command(label=opt, command=lambda v=opt: self.on_end_menu_var.set(v))

    def _select_audio(self):
        file = filedialog.askopenfilename(filetypes=[("WAV files", "*.wav")])
        if file:
            self.audio_path.set(file)

    def _use_animation_uploader(self):
        if not self.asset_folder or not self.trigger_name:
            messagebox.showerror("Missing Info", "Asset folder or trigger type not set.")
            return
        uploader = AnimationUploader(self.asset_folder, self.trigger_name)
        result_folder = uploader.run()
        if result_folder:
            self.frames_path.set(self.trigger_name)
            self._load_gif_preview()

    def _load_gif_preview(self):
        if not self.asset_folder or not self.trigger_name:
            return

        gif_path = os.path.join(self.asset_folder, self.trigger_name, "preview.gif")
        if not os.path.isfile(gif_path):
            return

        gif = Image.open(gif_path)
        width = 320
        height = int((width / gif.width) * gif.height)
        gif = gif.resize((width, height), Image.LANCZOS)

        self.preview_gif = ImageTk.PhotoImage(gif)
        self.preview_canvas.delete("all")
        self.preview_canvas.create_image(0, 0, anchor='nw', image=self.preview_gif)

    def load(self, trigger_name: str, anim_data: dict, asset_folder: str):
        self.asset_folder = asset_folder
        self.trigger_var.set(trigger_name)
        self._on_trigger_select(trigger_name)

        self.frames_path.set(anim_data.get("frames_path", ""))
        self.speed_var.set(str(anim_data.get("speed", "1.0")))
        self.on_end_menu_var.set(anim_data.get("on_end", ""))
        self.event_data_var.set(", ".join(anim_data.get("event_data", [])))
        self.randomize_start_var.set(anim_data.get("randomize", False))
        self.audio_path.set(anim_data.get("audio_path", ""))
        self.volume.set(anim_data.get("volume", 50))

        self._load_gif_preview()

    def save(self):
        if not self.trigger_name or not self.frames_path.get() or not self.speed_var.get():
            messagebox.showerror("Missing Data", "Required fields are missing: trigger, frames, or speed")
            return None

        if self.requires_area:
            area_path = os.path.join(self.asset_folder, f"{self.trigger_name}_area.json")
            if not os.path.isfile(area_path):
                messagebox.showerror("Missing Area", f"Required area file not found for trigger '{self.trigger_name}'")
                return None

        return {
            "frames_path": self.frames_path.get(),
            "speed": float(self.speed_var.get()),
            "on_end": self.on_end_menu_var.get(),
            "event_data": [x.strip() for x in self.event_data_var.get().split(",") if x.strip()],
            "randomize": self.randomize_start_var.get(),
            "audio_path": self.audio_path.get(),
            "volume": self.volume.get(),
            **({"area_path": f"{self.trigger_name}_area.json"} if self.requires_area else {})
        }

    def _on_area_configure(self):
        if not self.asset_folder or not self.trigger_name or not self.frames_path.get():
            messagebox.showerror("Missing Info", "Please select frames and a trigger before configuring area.")
            return

        full_path = os.path.join(self.asset_folder, self.frames_path.get())
        if not os.path.isdir(full_path):
            messagebox.showerror("Frames Folder", f"Frames directory not found: {full_path}")
            return

        def save_callback(geo):
            out_path = os.path.join(self.asset_folder, f"{self.trigger_name}_area.json")
            with open(out_path, "w") as f:
                json.dump(geo, f, indent=2)

        BoundaryConfigurator(self, base_folder=full_path, callback=save_callback)
