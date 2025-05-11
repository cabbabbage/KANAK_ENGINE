# pages/anim_area_page.py

import os, json, csv, shutil
import tkinter as tk
from tkinter import ttk, filedialog, messagebox
from pages.boundary import BoundaryConfigurator
from PIL import Image, ImageTk, ImageDraw

class AnimAreaPage(ttk.Frame):
    """
    Generic page for any X‐animation+area: handles
      • is_enabled_var (BooleanVar)
      • frame folder selection
      • area configuration via BoundaryConfigurator
      • loop + on_end dropdown
      • optional audio
      • preview of default/0.png with overlay
      • JSON load/save of data under given keys
    """

    def __init__(self, parent,
                 *,
                 title,                     # window title
                 json_enabled_key,          # e.g. "is_interactable"
                 json_anim_key,             # e.g. "interaction_animation"
                 json_area_key,             # e.g. "interaction_area"
                 folder_name,               # subfolder to copy frames into e.g. "interaction"
                 label_singular,            # human label e.g. "Interactable"
                 ):
        super().__init__(parent)

        # store parameters
        self.title_text        = title
        self.enabled_key       = json_enabled_key
        self.anim_key          = json_anim_key
        self.area_key          = json_area_key
        self.folder_name       = folder_name
        self.label_singular    = label_singular
        self.FONT              = ('Segoe UI', 14)
        self.FONT_BOLD         = ('Segoe UI', 18, 'bold')
        self.MAIN_COLOR        = "#005f73"
        self.SECONDARY_COLOR   = "#ee9b00"
        self.DRAW_COLOR        = "#f94144"
        W = 20

        # STATE
        self.asset_path      = None
        self.frames_source   = None
        self.area_geo        = None
        self.audio_path      = ""
        # ← correct BooleanVar usage: pass `self` as master
        self.loop_var        = tk.BooleanVar(self, value=False)
        self.enabled_var     = tk.BooleanVar(self, value=False)
        self.on_end_var      = tk.StringVar(self, value="default")
        self.volume_var      = tk.IntVar(self, value=0)

        # — Styles —
        style = ttk.Style(self)
        style.configure('Main.TButton', font=self.FONT, padding=6,
                        background=self.MAIN_COLOR, foreground=self.SECONDARY_COLOR)
        style.map('Main.TButton',
                  background=[('active',self.SECONDARY_COLOR)],
                  foreground=[('active',self.MAIN_COLOR)])
        style.configure('Main.TMenubutton', font=self.FONT, padding=6,
                        background='white', foreground=self.SECONDARY_COLOR,
                        bordercolor=self.MAIN_COLOR, borderwidth=2, relief='solid')
        style.map('Main.TMenubutton',
                  background=[('active','white')],
                  foreground=[('active',self.SECONDARY_COLOR)])
        style.configure('Large.TLabel', font=self.FONT)
        style.configure('LargeBold.TLabel', font=self.FONT_BOLD,
                        foreground=self.SECONDARY_COLOR)
        style.configure('Large.TCheckbutton', font=self.FONT)
        style.configure('Link.TLabel', font=self.FONT,
                        foreground=self.SECONDARY_COLOR)

        # — Layout columns
        for c in range(5):
            self.columnconfigure(c, weight=1)

        # — Title —
        ttk.Label(self, text=self.title_text, style='LargeBold.TLabel')\
            .grid(row=0, column=0, columnspan=5, pady=(10,20), padx=10)

        # — Enabled? —
        ttk.Checkbutton(self, text=f"Is {self.label_singular}",
                        variable=self.enabled_var,
                        command=self._on_toggle,
                        style='Large.TCheckbutton')\
            .grid(row=1, column=0, columnspan=2,
                  sticky="w", padx=10, pady=6)

        # — Frames —
        ttk.Label(self, text=f"{self.label_singular} Frames:", style='Large.TLabel')\
            .grid(row=2, column=0, sticky="e", padx=10, pady=6)
        self.btn_select = ttk.Button(self, text="Select Frames",
                                     command=self.select_frames,
                                     style='Main.TButton', width=W)
        self.btn_select.grid(row=2, column=1, sticky="w", padx=10, pady=6)
        self.frames_label = ttk.Label(self, text="", style='Link.TLabel',
                                      cursor="hand2", width=W)
        self.frames_label.grid(row=2, column=2, sticky="w", padx=10, pady=6)
        self.frames_label.bind("<Button-1>", lambda e: self._open_folder(self.frames_source))

        # — Configure Area —
        ttk.Label(self, text=f"{self.label_singular} Area:", style='Large.TLabel')\
            .grid(row=3, column=0, sticky="e", padx=10, pady=6)
        self.btn_config = ttk.Button(self, text="Configure Area",
                                     command=self.configure_area,
                                     style='Main.TButton', width=W)
        self.btn_config.grid(row=3, column=1, sticky="w", padx=10, pady=6)
        self.area_label = ttk.Label(self, text="(none)", style='Link.TLabel')
        self.area_label .grid(row=3, column=2, columnspan=3,
                              sticky="w", padx=10, pady=6)

        # — Loop / On-End —
        ttk.Checkbutton(self, text="Loop",
                        variable=self.loop_var,
                        command=self._update_on_end_state,
                        style='Large.TCheckbutton')\
            .grid(row=4, column=0, sticky="e", padx=10, pady=6)
        ttk.Label(self, text="On-End Animation:", style='Large.TLabel')\
            .grid(row=4, column=1, sticky="e", padx=10, pady=6)
        self.on_end_menu = ttk.OptionMenu(self, self.on_end_var, "default")
        self.on_end_menu.config(style='Main.TMenubutton', width=W)
        self.on_end_menu.grid(row=4, column=2, sticky="w", padx=10, pady=6)

        # — Audio & Volume —
        ttk.Label(self, text="Audio (optional):", style='Large.TLabel')\
            .grid(row=5, column=0, sticky="e", padx=10, pady=6)
        self.btn_audio = ttk.Button(self, text="Select Audio",
                                    command=self.select_audio,
                                    style='Main.TButton', width=W)
        self.btn_audio.grid(row=5, column=1, sticky="w", padx=10, pady=6)
        self.audio_label = ttk.Label(self, text="", style='Link.TLabel',
                                     cursor="hand2", width=W)
        self.audio_label.grid(row=5, column=2, sticky="w", padx=10, pady=6)
        self.audio_label.bind("<Button-1>", lambda e: self._open_file(self.audio_path))
        ttk.Label(self, text="Audio Volume:", style='Large.TLabel')\
            .grid(row=5, column=3, sticky="e", padx=10, pady=6)
        self.spin_vol = tk.Spinbox(self, from_=0, to=100,
                                   textvariable=self.volume_var,
                                   font=self.FONT, width=5,
                                   fg=self.SECONDARY_COLOR)
        self.spin_vol.grid(row=5, column=4, sticky="w", padx=10, pady=6)

        # — Save —
        tk.Button(self, text="Save", command=self.save,
                  bg=self.SECONDARY_COLOR, fg=self.MAIN_COLOR,
                  font=self.FONT, width=W, padx=6, pady=6,
                  activebackground=self.SECONDARY_COLOR,
                  activeforeground=self.MAIN_COLOR)\
          .grid(row=6, column=0, columnspan=5, pady=(20,10), padx=10)

        # — Preview Canvas —
        self.preview = tk.Canvas(self, bg='black', bd=2, relief='sunken')
        self.preview.grid(row=7, column=0, columnspan=5, pady=10, padx=10)

        self._on_toggle()

    # — Utility methods below — #

    def _open_folder(self, path):
        if path and os.path.isdir(path):
            os.startfile(path)

    def _open_file(self, path):
        if path and os.path.isfile(path):
            os.startfile(path)

    def select_frames(self):
        d = filedialog.askdirectory()
        if not d: return
        self.frames_source = d
        self.frames_label.config(text=os.path.basename(d))

    def configure_area(self):
        if not self.frames_source:
            messagebox.showerror("Error", "Select frames first.")
            return
        BoundaryConfigurator(self, self.frames_source, callback=self._area_cb)

    def _area_cb(self, geo):
        # normalize to dict-with-points if needed...
        pts = geo if isinstance(geo, list) else geo.get("points", [])
        self.area_geo = {"type":"mask","points":pts}
        self.area_label.config(text="(configured)")
        self._draw_preview()

    def _update_on_end_state(self):
        state = 'disabled' if self.loop_var.get() else 'normal'
        self.on_end_menu.config(state=state)
        m = self.on_end_menu['menu']
        end = m.index('end')
        if end is None: return
        for i in range(end+1):
            m.entryconfig(i, state=state)

    def _on_toggle(self):
        """Enable/disable controls based on the enabled_var."""
        st = 'normal' if self.enabled_var.get() else 'disabled'
        # iterate over every widget that needs toggling
        for w in (
            self.btn_select, self.frames_label,
            self.btn_config, self.area_label,
            self.on_end_menu, self.btn_audio,
            self.audio_label, self.spin_vol
        ):
            # ttk widgets support .state(), tk widgets do not
            try:
                w.state([st])
            except (AttributeError, tk.TclError):
                # fallback for plain tk widgets
                try:
                    w.configure(state=st)
                except Exception:
                    # some labels might not accept state, ignore
                    pass


