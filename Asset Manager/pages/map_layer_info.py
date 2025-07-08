import os
import tkinter as tk
from tkinter import ttk
from colorsys import hsv_to_rgb

from pages.search_rooms import SearchRoomsFrame
from pages.room_info import RoomInfo


def level_to_hex_color(level):
    hue = (level * 0.13) % 1.0
    r, g, b = hsv_to_rgb(hue, 0.6, 1.0)
    return '#{:02x}{:02x}{:02x}'.format(int(r*255), int(g*255), int(b*255))


class MapLayerInfo(ttk.Frame):
    def __init__(self, parent, level, rooms_dir,
                 name="", radius=0, rooms_data=None,
                 save_callback=None, delete_callback=None):
        self.level = level
        self.rooms_dir = rooms_dir
        self.save = save_callback or (lambda: None)
        self.delete = delete_callback or (lambda _: None)
        self.rooms = []
        self.rooms_data = rooms_data or []

        bg_color = level_to_hex_color(level)
        self.bg_frame = tk.Frame(parent, background=bg_color, borderwidth=1, relief="solid")
        self.bg_frame.pack(side="left", fill="y", padx=6, pady=6)

        super().__init__(self.bg_frame, borderwidth=0, relief="flat", padding=6)
        self.pack(fill="both", expand=True)

        self.name_var = tk.StringVar(value=name)
        self.radius_var = tk.IntVar(value=radius)
        self.min_rooms_var = tk.IntVar(value=0)
        self.max_rooms_var = tk.IntVar(value=0)

        self._build_ui()
        self._load_rooms_data(self.rooms_data)

    def _build_ui(self):
        top_frame = ttk.Frame(self); top_frame.pack(fill="x", pady=2)
        ttk.Label(top_frame, text="Level:", font=("Segoe UI", 13, "bold")).pack(side="left")
        self.level_label = ttk.Label(top_frame, text=str(self.level), font=("Segoe UI", 13, "bold"))
        self.level_label.pack(side="left", padx=4)
        ttk.Button(top_frame, text="✕", width=2, command=self._on_delete).pack(side="right")

        name_frame = ttk.Frame(self); name_frame.pack(fill="x", pady=2)
        ttk.Label(name_frame, text="Name:").pack(side="left")
        entry = ttk.Entry(name_frame, textvariable=self.name_var)
        entry.pack(side="left", fill="x", expand=True, padx=4)
        self.name_var.trace_add('write', lambda *a: self.save())

        rad_frame = ttk.Frame(self); rad_frame.pack(fill="x", pady=2)
        ttk.Label(rad_frame, text="Radius:").pack(side="left")
        self.radius_label = ttk.Label(rad_frame, text=str(self.radius_var.get()))
        self.radius_label.pack(side="left", padx=4)

        room_limit_frame = ttk.Frame(self); room_limit_frame.pack(fill="x", pady=2)
        ttk.Label(room_limit_frame, text="Min Rooms:").pack(side="left")
        self.min_spin = tk.Spinbox(
            room_limit_frame, from_=0, to=9999,
            textvariable=self.min_rooms_var, width=5,
            state="readonly"
        )
        self.min_spin.pack(side="left", padx=4)

        ttk.Label(room_limit_frame, text="Max Rooms:").pack(side="left", padx=(10, 0))
        self.max_spin = tk.Spinbox(
            room_limit_frame, from_=0, to=9999,
            textvariable=self.max_rooms_var, width=5,
            command=self._on_max_rooms_changed
        )
        self.max_spin.pack(side="left", padx=4)
        self.max_spin.bind("<FocusOut>", lambda e: self._on_max_rooms_changed())
        self.max_spin.bind("<Return>", lambda e: self._on_max_rooms_changed())

        btn_frame = ttk.Frame(self); btn_frame.pack(fill="x", pady=2)
        ttk.Button(btn_frame, text="Add Room", command=self._on_add_room).pack(side="left")

        rooms_frame = ttk.LabelFrame(self, text="Rooms")
        rooms_frame.pack(fill="both", expand=True, pady=4)
        self.rooms_container = rooms_frame

    def _on_delete(self):
        self.bg_frame.destroy()
        self.delete(self)

    def _on_add_room(self):
        top = tk.Toplevel(self); top.title("Search Rooms")
        sr = SearchRoomsFrame(
            top,
            rooms_dir=self.rooms_dir,
            on_select=lambda nm: self._add_room_callback(nm, top)
        )
        sr.pack(fill="both", expand=True)

    def _add_room_callback(self, room_name, toplevel):
        widget = RoomInfo(
            self.rooms_container,
            room_name,
            min_instances=0,
            max_instances=0,
            required_children=[]
        )
        widget.pack(fill="x", pady=2, padx=4)

        # ✅ Ensure this triggers when min/max are changed
        widget.save = self._on_room_value_change
        widget.min_var.trace_add("write", lambda *a: widget.save())
        widget.max_var.trace_add("write", lambda *a: widget.save())

        self.rooms.append(widget)
        self._on_room_value_change()
        toplevel.destroy()

    def _on_room_value_change(self):
        # Called when any room min/max changes
        min_total = sum(r.min_var.get() for r in self.rooms)
        max_total = sum(r.max_var.get() for r in self.rooms)

        self.min_rooms_var.set(min_total)

        # Clamp max to be between min and total max
        cur_max = self.max_rooms_var.get()
        if cur_max < min_total or cur_max > max_total:
            self.max_rooms_var.set(max(min_total, min(cur_max, max_total)))

        self.max_spin.config(from_=min_total, to=max_total)
        self.save()

    def _on_max_rooms_changed(self):
        min_total = self.min_rooms_var.get()
        max_total = sum(r.max_var.get() for r in self.rooms)
        try:
            val = int(self.max_rooms_var.get())
        except ValueError:
            val = min_total
        val = max(min_total, min(val, max_total))
        self.max_rooms_var.set(val)
        self.save()

    def set_radius(self, radius):
        self.radius_var.set(radius)
        self.radius_label.config(text=str(radius))

    def set_level(self, new_level):
        self.level = new_level
        self.level_label.config(text=str(new_level))
        self.bg_frame.config(background=level_to_hex_color(new_level))

    def get_data(self):
        return {
            "level": self.level,
            "name": self.name_var.get(),
            "radius": self.radius_var.get(),
            "min_rooms": self.min_rooms_var.get(),
            "max_rooms": self.max_rooms_var.get(),
            "rooms": [r.get_data() for r in self.rooms]
        }

    def _load_rooms_data(self, rooms_data):
        for w in self.rooms:
            w.destroy()
        self.rooms.clear()

        for rd in rooms_data:
            name = rd.get("name")
            min_i = rd.get("min_instances", 0)
            max_i = rd.get("max_instances", 0)
            children = rd.get("required_children", [])

            widget = RoomInfo(
                self.rooms_container,
                name,
                min_instances=min_i,
                max_instances=max_i,
                required_children=children
            )
            widget.pack(fill="x", pady=2, padx=4)

            # ✅ Ensure changes to this room update the totals
            widget.save = self._on_room_value_change
            widget.min_var.trace_add("write", lambda *a: widget.save())
            widget.max_var.trace_add("write", lambda *a: widget.save())

            self.rooms.append(widget)

        self._on_room_value_change()

    def load(self, data):
        self.name_var.set(data.get("name", ""))
        self.set_radius(data.get("radius", 0))
        self.min_rooms_var.set(data.get("min_rooms", 0))
        self.max_rooms_var.set(data.get("max_rooms", 0))
        self._load_rooms_data(data.get("rooms", []))
