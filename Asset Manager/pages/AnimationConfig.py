# === File 1: AnimationEditor.py ===
import os
import json
import shutil
import tkinter as tk
from tkinter import ttk, filedialog, messagebox
from PIL import Image, ImageTk
import pandas as pd
from pages.boundary import BoundaryConfigurator

class AnimationEditor(ttk.LabelFrame):
    def __init__(self, parent):
        super().__init__(parent, text="Animations")
        self.asset_folder = None
        self.trigger_name = None
        self.requires_area = False
        self.top_level_flag = None

        self.frames_path = tk.StringVar()
        self.speed_var = tk.StringVar(value="1.0")
        self.on_end_var = tk.StringVar(value="loop")
        self.randomize_start_var = tk.BooleanVar(value=False)
        self.audio_path = tk.StringVar()
        self.volume = tk.IntVar(value=50)

        self.frames = []
        self.preview_canvas = tk.Canvas(self, bg="black", width=320, height=240)
        self.preview_job = None
        self.current_frame = 0

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
        ttk.Button(self, text="Browse", command=self._select_folder).grid(row=row, column=2, sticky='w', padx=8, pady=4)

        row += 1
        ttk.Label(self, text="Speed:", font=font_large).grid(row=row, column=0, sticky='w', padx=8, pady=4)
        speeds = ["0.25", "0.5", "1.0", "1.25", "1.5", "2.0"]
        ttk.OptionMenu(self, self.speed_var, "1.0", *speeds).grid(row=row, column=1, sticky='w', padx=8, pady=4)

        row += 1
        ttk.Label(self, text="On End:", font=font_large).grid(row=row, column=0, sticky='w', padx=8, pady=4)
        self.on_end_menu = ttk.OptionMenu(self, self.on_end_var, "loop", "loop", "reverse")
        self.on_end_menu.grid(row=row, column=1, sticky='ew', padx=8, pady=4)

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
            # Properly convert to boolean: 1/True = True, 0/False = False
            self.requires_area = str(raw_val).strip().lower() in ("1", "true")
            self.top_level_flag = row["top_level_flag"].values[0]
        else:
            self.requires_area = False
            self.top_level_flag = None

        if self.requires_area:
            self.area_button.grid()
        else:
            self.area_button.grid_remove()


    def _select_audio(self):
        file = filedialog.askopenfilename(filetypes=[("WAV files", "*.wav")])
        if file:
            self.audio_path.set(file)

    def _select_folder(self):
        folder = filedialog.askdirectory()
        if not folder or not self.asset_folder or not self.trigger_name:
            return

        target_folder = os.path.join(self.asset_folder, self.trigger_name)
        os.makedirs(target_folder, exist_ok=True)
        for file in os.listdir(folder):
            if file.endswith(".png"):
                shutil.copy2(os.path.join(folder, file), os.path.join(target_folder, file))

        # ✅ Store only relative path
        self.frames_path.set(self.trigger_name)
        self._load_preview_frames(target_folder)


    def _load_preview_frames(self, folder):
        self.frames = []
        for file in sorted(os.listdir(folder)):
            if file.endswith(".png"):
                img = Image.open(os.path.join(folder, file)).convert("RGBA")
                img.thumbnail((320, 240), Image.LANCZOS)
                self.frames.append(ImageTk.PhotoImage(img))
        if self.frames:
            self.current_frame = 0
            self._animate_preview()

    def _animate_preview(self):
        if not self.frames:
            return
        self.preview_canvas.delete("all")
        self.preview_canvas.create_image(0, 0, anchor='nw', image=self.frames[self.current_frame])
        self.current_frame = (self.current_frame + 1) % len(self.frames)
        try:
            speed = float(self.speed_var.get())
        except ValueError:
            speed = 1.0
        delay = int(1000 / (24 * max(0.01, speed)))
        self.preview_job = self.after(delay, self._animate_preview)

    def load(self, trigger_name: str, anim_data: dict, asset_folder: str):
        self.asset_folder = asset_folder
        self.trigger_var.set(trigger_name)
        self._on_trigger_select(trigger_name)

        self.frames_path.set(anim_data.get("frames_path", ""))
        self.speed_var.set(str(anim_data.get("speed", "1.0")))
        self.on_end_var.set(anim_data.get("on_end", "loop"))
        self.randomize_start_var.set(anim_data.get("randomize", False))
        self.audio_path.set(anim_data.get("audio_path", ""))
        self.volume.set(anim_data.get("volume", 50))

        preview_dir = os.path.join(asset_folder, anim_data.get("frames_path", ""))
        if os.path.isdir(preview_dir):
            self._load_preview_frames(preview_dir)

    def save(self):
        if not self.trigger_name or not self.frames_path.get() or not self.speed_var.get() or not self.on_end_var.get():
            messagebox.showerror("Missing Data", "Required fields are missing: trigger, frames, speed, or on_end")
            return None

        if self.requires_area:
            area_path = os.path.join(self.asset_folder, f"{self.trigger_name}_area.json")
            if not os.path.isfile(area_path):
                messagebox.showerror("Missing Area", f"Required area file not found for trigger '{self.trigger_name}'")
                return None

        return {
            "frames_path": self.frames_path.get(),
            "speed": float(self.speed_var.get()),
            "on_end": self.on_end_var.get(),
            "loop": self.on_end_var.get() == "loop",
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