# pages/anim_area_page.py  (addition inside AnimAreaPage)

    def _draw_preview(self):
        """Load default/0.png at 50% and overlay the configured area."""
        if not self.asset_path or not self.area_geo:
            return

        # locate the default image
        base_dir = os.path.dirname(self.asset_path)
        default_png = os.path.join(base_dir, 'default', '0.png')
        if not os.path.isfile(default_png):
            self.preview.delete("all")
            return

        # load & downscale
        img = Image.open(default_png).convert('RGBA')
        w, h = img.size
        scale = 0.5
        disp = (int(w * scale), int(h * scale))
        base = img.resize(disp, Image.LANCZOS)

        # build overlay
        overlay = Image.new('RGBA', disp, (0,0,0,0))
        draw = ImageDraw.Draw(overlay)
        geo = self.area_geo

        # two possible formats: dict with type or raw list
        if isinstance(geo, dict):
            t = geo.get('type')
            if t == 'mask':
                pts = geo.get('points', [])
                for x, y in pts:
                    dx, dy = int(x*scale), int(y*scale)
                    if 0 <= dx < disp[0] and 0 <= dy < disp[1]:
                        overlay.putpixel((dx, dy),
                                         (255,0,0,128))
            elif t == 'circle':
                cx = int(geo['x']*scale)
                cy = int(geo['y']*scale)
                rw = int(geo['w']*scale)//2
                rh = int(geo['h']*scale)//2
                draw.ellipse((cx-rw, cy-rh, cx+rw, cy+rh),
                             fill=(255,0,0,128))
        else:
            # assume list of [x,y]
            for x, y in geo:
                dx, dy = int(x*scale), int(y*scale)
                if 0 <= dx < disp[0] and 0 <= dy < disp[1]:
                    overlay.putpixel((dx, dy),
                                     (255,0,0,128))

        # composite & display
        comp = Image.alpha_composite(base, overlay)
        self._tk_preview = ImageTk.PhotoImage(comp)
        self.preview.config(width=disp[0], height=disp[1])
        self.preview.delete("all")
        self.preview.create_image(0, 0, anchor='nw',
                                  image=self._tk_preview)

    def select_audio(self):
        file = filedialog.askopenfilename(
            filetypes=[("Audio","*.wav;*.mp3;*.ogg")]
        )
        if file:
            self.audio_path = file
            self.audio_label.config(text=os.path.basename(file))

    def open_audio_file(self, event):
        if self.audio_path and os.path.isfile(self.audio_path):
            os.startfile(self.audio_path)



    def load(self, info_path):
        """Load JSON keys, set up UI & preview."""
        self.asset_path = info_path
        if not info_path: return
        # ensure file exists
        if not os.path.exists(info_path):
            with open(info_path,"w"): pass
        data = json.load(open(info_path,"r"))

        # ensure defaults
        changed = False
        for k, default in [
            (self.enabled_key, False),
            (self.anim_key,      {"frames_path":"","on_end":"","loop":False,"audio_path":"","volume":0}),
            (self.area_key,      "")
        ]:
            if k not in data:
                data[k] = default
                changed = True
        if changed:
            with open(info_path,"w") as f:
                json.dump(data,f,indent=4)

        # populate
        self.enabled_var.set(data[self.enabled_key])
        anim = data[self.anim_key]
        fp = anim.get("frames_path","")
        self.frames_label.config(text=fp)
        if fp:
            full = os.path.join(os.path.dirname(info_path),fp)
            if os.path.isdir(full):
                self.frames_source = full

        area = data.get(self.area_key)
        if isinstance(area,str) and area:
            full = os.path.join(os.path.dirname(info_path),area)
            self.area_geo = json.load(open(full,"r"))
            self.area_label.config(text="(configured)")
        else:
            self.area_geo = None
            self.area_label.config(text="(none)")

        self.loop_var.set(anim.get("loop",False))
        self.available_anims = data.get("available_animations",[])
        if "default" not in self.available_anims:
            self.available_anims.insert(0,"default")
        if self.folder_name not in self.available_anims and self.enabled_var.get():
            self.available_anims.append(self.folder_name)
        m = self.on_end_menu["menu"]
        m.delete(0,'end')
        for a in self.available_anims:
            m.add_command(label=a, command=lambda v=a: self.on_end_var.set(v))
        self.on_end_var.set(anim.get("on_end","default"))

        self.audio_path = anim.get("audio_path","")
        self.audio_label.config(
            text=os.path.basename(self.audio_path) if self.audio_path else ""
        )
        self.volume_var.set(anim.get("volume",0))

        self._draw_preview()
        self._on_toggle()

    def save(self):
        """Write back JSON, copy frames, write area JSON."""
        if not self.asset_path:
            messagebox.showerror("Error","No asset selected.")
            return
        data = json.load(open(self.asset_path,"r"))
        base = os.path.dirname(self.asset_path)

        # enabled?
        data[self.enabled_key] = self.enabled_var.get()

        if not self.enabled_var.get():
            # reset animation & area
            data[self.anim_key] = {"frames_path":"","on_end":"","loop":False,"audio_path":"","volume":0}
            data[self.area_key] = ""
            if self.folder_name in data.get("available_animations",[]):
                data["available_animations"].remove(self.folder_name)

        else:
            # write anim block
            anim = {
                "frames_path": self.folder_name,
                "on_end":     self.folder_name if self.loop_var.get() else self.on_end_var.get(),
                "loop":       self.loop_var.get(),
                "audio_path": self.audio_path,
                "volume":     self.volume_var.get()
            }
            data[self.anim_key] = anim

            # write area JSON
            af = f"{self.folder_name}_area.json"
            with open(os.path.join(base,af),"w") as f:
                json.dump(self.area_geo,f,indent=4)
            data[self.area_key] = af

            # copy frames
            dest = os.path.join(base,self.folder_name)
            if os.path.exists(dest): shutil.rmtree(dest)
            shutil.copytree(self.frames_source,dest)

            if self.folder_name not in data["available_animations"]:
                data["available_animations"].append(self.folder_name)

        with open(self.asset_path,"w") as f:
            json.dump(data,f,indent=4)

        messagebox.showinfo("Saved",f"{self.title_text} saved.")
