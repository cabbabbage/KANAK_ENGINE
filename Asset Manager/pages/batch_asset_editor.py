import tkinter as tk
from tkinter import ttk
from pages.range import Range
from pages.search import AssetSearchWindow
import math
import random

class BatchAssetEditor(ttk.Frame):
    def __init__(self, parent, save_callback=None):
        super().__init__(parent)

        self.save_callback = save_callback

        border = ttk.Frame(self, padding=1, relief="solid", borderwidth=1)
        border.pack(fill=tk.BOTH, expand=True)

        self.pie = []  # list of {name, percent, color, tag}
        self.slider = None
        self.remove_button = None
        self.selected_index = None
        self.colors = []
        self._generate_colors()

        self.grid_spacing = Range(border, label="Grid Spacing", min_bound=0, max_bound=400, set_min=100, set_max=100)
        self.grid_spacing.pack(fill=tk.X, padx=10, pady=4)
        self.grid_spacing.on_change = self._trigger_save

        self.jitter = Range(border, label="Jitter", min_bound=0, max_bound=200, set_min=0, set_max=0)
        self.jitter.pack(fill=tk.X, padx=10, pady=(0, 6))
        self.jitter.on_change = self._trigger_save

        self.canvas = tk.Canvas(border, height=360)
        self.canvas.pack(fill=tk.BOTH, expand=True, padx=10, pady=(0, 10))
        self.canvas.bind("<Double-Button-1>", self._on_double_click)

        button_frame = ttk.Frame(border)
        button_frame.pack(fill=tk.X, padx=10, pady=4)
        ttk.Button(button_frame, text="Add Asset", command=self._add_asset).pack(side=tk.LEFT)

        self._redraw()

    def _trigger_save(self):
        if self.save_callback:
            self.save_callback()

    def _generate_colors(self):
        for _ in range(100):
            self.colors.append("#%06x" % random.randint(0, 0xFFFFFF))

    def _add_asset(self):
        window = AssetSearchWindow(self)
        window.wait_window()
        result = getattr(window, "selected_result", None)
        if not result:
            return
        kind, name = result
        is_tag = (kind == "tag")
        if any(s["name"] == name and s.get("tag", False) == is_tag for s in self.pie):
            return

        if self.pie and self.pie[0]["name"] == "null" and len(self.pie) == 1:
            self.pie.clear()
            self.pie.append({
                "name": name,
                "tag": is_tag,
                "percent": 5,
                "color": self.colors[0]
            })
            self.pie.append({
                "name": "null",
                "tag": False,
                "percent": 95,
                "color": "#DDDDDD"
            })
        elif not self.pie:
            self.pie.append({
                "name": name,
                "tag": is_tag,
                "percent": 5,
                "color": self.colors[0]
            })
            self.pie.append({
                "name": "null",
                "tag": False,
                "percent": 95,
                "color": "#DDDDDD"
            })
        else:
            new_percent = 5
            self._adjust_existing(new_percent)
            self.pie.append({
                "name": name,
                "tag": is_tag,
                "percent": new_percent,
                "color": self.colors[len(self.pie) % len(self.colors)]
            })
            self._normalize_percentages()
        self._redraw()
        self._trigger_save()

    def _adjust_existing(self, remove_amount):
        total = sum(s["percent"] for s in self.pie)
        if total == 0:
            return
        for s in self.pie:
            share = s["percent"] / total
            s["percent"] -= share * remove_amount

    def _normalize_percentages(self):
        total = sum(s["percent"] for s in self.pie)
        if total == 0:
            return
        scaled = [round(s["percent"] * 100 / total) for s in self.pie]
        diff = 100 - sum(scaled)
        if scaled and diff != 0:
            max_index = max(range(len(scaled)), key=lambda i: scaled[i])
            scaled[max_index] += diff
        for i, s in enumerate(self.pie):
            s["percent"] = scaled[i]

    def _redraw(self):
        self.canvas.delete("all")
        if not self.pie:
            self.pie = [{"name": "null", "tag": False, "percent": 100, "color": "#DDDDDD"}]
        x, y, r = 220, 180, 120
        label_radius = r + 30
        angle = 0
        for i, s in enumerate(self.pie):
            extent = 360 * (s["percent"] / 100)
            self.canvas.create_arc(
                x - r, y - r, x + r, y + r,
                start=-angle - extent, extent=extent,
                fill=s["color"], outline="black", tags=("sector", str(i))
            )
            mid_angle_deg = angle + extent / 2
            mid_angle_rad = math.radians(-mid_angle_deg)
            edge_x = x + r * math.cos(mid_angle_rad)
            edge_y = y + r * math.sin(mid_angle_rad)
            label_x = x + label_radius * math.cos(mid_angle_rad)
            label_y = y + label_radius * math.sin(mid_angle_rad)
            self.canvas.create_line(edge_x, edge_y, label_x, label_y, fill="black")
            label_text = f"#{s['name']}" if s.get("tag", False) else s["name"]
            self.canvas.create_text(
                label_x, label_y,
                text=f"{label_text}\n{s['percent']}%",
                font=("Segoe UI", 9, "bold"),
                anchor="center"
            )
            angle += extent

    def _on_double_click(self, event):
        clicked = self.canvas.find_withtag("current")
        if not clicked:
            return
        tags = self.canvas.gettags(clicked[0])
        if "sector" not in tags:
            return
        index = int(tags[1])
        self._open_slider(index)

    def _open_slider(self, index):
        if self.slider:
            self.slider.destroy()
        if self.remove_button:
            self.remove_button.destroy()

        self.selected_index = index
        sector = self.pie[index]
        self.slider = Range(self, min_bound=1, max_bound=100, set_min=sector["percent"],
                            set_max=sector["percent"], force_fixed=True)
        self.slider.pack(fill=tk.X, padx=10, pady=(0, 4))

        self.remove_button = ttk.Button(self, text="✕", width=3, command=self._remove_selected)
        self.remove_button.pack(padx=10, pady=(0, 10))

        def on_change(*_):
            new_val = self.slider.get_min()
            self._recalculate_distribution(new_val)

        self.slider.var_max.trace_add("write", on_change)

    def _remove_selected(self):
        if self.selected_index is None:
            return
        del self.pie[self.selected_index]
        self.selected_index = None
        if self.slider:
            self.slider.destroy()
            self.slider = None
        if self.remove_button:
            self.remove_button.destroy()
            self.remove_button = None
        self._normalize_percentages()
        self._redraw()
        self._trigger_save()

    def _recalculate_distribution(self, new_val):
        rest = [i for i in range(len(self.pie)) if i != self.selected_index]
        if not rest:
            self.pie[self.selected_index]["percent"] = new_val
            self._redraw()
            self._trigger_save()
            return

        leftover = 100 - new_val
        current_total = sum(self.pie[i]["percent"] for i in rest)
        new_distribution = []
        for i in rest:
            if current_total == 0:
                new_distribution.append((i, leftover / len(rest)))
            else:
                ratio = self.pie[i]["percent"] / current_total
                new_distribution.append((i, leftover * ratio))

        rounded = [round(p) for _, p in new_distribution]
        total = sum(rounded)
        delta = 100 - new_val - total
        if rounded and delta != 0:
            max_index = max(range(len(rounded)), key=lambda i: rounded[i])
            rounded[max_index] += delta

        for (i, _), r in zip(new_distribution, rounded):
            self.pie[i]["percent"] = r
        self.pie[self.selected_index]["percent"] = new_val
        self._redraw()
        self._trigger_save()

    def save(self):
        return {
            "has_batch_assets": any(s["name"] != "null" for s in self.pie),
            "grid_spacing_min": self.grid_spacing.get_min(),
            "grid_spacing_max": self.grid_spacing.get_max(),
            "jitter_min": self.jitter.get_min(),
            "jitter_max": self.jitter.get_max(),
            "batch_assets": [
                {
                    "name": s["name"],
                    "tag": s.get("tag", False),
                    "percent": s["percent"]
                } for s in self.pie
            ]
        }


    def load(self, data=None):
        if not data:
            return
        self.grid_spacing.set(data.get("grid_spacing_min", 100), data.get("grid_spacing_max", 100))
        self.jitter.set(data.get("jitter_min", 0), data.get("jitter_max", 0))
        self.pie = data.get("batch_assets", [])

        for i, s in enumerate(self.pie):
            if "color" not in s:
                s["color"] = self.colors[i % len(self.colors)]

        if not self.pie:
            self.pie = [{"name": "null", "tag": False, "percent": 100, "color": "#DDDDDD"}]

        self._normalize_percentages()
        self._redraw()