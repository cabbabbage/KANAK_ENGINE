# pages/basic_info.py

import os
import json
import shutil
import tkinter as tk
from tkinter import ttk, filedialog, messagebox
from PIL import Image, ImageTk

class BasicInfoPage(ttk.Frame):
    def __init__(self, parent, on_rename_callback=None):
        super().__init__(parent)

        # — Theme & fonts —
        FONT             = ('Segoe UI', 14)
        FONT_BOLD        = ('Segoe UI', 18, 'bold')
        MAIN_COLOR       = "#005f73"  # teal
        SECONDARY_COLOR  = "#ee9b00"  # amber
        DRAW_COLOR       = "#f94144"  # coral

        # --- state ---
        self.asset_path = None
        self.default_frames_path = None
        self.audio_path = ""
        self.on_rename = on_rename_callback

        # track if user picked new frames this session
        self._frames_changed = False

        # Preview & center state
        self.preview_frames = []
        self.current_preview_idx = 0
        self.preview_job = None
        self.scale = 0.5
        self.orig_w = self.orig_h = 0
        self.center_x = None
        self.center_y = None
        self.center_dot_id = None

        W = 20  # uniform widget width

        # — Styles —
        style = ttk.Style(self)
        style.configure('Main.TButton',
                        font=FONT, padding=6,
                        background=MAIN_COLOR,
                        foreground=SECONDARY_COLOR)
        style.map('Main.TButton',
                  background=[('active', SECONDARY_COLOR)],
                  foreground=[('active', MAIN_COLOR)])
        style.configure('Secondary.TButton',
                        font=FONT, padding=6,
                        background=SECONDARY_COLOR,
                        foreground=MAIN_COLOR)
        style.map('Secondary.TButton',
                  background=[('active', MAIN_COLOR)],
                  foreground=[('active', SECONDARY_COLOR)])
        style.configure('Main.TMenubutton',
                        font=FONT, padding=6,
                        background='white',
                        foreground=SECONDARY_COLOR,
                        borderwidth=2, relief='solid')
        style.map('Main.TMenubutton',
                  background=[('active', 'white')],
                  foreground=[('active', SECONDARY_COLOR)])
        style.configure('Large.TLabel', font=FONT)
        style.configure('LargeBold.TLabel',
                        font=FONT_BOLD,
                        foreground=SECONDARY_COLOR)
        style.configure('Large.TEntry', font=FONT)
        style.configure('Link.TLabel',
                        font=FONT,
                        foreground=SECONDARY_COLOR)
        style.configure('Large.TCheckbutton', font=FONT)

        self._draw_color = DRAW_COLOR  # center‐dot color

        # equal column weights
        for col in range(5):
            self.columnconfigure(col, weight=1)

        # — Title —
        ttk.Label(self, text="Basic Asset Info",
                  style='LargeBold.TLabel')\
            .grid(row=0, column=0, columnspan=5,
                  pady=(10, 20), padx=12)

        # — Asset Name —
        ttk.Label(self, text="Asset Name:", style='Large.TLabel')\
            .grid(row=1, column=0, sticky="w", padx=12, pady=6)
        self.name_entry = ttk.Entry(self, width=W, style='Large.TEntry')
        self.name_entry.grid(row=1, column=1, sticky="w",
                             padx=12, pady=6, ipady=6)

        # — Asset Type —
        ttk.Label(self, text="Asset Type:", style='Large.TLabel')\
            .grid(row=2, column=0, sticky="w", padx=12, pady=6)
        self.type_var = tk.StringVar(self)
        self.type_menu = ttk.OptionMenu(self, self.type_var, "")
        self.type_menu.config(style='Main.TMenubutton', width=W)
        self.type_menu.grid(row=2, column=1, sticky="w", padx=12, pady=6)

        # — Default Frames —
        ttk.Label(self, text="Default Frames:", style='Large.TLabel')\
            .grid(row=3, column=0, sticky="w", padx=12, pady=6)
        ttk.Button(self,
                   text="Select Frames",
                   command=self.select_frames,
                   style='Main.TButton',
                   width=W)\
            .grid(row=3, column=1, sticky="w", padx=12, pady=6)
        self.default_frames_label = ttk.Label(
            self, text="", style='Link.TLabel',
            cursor="hand2", width=W
        )
        self.default_frames_label.grid(
            row=3, column=2, sticky="w", padx=12, pady=6
        )
        self.default_frames_label.bind(
            "<Button-1>", self.open_frames_folder
        )

        # — On-End Animation —
        ttk.Label(self, text="On-End Animation:", style='Large.TLabel')\
            .grid(row=4, column=0, sticky="w", padx=12, pady=6)
        self.on_end_var = tk.StringVar(self, value="default")
        self.on_end_menu = ttk.OptionMenu(self, self.on_end_var, "default")
        self.on_end_menu.config(style='Main.TMenubutton', width=W)
        self.on_end_menu.grid(row=4, column=1, sticky="w", padx=12, pady=6)
        self.loop_var = tk.BooleanVar(self, True)
        ttk.Checkbutton(self, text="Loop", variable=self.loop_var,
                        command=self._update_on_end_state,
                        style='Large.TCheckbutton')\
            .grid(row=4, column=2, sticky="w", padx=12, pady=6)

        # — Audio & Volume —
        ttk.Label(self, text="Audio (optional):", style='Large.TLabel')\
            .grid(row=5, column=0, sticky="w", padx=12, pady=6)
        ttk.Button(self,
                   text="Select Audio",
                   command=self.select_audio,
                   style='Main.TButton',
                   width=W)\
            .grid(row=5, column=1, sticky="w", padx=12, pady=6)
        self.audio_label = ttk.Label(
            self, text="", style='Link.TLabel',
            cursor="hand2", width=W
        )
        self.audio_label.grid(row=5, column=2, sticky="w", padx=12, pady=6)
        self.audio_label.bind("<Button-1>", self.open_audio_file)
        ttk.Label(self, text="Audio Volume:", style='Large.TLabel')\
            .grid(row=5, column=3, sticky="w", padx=12, pady=6)
        self.volume_var = tk.IntVar(self, 0)
        self.volume_spin = tk.Spinbox(
            self, from_=0, to=100,
            textvariable=self.volume_var,
            font=FONT,
            width=5, fg=SECONDARY_COLOR
        )
        self.volume_spin.grid(row=5, column=4,
                              sticky="w", padx=12, pady=6)

        # — Save Button (above preview) —
        ttk.Button(self,
                   text="Save",
                   command=self.save,
                   style='Secondary.TButton',
                   width=W)\
            .grid(row=6, column=0, columnspan=5,
                  pady=(20,10), padx=12)

        # — Preview Canvas —
        self.preview_canvas = tk.Canvas(
            self, bg='black', bd=2, relief='sunken'
        )
        self.preview_canvas.grid(
            row=7, column=0, columnspan=5,
            pady=12, padx=12
        )
        self.preview_canvas.bind("<Button-1>", self._on_canvas_click)


    def select_frames(self):
        folder = filedialog.askdirectory()
        if not folder:
            return
        self._frames_changed = True
        self.default_frames_path = folder
        self.default_frames_label.config(text=os.path.basename(folder))
        self._load_preview_frames()


    def _load_preview_frames(self):
        if self.preview_job:
            self.after_cancel(self.preview_job)
            self.preview_job = None

        files = sorted(
            f for f in os.listdir(self.default_frames_path)
            if f.lower().endswith('.png')
        )
        self.preview_frames = []
        for idx, fname in enumerate(files):
            path = os.path.join(self.default_frames_path, fname)
            img = Image.open(path).convert('RGBA')
            if idx == 0:
                self.orig_w, self.orig_h = img.size
                if self.center_x is None:
                    self.center_x = self.orig_w // 2
                    self.center_y = self.orig_h // 2
            w, h = img.size
            disp = (int(w * self.scale), int(h * self.scale))
            img_resized = img.resize(disp, Image.LANCZOS)
            self.preview_frames.append(ImageTk.PhotoImage(img_resized))

        if self.preview_frames:
            w_disp, h_disp = self.preview_frames[0]._PhotoImage__size
            self.preview_canvas.config(width=w_disp, height=h_disp)
            self.current_preview_idx = 0
            self._animate_preview()


    def _animate_preview(self):
        frame = self.preview_frames[self.current_preview_idx]
        self.preview_canvas.delete("all")
        self.preview_canvas.create_image(0, 0,
                                         anchor='nw',
                                         image=frame)
        self._draw_center_dot()
        self.current_preview_idx = (
            self.current_preview_idx + 1
        ) % len(self.preview_frames)
        self.preview_job = self.after(
            200, self._animate_preview
        )


    def _draw_center_dot(self):
        if self.center_dot_id:
            self.preview_canvas.delete(self.center_dot_id)
        if self.center_x is None:
            return
        dx = int(self.center_x * self.scale)
        dy = int(self.center_y * self.scale)
        r = 5
        self.center_dot_id = self.preview_canvas.create_oval(
            dx-r, dy-r, dx+r, dy+r,
            fill=self._draw_color, outline=''
        )


    def _on_canvas_click(self, event):
        if not self.orig_w:
            return
        disp_x, disp_y = event.x, event.y
        disp_w = self.preview_canvas.winfo_width()
        disp_h = self.preview_canvas.winfo_height()
        disp_x = max(0, min(disp_x, disp_w))
        disp_y = max(0, min(disp_y, disp_h))
        self.center_x = int(disp_x / self.scale)
        self.center_y = int(disp_y / self.scale)
        self._draw_center_dot()


    def open_frames_folder(self, event):
        if self.default_frames_path and os.path.isdir(self.default_frames_path):
            os.startfile(self.default_frames_path)


    def select_audio(self):
        file = filedialog.askopenfilename(
            filetypes=[("Audio", "*.wav;*.mp3;*.ogg")]
        )
        if file:
            self.audio_path = file
            self.audio_label.config(text=os.path.basename(file))


    def open_audio_file(self, event):
        if self.audio_path and os.path.isfile(self.audio_path):
            os.startfile(self.audio_path)


    def _update_on_end_state(self):
        state = "disabled" if self.loop_var.get() else "normal"
        self.on_end_menu.config(state=state)
        menu = self.on_end_menu["menu"]
        end_index = menu.index("end")
        if end_index is None:
            return
        for i in range(end_index + 1):
            menu.entryconfig(i, state=state)


    def load(self, info_path):
        """Load JSON, initialize defaults, populate UI & preview."""
        self.asset_path = info_path
        if not info_path:
            return
        if not os.path.exists(info_path):
            with open(info_path, "w") as f:
                f.write("{}")
        with open(info_path, "r") as f:
            data = json.load(f)

        # initialize missing keys…
        changed = False
        if "types" not in data:
            data["types"] = ["Player","Object","Background","Enemy"]
            changed = True
        if "available_animations" not in data:
            data["available_animations"] = []
            changed = True
        if "child_only" not in data:
            data["child_only"] = False
            changed = True
        if changed:
            with open(info_path,"w") as f:
                json.dump(data, f, indent=4)

        # Asset name and type…
        self.name_entry.delete(0, tk.END)
        self.name_entry.insert(0, data.get("asset_name",""))
        types = data["types"]
        self.type_var.set(data.get("asset_type", types[0]))
        menu = self.type_menu["menu"]
        menu.delete(0,"end")
        for t in types:
            menu.add_command(label=t,
                             command=lambda v=t: self.type_var.set(v))

        # Default animation…
        default = data.get("default_animation", {})
        on_start = default.get("on_start","")
        self.default_frames_label.config(text=on_start)
        self.loop_var.set(default.get("loop", True))
        folder = os.path.join(os.path.dirname(info_path), on_start)
        if on_start and os.path.isdir(folder):
            self.default_frames_path = folder
            center = data.get("center")
            if center:
                self.center_x = center.get("x")
                self.center_y = center.get("y")
            self._load_preview_frames()

        # On-end dropdown…
        menu = self.on_end_menu["menu"]
        menu.delete(0,"end")
        for anim in data.get("available_animations", []):
            menu.add_command(label=anim,
                             command=lambda v=anim: self.on_end_var.set(v))
        self.on_end_var.set(default.get("on_end","default"))
        self._update_on_end_state()

        # Audio/volume…
        self.audio_path = default.get("audio_path","")
        self.audio_label.config(
            text=os.path.basename(self.audio_path) if self.audio_path else ""
        )
        self.volume_var.set(default.get("volume",0))


    def save(self):
        """Save basic info; rename folder, update JSON, copy frames,
        and propagate any child‐asset references across SRC."""
        if not self.asset_path:
            messagebox.showerror("Error","No asset selected.")
            return

        # load existing JSON
        with open(self.asset_path,"r") as f:
            data = json.load(f)

        # rename folder if needed…
        old_name = os.path.basename(os.path.dirname(self.asset_path))
        new_name = self.name_entry.get().strip() or old_name
        asset_root = os.path.dirname(os.path.dirname(self.asset_path))
        old_folder = os.path.join(asset_root, old_name)
        new_folder = os.path.join(asset_root, new_name)
        if new_name != old_name:
            if os.path.exists(new_folder):
                messagebox.showerror("Error",f"Folder '{new_name}' exists.")
                return
            # 1) rename the folder on disk
            os.rename(old_folder, new_folder)
            # 2) update our own asset_path
            self.asset_path = os.path.join(new_folder,"info.json")
            # 3) tell main app to rename list entry
            if self.on_rename:
                self.on_rename(old_name, new_name)

            # 4) Now walk every other asset and update child_assets references:
            for root, dirs, files in os.walk(asset_root):
                if "info.json" not in files:
                    continue
                path = os.path.join(root, "info.json")
                with open(path, "r") as jf:
                    j = json.load(jf)
                changed = False

                # Update any child_assets entries
                for child in j.get("child_assets", []):
                    if child.get("asset") == old_name:
                        child["asset"] = new_name
                        changed = True

                # If this is a spacing page or any other that lists assets, you
                # could add other keys here likewise.

                if changed:
                    with open(path, "w") as jf:
                        json.dump(j, jf, indent=4)

        # --- now update this asset's own JSON fields below as before ---

        data["asset_name"] = new_name
        data["asset_type"] = self.type_var.get()
        anim = {
            "on_start":   "default",
            "on_end":     "default" if self.loop_var.get() else self.on_end_var.get(),
            "loop":       self.loop_var.get(),
            "audio_path": self.audio_path,
            "volume":     self.volume_var.get()
        }
        data["default_animation"] = anim
        if "default" not in data.get("available_animations", []):
            data.setdefault("available_animations",[]).append("default")
        if self.center_x is not None and self.center_y is not None:
            data["center"] = {"x": self.center_x, "y": self.center_y}

        # write JSON back
        with open(self.asset_path,"w") as f:
            json.dump(data, f, indent=4)

        # only overwrite default/ if user picked new frames
        if self._frames_changed and self.default_frames_path:
            dest = os.path.join(os.path.dirname(self.asset_path), "default")
            if os.path.exists(dest):
                shutil.rmtree(dest)
            shutil.copytree(self.default_frames_path, dest)

        messagebox.showinfo("Saved","Basic info saved.")
