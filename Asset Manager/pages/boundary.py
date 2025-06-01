import os
import tkinter as tk
from tkinter import ttk, messagebox
from PIL import Image, ImageTk, ImageDraw, ImageFilter
import numpy as np
import scipy.ndimage as ndi
import math

def union_mask_from_frames(frames):
    arrs = [np.array(frame.split()[-1]) > 0 for frame in frames]
    union = np.logical_or.reduce(arrs)
    mask = (union.astype(np.uint8) * 255)
    return Image.fromarray(mask)

class BoundaryConfigurator(tk.Toplevel):
    def __init__(self, master, base_folder, callback):
        super().__init__(master)
        self.title("Boundary Configurator")
        self.callback = callback
        self.state('zoomed')

        self.FONT = ('Segoe UI', 14)
        self.MAIN_COLOR = "#005f73"
        self.SECONDARY_COLOR = "#ee9b00"
        self.BRUSH_RADIUS = 10

        style = ttk.Style(self)
        style.configure('BC.TButton', font=self.FONT, padding=6,
                        background=self.MAIN_COLOR, foreground=self.SECONDARY_COLOR)
        style.map('BC.TButton', background=[('active', self.SECONDARY_COLOR)])
        style.configure('Large.TLabel', font=self.FONT)

        # Load PNG frames
        files = sorted(f for f in os.listdir(base_folder) if f.lower().endswith('.png'))
        self.frames = [Image.open(os.path.join(base_folder, f)).convert('RGBA') for f in files]
        if not self.frames:
            self.destroy()
            return

        self.orig_w, self.orig_h = self.frames[0].size
        self.scale = 1.0
        self._calculate_display_size()

        # Layout grid config
        self.grid_columnconfigure(0, weight=1)
        self.grid_rowconfigure(1, weight=1)

        # Canvas (must come before offset update)
        self.canvas = tk.Canvas(self, bg='black')
        self.canvas.bind("<Configure>", lambda e: self._on_resize())
        self.canvas.grid(row=1, column=0, sticky='nsew')

        self.x_offset = 0
        self.y_offset = 0
        self._update_offsets()

        # Pre-render scaled image and mask
        self.disp_base = self.frames[0].resize((self.disp_w, self.disp_h), Image.LANCZOS)
        union = union_mask_from_frames(self.frames)
        self.disp_mask = union.resize((self.disp_w, self.disp_h), Image.NEAREST)
        self._mask_arr = np.array(self.disp_mask) > 0
        self._preview_job = None

        # Display base image
        self.tk_base = ImageTk.PhotoImage(self.disp_base)
        self.image_id = self.canvas.create_image(self.x_offset, self.y_offset, anchor='nw', image=self.tk_base)

        # Top toolbar: mode + zoom
        ctrl_row = ttk.Frame(self)
        ctrl_row.grid(row=0, column=0, sticky='ew', pady=5)
        for mode in ('mask', 'circle', 'draw'):
            ttk.Button(ctrl_row, text=mode.title(), style='BC.TButton',
                       command=lambda m=mode: self.select_mode(m)).pack(side='left', padx=10)

        ttk.Label(ctrl_row, text="Zoom:", style='Large.TLabel').pack(side='left', padx=(20, 0))
        self.zoom_var = tk.DoubleVar(value=0.2)

        zoom_slider = tk.Scale(ctrl_row, from_=0.01, to=0.5, resolution=0.01,
                               variable=self.zoom_var, orient='horizontal',
                               command=self._on_zoom_change, length=300)
        zoom_slider.pack(side='left', padx=5)

        # Bottom "Next" button
        btn_next = ttk.Button(self, text="Next", style='BC.TButton', command=self.finish)
        btn_next.grid(row=2, column=0, pady=10)

        self._on_zoom_change()
        self.after(100, lambda: self.select_mode("draw"))
        self.after(200, lambda: self.select_mode("draw"))

        # State
        self.mode = None
        self.expand = 0
        self.sliders = {}
        self.circle = {}
        self.draw_mask = None
        self.overlay_item = None
        self.cursor_oval = None

    def _calculate_display_size(self):
        self.disp_w = int(self.orig_w * self.scale * 2)
        self.disp_h = int(self.orig_h * self.scale * 2)

    def _update_offsets(self):
        canvas_w = self.canvas.winfo_width()
        canvas_h = self.canvas.winfo_height()
        self.x_offset = max((canvas_w - self.disp_w) // 2, 0)
        self.y_offset = max((canvas_h - self.disp_h) // 2, 0)

    def _on_resize(self):
        self._update_offsets()
        self.canvas.coords(self.image_id, self.x_offset, self.y_offset)
        if self.overlay_item:
            self.canvas.coords(self.overlay_item, self.x_offset, self.y_offset)
        if self.cursor_oval:
            self.canvas.delete(self.cursor_oval)
            self.cursor_oval = None

    def _on_zoom_change(self, _val=None):
        self.scale = self.zoom_var.get()
        self._calculate_display_size()
        self._update_offsets()

        self.disp_base = self.frames[0].resize((self.disp_w, self.disp_h), Image.LANCZOS)
        union = union_mask_from_frames(self.frames)
        self.disp_mask = union.resize((self.disp_w, self.disp_h), Image.NEAREST)
        self._mask_arr = np.array(self.disp_mask) > 0

        self.canvas.delete("all")
        self.tk_base = ImageTk.PhotoImage(self.disp_base)
        self.image_id = self.canvas.create_image(self.x_offset, self.y_offset, anchor='nw', image=self.tk_base)

    def select_mode(self, mode):
        self.mode = mode
        if self.overlay_item:
            self.canvas.delete(self.overlay_item)
            self.overlay_item = None
        if self.cursor_oval:
            self.canvas.delete(self.cursor_oval)
            self.cursor_oval = None
        for s in self.sliders.values():
            s.destroy()
        self.sliders.clear()
        self.expand = 0
        self.canvas.unbind("<B1-Motion>")
        self.canvas.unbind("<Button-1>")
        self.canvas.unbind("<Motion>")

        if mode == 'mask':
            self._schedule_mask_preview()
            # Add all four mask-trim sliders:
            self._add_slider('top_pct', 0, 100, 0)
            self._add_slider('bottom_pct', 0, 100, 0)
            self._add_slider('left_pct', 0, 100, 0)
            self._add_slider('right_pct', 0, 100, 0)

        elif mode == 'circle':
            self.circle = {
                'w': self.disp_w // 2,
                'h': self.disp_h // 2,
                'x': self.disp_w // 2,
                'y': self.disp_h // 2
            }
            self._draw_circle()
            for name in ('w', 'h', 'x', 'y'):
                self._add_slider(name, 10, max(self.disp_w, self.disp_h), self.circle[name])

        elif mode == 'draw':
            draw_w = self.disp_w * 2
            draw_h = self.disp_h * 2
            self.draw_mask = Image.new('L', (draw_w, draw_h), 0)
            self.draw_center_x = (draw_w - self.disp_w) // 2
            self.draw_center_y = (draw_h - self.disp_h) // 2
            self._refresh_draw_preview()
            self.canvas.bind("<B1-Motion>", self._on_draw)
            self.canvas.bind("<Button-1>", self._on_draw)
            self.canvas.bind("<Motion>", self._show_brush_cursor)
            self._add_slider('brush_size', 1, max(draw_w, draw_h) // 2, self.BRUSH_RADIUS)

    def _refresh_draw_preview(self):
        red_overlay = Image.new('RGBA', self.draw_mask.size, (255, 0, 0, 128))
        base_expanded = Image.new('RGBA', self.draw_mask.size)
        base_expanded.paste(self.disp_base, (self.draw_center_x, self.draw_center_y))
        comp = base_expanded.copy()
        comp.paste(red_overlay, (0, 0), self.draw_mask)
        self.tk_draw = ImageTk.PhotoImage(comp)
        self.canvas.delete("all")
        self.overlay_item = self.canvas.create_image(
            self.x_offset - self.draw_center_x,
            self.y_offset - self.draw_center_y,
            anchor='nw',
            image=self.tk_draw
        )

    def _schedule_mask_preview(self):
        if self._preview_job:
            self.after_cancel(self._preview_job)
        self._preview_job = self.after(50, self._refresh_mask_preview)

    def _draw_circle(self):
        mask = Image.new('L', (self.disp_w, self.disp_h))
        draw = ImageDraw.Draw(mask)
        w, h, x, y = (self.circle[k] for k in ('w', 'h', 'x', 'y'))
        draw.ellipse((x - w // 2, y - h // 2, x + w // 2, y + h // 2), fill=255)
        overlay = Image.new('RGBA', (self.disp_w, self.disp_h), (255, 0, 0, 128))
        comp = self.disp_base.copy()
        comp.paste(overlay, (0, 0), mask)
        self.tk_overlay = ImageTk.PhotoImage(comp)
        self.canvas.delete("all")
        self.overlay_item = self.canvas.create_image(self.x_offset, self.y_offset, anchor='nw', image=self.tk_overlay)

    def _refresh_mask_preview(self):
        self._preview_job = None
        size = abs(self.expand) * 2 + 1
        arr = self._mask_arr
        if self.expand > 0:
            arr2 = ndi.grey_dilation(arr.astype(np.uint8), size=(size, size)) > 0
        elif self.expand < 0:
            arr2 = ndi.grey_erosion(arr.astype(np.uint8), size=(size, size)) > 0
        else:
            arr2 = arr.copy()

        # Read all four sliders:
        top = self.sliders.get('top_pct', tk.Scale()).get() / 100
        bot = self.sliders.get('bottom_pct', tk.Scale()).get() / 100
        left = self.sliders.get('left_pct', tk.Scale()).get() / 100
        right = self.sliders.get('right_pct', tk.Scale()).get() / 100

        h0, w0 = arr2.shape
        y0 = int(top * h0)
        y1 = int((1 - bot) * h0)
        x0 = int(left * w0)
        x1 = int((1 - right) * w0)

        arr2[:y0, :] = False
        arr2[y1:, :] = False
        arr2[:, :x0] = False
        arr2[:, x1:] = False

        overlay = np.zeros((h0, w0, 4), dtype=np.uint8)
        overlay[arr2, 0] = 255
        overlay[arr2, 3] = 128
        ov_img = Image.fromarray(overlay, 'RGBA').resize((self.disp_w, self.disp_h), Image.NEAREST)
        comp = self.disp_base.copy()
        comp.paste(ov_img, (0, 0), ov_img)
        self.tk_mask = ImageTk.PhotoImage(comp)
        self.canvas.delete("all")
        self.overlay_item = self.canvas.create_image(self.x_offset, self.y_offset, anchor='nw', image=self.tk_mask)

    def _on_draw(self, event):
        draw_w, draw_h = self.draw_mask.size
        center_x = (draw_w - self.disp_w) // 2
        center_y = (draw_h - self.disp_h) // 2
        x = event.x - (self.x_offset - center_x)
        y = event.y - (self.y_offset - center_y)
        if x < 0 or y < 0 or x >= draw_w or y >= draw_h:
            return
        draw = ImageDraw.Draw(self.draw_mask)
        r = self.BRUSH_RADIUS
        draw.ellipse((x - r, y - r, x + r, y + r), fill=255)
        self._refresh_draw_preview()

    def _show_brush_cursor(self, event):
        if self.cursor_oval:
            self.canvas.delete(self.cursor_oval)
        x = event.x
        y = event.y
        r = self.BRUSH_RADIUS
        self.cursor_oval = self.canvas.create_oval(
            x - r, y - r, x + r, y + r,
            outline='white', width=1, dash=(2,)
        )

    def _add_slider(self, name, mn, mx, val):
        frm = ttk.Frame(self)
        frm.grid(row=3 + len(self.sliders), column=0, sticky='ew', padx=10, pady=4)
        ttk.Label(frm, text=name, style='Large.TLabel').pack(side='left')

        def on_slider_change(v):
            value = int(v)
            if self.mode == 'circle' and name in self.circle:
                self.circle[name] = value
                self._draw_circle()
            elif self.mode == 'mask' and name in ('top_pct', 'bottom_pct', 'left_pct', 'right_pct'):
                self._schedule_mask_preview()
            elif self.mode == 'draw' and name == 'brush_size':
                self.BRUSH_RADIUS = value

        slider = tk.Scale(frm, from_=mn, to=mx, orient='horizontal', command=on_slider_change)
        slider.set(val)
        slider.pack(side='left', fill='x', expand=True, padx=5)
        self.sliders[name] = slider

    def _extract_edge_pixels(self, mask_image):
        """Given an L-mode PIL image at original resolution, return a list of Python‐int (x,y) points."""
        edges = mask_image.filter(ImageFilter.FIND_EDGES)
        arr = np.array(edges) > 0  # boolean array of shape (orig_h, orig_w)
        pts = list(zip(*np.nonzero(arr)[::-1]))  # these may be numpy.int64
        # Convert each to (int, int):
        return [(int(x), int(y)) for (x, y) in pts]

    def _get_draw_mode_points(self):
        """Return perimeter points for DRAW mode (based on self.draw_mask)."""
        mask_full = self.draw_mask.resize((self.orig_w, self.orig_h), Image.NEAREST)
        return self._extract_edge_pixels(mask_full)

    def _get_mask_mode_points(self):
        """Return perimeter points for MASK mode, applying expand + 4 trim sliders."""
        union = union_mask_from_frames(self.frames)
        arr = np.array(union) > 0  # boolean array

        size = abs(self.expand) * 2 + 1
        if self.expand > 0:
            arr = ndi.grey_dilation(arr.astype(np.uint8), size=(size, size)) > 0
        elif self.expand < 0:
            arr = ndi.grey_erosion(arr.astype(np.uint8), size=(size, size)) > 0

        top_frac = float(self.sliders.get('top_pct', tk.Scale()).get()) / 100.0
        bottom_frac = float(self.sliders.get('bottom_pct', tk.Scale()).get()) / 100.0
        left_frac = float(self.sliders.get('left_pct', tk.Scale()).get()) / 100.0
        right_frac = float(self.sliders.get('right_pct', tk.Scale()).get()) / 100.0

        h0, w0 = arr.shape
        y0 = int(top_frac * h0)
        y1 = int((1 - bottom_frac) * h0)
        x0 = int(left_frac * w0)
        x1 = int((1 - right_frac) * w0)

        arr[:y0, :] = False
        arr[y1:, :] = False
        arr[:, :x0] = False
        arr[:, x1:] = False

        mask_full = Image.fromarray((arr.astype(np.uint8) * 255))
        return self._extract_edge_pixels(mask_full)

    def _get_circle_mode_points(self):
        """
        Return only the perimeter points for an ellipse defined by the four
        sliders: self.circle['x'], ['y'], ['w'], ['h'] in DISPLAY coords.
        We:
          1. Draw the filled ellipse onto a temporary L-mode image at (disp_w, disp_h)
             using the exact same logic as the preview (_draw_circle).
          2. Resize that “disp” mask back to (orig_w, orig_h) with NEAREST.
          3. Extract edge pixels from that scaled‐up mask.
        """
        # --- 1) Create a blank L-mode at display size and draw ellipse there ---
        mask_disp = Image.new('L', (self.disp_w, self.disp_h), 0)
        dd = ImageDraw.Draw(mask_disp)

        w_disp = int(self.circle['w'])
        h_disp = int(self.circle['h'])
        x_disp = int(self.circle['x'])
        y_disp = int(self.circle['y'])

        # The preview uses exactly: draw.ellipse((x - w//2, y - h//2, x + w//2, y + h//2), fill=255)
        dd.ellipse((x_disp - w_disp // 2,
                    y_disp - h_disp // 2,
                    x_disp + w_disp // 2,
                    y_disp + h_disp // 2),
                   fill=255)

        # --- 2) Nearest‐neighbour scale up from (disp_w,disp_h) to (orig_w,orig_h) ---
        mask_full = mask_disp.resize((self.orig_w, self.orig_h), Image.NEAREST)

        # --- 3) Extract true edge pixels from the scaled mask ---
        return self._extract_edge_pixels(mask_full)

    def finish(self):
        if not self.mode:
            messagebox.showerror("Error", "No mode selected")
            return

        # 1) Get raw perimeter points in original‐resolution coordinates
        if self.mode == 'draw':
            raw_pts = self._get_draw_mode_points()
        elif self.mode == 'mask':
            raw_pts = self._get_mask_mode_points()
        elif self.mode == 'circle':
            raw_pts = self._get_circle_mode_points()
        else:
            messagebox.showerror("Error", f"Unsupported mode: {self.mode}")
            return

        if not raw_pts:
            messagebox.showerror("Error", "No valid boundary points found.")
            return

        # 2) RDP‐simplify (still int tuples)
        def rdp(points, epsilon=2.0):
            if len(points) < 3:
                return list(points)
            a, b = points[0], points[-1]
            ax, ay, bx, by = a[0], a[1], b[0], b[1]
            max_dist, index = 0.0, -1
            for i, (px, py) in enumerate(points[1:-1], start=1):
                if (ax, ay) == (bx, by):
                    d = math.hypot(px - ax, py - ay)
                else:
                    num = (px - ax) * (bx - ax) + (py - ay) * (by - ay)
                    den = (bx - ax)**2 + (by - ay)**2 + 1e-9
                    t = num / den
                    if t < 0: t = 0
                    elif t > 1: t = 1
                    proj_x = ax + t * (bx - ax)
                    proj_y = ay + t * (by - ay)
                    d = math.hypot(px - proj_x, py - proj_y)
                if d > max_dist:
                    max_dist, index = d, i
            if max_dist <= epsilon:
                return [a, b]
            left = rdp(points[: index + 1], epsilon)
            right = rdp(points[index:], epsilon)
            return left[:-1] + right

        simplified = rdp(raw_pts, epsilon=2.0)

        # 3) Determine the bottom‐center anchor for a “bottom_center” scheme
        #    The image’s width = self.orig_w, height = self.orig_h.
        #    Bottom‐center pixel coordinate is at (orig_w/2, orig_h).
        anchor_x = self.orig_w / 2.0
        anchor_y = float(self.orig_h)

        # 4) Convert each simplified point into an offset from bottom‐center
        offset_points = []
        for (x, y) in simplified:
            dx = x - anchor_x
            dy = y - anchor_y
            offset_points.append([int(round(dx)), int(round(dy))])

        # 5) Build the base JSON payload
        json_data = {
            "points": offset_points,
            "anchor": "bottom_center",
            "original_anchor": [int(round(anchor_x)), int(round(anchor_y))],
            "original_dimensions": [int(self.orig_w), int(self.orig_h)],
            "type": self.mode,
        }

        # 6) If circle mode, also record the ellipse parameters relative to bottom‐center
        if self.mode == "circle":
            # In display coords, self.circle['x'], self.circle['y'] are the ellipse center.
            # We must convert them back to original‐resolution, then subtract bottom‐center.
            factor = self.scale * 2.0
            x_disp = float(self.circle['x'])
            y_disp = float(self.circle['y'])
            w_disp = float(self.circle['w'])
            h_disp = float(self.circle['h'])

            # Convert display→original
            cx_orig = int(round(x_disp / factor))
            cy_orig = int(round(y_disp / factor))
            # Radii in display coords → diameters in original coords:
            diameter_w = int(round((w_disp / factor) * 2.0))
            diameter_h = int(round((h_disp / factor) * 2.0))

            # Now make each relative to bottom‐center:
            rel_cx = cx_orig - anchor_x
            rel_cy = cy_orig - anchor_y

            json_data["x"] = int(round(rel_cx))
            json_data["y"] = int(round(rel_cy))
            json_data["w"] = diameter_w
            json_data["h"] = diameter_h

        # 7) Invoke callback + close
        try:
            self.callback(json_data)
            if hasattr(self.master, "save"):
                self.master.save()
        except KeyError as e:
            messagebox.showerror("Error", f"Failed to save boundary data: missing key {e}")
        except Exception as e:
            messagebox.showerror("Error", f"Failed to save boundary data:\n{e}")
        finally:
            self.destroy()
