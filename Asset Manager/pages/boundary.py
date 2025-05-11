import os
import tkinter as tk
from tkinter import messagebox, ttk
from PIL import Image, ImageTk, ImageDraw, ImageFilter
import numpy as np

def union_mask_from_frames(frames):
    """Union of all non-transparent pixels across a list of RGBA PIL Images."""
    arrs = [np.array(frame.split()[-1]) > 0 for frame in frames]
    union = np.logical_or.reduce(arrs)
    mask = (union.astype(np.uint8) * 255)
    return Image.fromarray(mask)

class BoundaryConfigurator(tk.Toplevel):
    def __init__(self, master, base_folder, callback):
        super().__init__(master)
        self.title("Boundary Configurator")
        self.callback = callback

        # Theme & fonts
        self.FONT            = ('Segoe UI', 14)
        self.MAIN_COLOR      = "#005f73"
        self.SECONDARY_COLOR = "#ee9b00"
        self.BRUSH_RADIUS    = 10  # display-space brush size

        # Styles
        style = ttk.Style(self)
        style.configure('BC.TButton', font=self.FONT, padding=6,
                        background=self.MAIN_COLOR, foreground=self.SECONDARY_COLOR)
        style.map('BC.TButton', background=[('active', self.SECONDARY_COLOR)])
        style.configure('Large.TLabel', font=self.FONT)

        # Load frames
        try:
            files = sorted(f for f in os.listdir(base_folder) if f.lower().endswith('.png'))
        except OSError:
            messagebox.showerror("Error", f"Cannot access {base_folder}")
            self.destroy()
            return

        self.frames = []
        for fn in files:
            try:
                img = Image.open(os.path.join(base_folder, fn)).convert('RGBA')
                self.frames.append(img)
            except Exception:
                continue
        if not self.frames:
            messagebox.showerror("Error", "No PNGs found in folder")
            self.destroy()
            return

        # Compute display sizes & scaled images
        self.orig_w, self.orig_h = self.frames[0].size
        max_w, max_h = 800, 600
        self.scale = min(1.0, max_w/self.orig_w, max_h/self.orig_h)
        disp_w = int(self.orig_w * self.scale)
        disp_h = int(self.orig_h * self.scale)

        self.disp_base = self.frames[0].resize((disp_w, disp_h), Image.LANCZOS)
        union = union_mask_from_frames(self.frames)
        self.disp_mask = union.resize((disp_w, disp_h), Image.NEAREST)

        # Mode buttons
        btn_frame = ttk.Frame(self)
        btn_frame.pack(pady=5)
        for mode in ('mask', 'circle', 'draw'):
            btn = ttk.Button(btn_frame, text=mode.title(), style='BC.TButton',
                             command=lambda m=mode: self.select_mode(m))
            btn.pack(side='left', padx=5)

        # Canvas
        self.canvas = tk.Canvas(self, width=disp_w, height=disp_h)
        self.canvas.pack()
        self.tk_base = ImageTk.PhotoImage(self.disp_base)
        self.canvas.create_image(0, 0, anchor='nw', image=self.tk_base)
        self.overlay_item = None
        self.cursor_oval = None

        # State holders
        self.mode = None
        self.expand = 0
        self.sliders = {}
        self.circle = {}
        self.draw_mask = None

        # Next button
        next_btn = ttk.Button(self, text="Next", style='BC.TButton', command=self.finish)
        next_btn.pack(pady=10)

    def select_mode(self, mode):
        # Reset overlay, cursor, sliders, events
        self.mode = mode
        if self.overlay_item:
            self.canvas.delete(self.overlay_item)
            self.overlay_item = None
        if self.cursor_oval:
            self.canvas.delete(self.cursor_oval)
            self.cursor_oval = None
        for slider in self.sliders.values():
            slider.destroy()
        self.sliders.clear()
        self.expand = 0
        self.canvas.unbind("<B1-Motion>")
        self.canvas.unbind("<Button-1>")
        self.canvas.unbind("<Motion>")

        disp_w = int(self.orig_w * self.scale)
        disp_h = int(self.orig_h * self.scale)

        if mode == 'mask':
            self._refresh_mask_preview()
            ctrl = ttk.Frame(self)
            ctrl.pack(pady=10)
            ttk.Label(ctrl, text="Expand:", style='Large.TLabel').pack(side='left')
            ttk.Button(ctrl, text="-5", style='BC.TButton', command=lambda: self._change_expand(-5)).pack(side='left', padx=5)
            ttk.Button(ctrl, text="+5", style='BC.TButton', command=lambda: self._change_expand(5)).pack(side='left', padx=5)
            self._add_slider('top_pct', 0, 100, 0)
            self._add_slider('bottom_pct', 0, 100, 0)

        elif mode == 'circle':
            self.circle = {'w': disp_w//2, 'h': disp_h//2, 'x': disp_w//2, 'y': disp_h//2}
            self._draw_circle()
            for name, mn, mx, val in (('w',10,disp_w,self.circle['w']),
                                       ('h',10,disp_h,self.circle['h']),
                                       ('x',0,disp_w,self.circle['x']),
                                       ('y',0,disp_h,self.circle['y'])):
                self._add_slider(name, mn, mx, val)

        elif mode == 'draw':
            self.draw_mask = Image.new('L', (disp_w, disp_h), 0)
            self._refresh_draw_preview()
            self.canvas.bind("<B1-Motion>", self._on_draw)
            self.canvas.bind("<Button-1>", self._on_draw)
            self.canvas.bind("<Motion>", self._show_brush_cursor)
            self._add_slider('brush_size', 1, max(disp_w, disp_h)//2, self.BRUSH_RADIUS)

    def _change_expand(self, delta):
        self.expand = max(-self.orig_w, min(self.orig_w, self.expand + delta))
        self._refresh_mask_preview()

    def _refresh_mask_preview(self):
        disp_w = int(self.orig_w * self.scale)
        disp_h = int(self.orig_h * self.scale)
        size = abs(self.expand) * 2 + 1
        filt = None
        if self.expand > 0:
            filt = ImageFilter.MaxFilter(size)
        elif self.expand < 0:
            filt = ImageFilter.MinFilter(size)
        mask = self.disp_mask.filter(filt) if filt else self.disp_mask

        top = self.sliders.get('top_pct')
        bot = self.sliders.get('bottom_pct')
        y0 = int((top.get()/100) * disp_h) if top else 0
        y1 = int((1 - (bot.get()/100)) * disp_h) if bot else disp_h
        full_mask = Image.new('L', (disp_w, disp_h))
        cropped = mask.crop((0, y0, disp_w, y1))
        full_mask.paste(cropped, (0, y0))

        red_overlay = Image.new('RGBA', (disp_w, disp_h), (255,0,0,128))
        comp = self.disp_base.copy()
        comp.paste(red_overlay, (0,0), full_mask)

        if self.overlay_item:
            self.canvas.delete(self.overlay_item)
        self.tk_mask = ImageTk.PhotoImage(comp)
        self.overlay_item = self.canvas.create_image(0, 0, anchor='nw', image=self.tk_mask)

    def _refresh_draw_preview(self):
        disp_w = int(self.orig_w * self.scale)
        disp_h = int(self.orig_h * self.scale)
        red_overlay = Image.new('RGBA', (disp_w, disp_h), (255,0,0,128))
        comp = self.disp_base.copy()
        comp.paste(red_overlay, (0,0), self.draw_mask)

        if self.overlay_item:
            self.canvas.delete(self.overlay_item)
        self.tk_draw = ImageTk.PhotoImage(comp)
        self.overlay_item = self.canvas.create_image(0, 0, anchor='nw', image=self.tk_draw)

    def _on_draw(self, event):
        x = min(max(event.x, 0), int(self.orig_w*self.scale)-1)
        y = min(max(event.y, 0), int(self.orig_h*self.scale)-1)
        draw = ImageDraw.Draw(self.draw_mask)
        r = self.BRUSH_RADIUS
        draw.ellipse((x-r, y-r, x+r, y+r), fill=255)
        self._refresh_draw_preview()

    def _show_brush_cursor(self, event):
        if self.cursor_oval:
            self.canvas.delete(self.cursor_oval)
        x, y = event.x, event.y
        r = self.BRUSH_RADIUS
        self.cursor_oval = self.canvas.create_oval(x-r, y-r, x+r, y+r,
                                                 outline='white', width=1, dash=(2,))

    def _add_slider(self, name, mn, mx, val):
        frm = ttk.Frame(self)
        frm.pack(fill='x', padx=10, pady=4)
        ttk.Label(frm, text=name, style='Large.TLabel').pack(side='left')
        slider = tk.Scale(frm, from_=mn, to=mx, orient='horizontal',
                          command=lambda v, n=name: self._on_slider(n, int(v)))
        slider.set(val)
        slider.pack(side='left', fill='x', expand=True, padx=5)
        self.sliders[name] = slider

    def _on_slider(self, name, val):
        if self.mode == 'mask':
            self._refresh_mask_preview()
        elif self.mode == 'circle':
            self.circle[name] = val
            self._draw_circle()
        elif self.mode == 'draw' and name == 'brush_size':
            self.BRUSH_RADIUS = val

    def _draw_circle(self):
        disp_w = int(self.orig_w * self.scale)
        disp_h = int(self.orig_h * self.scale)
        mask = Image.new('L', (disp_w, disp_h))
        draw = ImageDraw.Draw(mask)
        w, h, x, y = (self.circle[k] for k in ('w','h','x','y'))
        draw.ellipse((x-w//2, y-h//2, x+w//2, y+h//2), fill=255)

        overlay = Image.new('RGBA', (disp_w, disp_h), (255,0,0,128))
        comp = self.disp_base.copy()
        comp.paste(overlay, (0,0), mask)

        if self.overlay_item:
            self.canvas.delete(self.overlay_item)
        self.tk_overlay = ImageTk.PhotoImage(comp)
        self.overlay_item = self.canvas.create_image(0, 0, anchor='nw', image=self.tk_overlay)

    def finish(self):
        pts = []
        if self.mode == 'mask':
            expand = self.expand
            size = abs(expand) * 2 + 1
            filt = ImageFilter.MaxFilter(size) if expand>0 else ImageFilter.MinFilter(size) if expand<0 else None
            mask_full = union_mask_from_frames(self.frames)
            mask_full = mask_full.filter(filt) if filt else mask_full
            top_pct = self.sliders['top_pct'].get() / 100
            bottom_pct = self.sliders['bottom_pct'].get() / 100
            ow, oh = mask_full.size
            y0 = int(top_pct * oh)
            y1 = int((1 - bottom_pct) * oh)
            mask_full = mask_full.crop((0, y0, ow, y1))
            arr = np.array(mask_full) > 0
            ys, xs = np.nonzero(arr)
            pts = [[int(x), int(y + y0)] for x, y in zip(xs, ys)]
        elif self.mode == 'circle':
            w = int(self.circle['w'] / self.scale)
            h = int(self.circle['h'] / self.scale)
            x = int(self.circle['x'] / self.scale)
            y = int(self.circle['y'] / self.scale)
            mask = Image.new('L', (self.orig_w, self.orig_h))
            draw = ImageDraw.Draw(mask)
            draw.ellipse((x-w//2, y-h//2, x+w//2, y+h//2), fill=255)
            arr = np.array(mask) > 0
            ys, xs = np.nonzero(arr)
            pts = [[int(x), int(y)] for x, y in zip(xs, ys)]
        elif self.mode == 'draw':
            dm = self.draw_mask.resize((self.orig_w, self.orig_h), Image.NEAREST)
            arr = np.array(dm) > 0
            ys, xs = np.nonzero(arr)
            pts = [[int(x), int(y)] for x, y in zip(xs, ys)]

        self.callback(pts)
        self.destroy()