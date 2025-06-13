import os
import json
import tkinter as tk
from tkinter import ttk, messagebox
import webbrowser
from pages.range import Range


class MapInfoPage(ttk.Frame):
    def __init__(self, parent):
        super().__init__(parent)
        self.data = {}
        self.current_path = None
        self.current_folder = None
        self.original_folder_name = None

        form = ttk.Frame(self)
        form.pack(fill=tk.X, padx=20, pady=10)

        self.name_var = tk.StringVar()

        self.width_range = Range(form, min_bound=10000, max_bound=50000, force_fixed=True, label="Map Width")
        self.height_range = Range(form, min_bound=10000, max_bound=50000, force_fixed=True, label="Map Height")
        self.min_room_range = Range(form, min_bound=0, max_bound=20, force_fixed=True, label="Min Rooms")
        self.max_room_range = Range(form, min_bound=0, max_bound=20, force_fixed=True, label="Max Rooms")

        self._add_entry(form, "Map Name:", self.name_var, 0)
        self._add_range(form, self.width_range, 1)
        self._add_range(form, self.height_range, 2)
        self._add_range(form, self.min_room_range, 3)
        self._add_range(form, self.max_room_range, 4)

        ttk.Label(self, text="Rooms", font=('Segoe UI', 14, 'bold')).pack(anchor="w", padx=20, pady=(20, 5))
        self.room_container = ttk.Frame(self)
        self.room_container.pack(fill=tk.BOTH, expand=True, padx=20)

        self.room_canvas = tk.Canvas(self.room_container, height=300)
        self.room_scroll = ttk.Scrollbar(self.room_container, orient="vertical", command=self.room_canvas.yview)
        self.room_list_frame = ttk.Frame(self.room_canvas)

        self.room_list_frame.bind("<Configure>", lambda e: self.room_canvas.configure(scrollregion=self.room_canvas.bbox("all")))
        self.room_canvas.create_window((0, 0), window=self.room_list_frame, anchor="nw")
        self.room_canvas.configure(yscrollcommand=self.room_scroll.set)
        self.room_canvas.pack(side="left", fill="both", expand=True)
        self.room_scroll.pack(side="right", fill="y")

        ttk.Label(self, text="Trails", font=('Segoe UI', 14, 'bold')).pack(anchor="w", padx=20, pady=(10, 5))
        self.trail_listbox = tk.Listbox(self, height=6, font=("Segoe UI", 10))
        self.trail_listbox.pack(fill=tk.BOTH, expand=False, padx=20, pady=(0, 20))

    def _add_entry(self, container, label, variable, row):
        ttk.Label(container, text=label, font=('Segoe UI', 10)).grid(row=row, column=0, sticky="w", pady=4, padx=(0, 10))
        entry = ttk.Entry(container, textvariable=variable, font=('Segoe UI', 10), state="readonly")
        entry.grid(row=row, column=1, sticky="ew", pady=4)
        container.columnconfigure(1, weight=1)

    def _add_range(self, container, widget, row):
        widget.grid(row=row, column=0, columnspan=2, sticky="ew", pady=4)
        # trace_add moved to end of load_data()

    def load_data(self, data, json_path=None):
        self.current_path = json_path
        self.current_folder = os.path.dirname(json_path)
        self.original_folder_name = os.path.basename(self.current_folder)

        if data is None and os.path.exists(json_path):
            with open(json_path, "r") as f:
                data = json.load(f)

        if not data or not isinstance(data, dict):
            data = {}

        self.data = data

        self.data.setdefault("map_name", self.original_folder_name)
        self.data.setdefault("map_width_", 10000)
        self.data.setdefault("map_height_", 10000)
        self.data.setdefault("min_rooms", 0)
        self.data.setdefault("max_rooms", 0)
        self.data.setdefault("map_assets", "map_assets.json")
        self.data.setdefault("map_boundary", "map_boundary.json")
        self.data.setdefault("rooms", [])
        self.data.setdefault("trails", [])

        self.name_var.set(self.data["map_name"])
        self.width_range.set(self.data["map_width_"], self.data["map_width_"])
        self.height_range.set(self.data["map_height_"], self.data["map_height_"])
        self.min_room_range.set(self.data["min_rooms"], self.data["min_rooms"])
        self.max_room_range.set(self.data["max_rooms"], self.data["max_rooms"])

        # === Deferred trace_add setup ===
        self.width_range.var_max.trace_add("write", lambda *_: self._save_json())
        self.height_range.var_max.trace_add("write", lambda *_: self._save_json())
        self.min_room_range.var_max.trace_add("write", lambda *_: self._save_json())
        self.max_room_range.var_max.trace_add("write", lambda *_: self._save_json())

        self.trail_listbox.delete(0, tk.END)
        for trail in self.data.get("trails", []):
            trail_path = trail.get("trail_path", "")
            name = os.path.splitext(os.path.basename(trail_path))[0]
            self.trail_listbox.insert(tk.END, name)

        self._refresh_rooms()
        self._save_json()

    def _refresh_rooms(self):
        for child in self.room_list_frame.winfo_children():
            child.destroy()

        for room in self.data.get("rooms", []):
            room_frame = ttk.Frame(self.room_list_frame, relief="solid", borderwidth=1, padding=4)
            room_frame.pack(fill="x", pady=4)

            name = os.path.splitext(os.path.basename(room.get("room_path", "")))[0]
            label = ttk.Label(room_frame, text=name, font=("Segoe UI", 10, "bold"))
            label.pack(anchor="w")

            max_instances = tk.IntVar(value=room.get("max_instances", 1))
            required = tk.BooleanVar(value=room.get("required", False))

            def make_saver(r=room, v=max_instances, b=required):
                return lambda *_: self._update_room_data(r, v.get(), b.get())

            row = ttk.Frame(room_frame)
            row.pack(fill=tk.X, pady=2)
            ttk.Label(row, text="Max Instances:").pack(side=tk.LEFT)
            entry = ttk.Entry(row, textvariable=max_instances, width=5)
            entry.pack(side=tk.LEFT, padx=4)
            entry.bind("<FocusOut>", make_saver())
            entry.bind("<Return>", make_saver())

            ttk.Checkbutton(row, text="Required", variable=required, command=make_saver()).pack(side=tk.LEFT, padx=8)

    def _update_room_data(self, room, max_instances, required):
        room["max_instances"] = max_instances
        room["required"] = required
        self._save_json()

    def _save_json(self):
        self.data["map_name"] = self.original_folder_name
        self.data["map_width_"] = self.width_range.get_max()
        self.data["map_height_"] = self.height_range.get_max()
        self.data["min_rooms"] = self.min_room_range.get_max()
        self.data["max_rooms"] = self.max_room_range.get_max()

        trails_dir = os.path.join(self.current_folder, "trails")
        if os.path.exists(trails_dir):
            trail_files = sorted(f for f in os.listdir(trails_dir) if f.endswith(".json"))
            self.data["trails"] = [{"trail_path": os.path.join("trails", f)} for f in trail_files]

        rooms_dir = os.path.join(self.current_folder, "rooms")
        if os.path.exists(rooms_dir):
            room_files = sorted(f for f in os.listdir(rooms_dir) if f.endswith(".json"))
            existing_room_paths = {r.get("room_path") for r in self.data.get("rooms", [])}
            new_rooms = []
            for f in room_files:
                path = os.path.join("rooms", f)
                match = next((r for r in self.data["rooms"] if r.get("room_path") == path), None)
                if match:
                    new_rooms.append(match)
                else:
                    new_rooms.append({"room_path": path, "max_instances": 1, "required": False})
            self.data["rooms"] = new_rooms

        with open(self.current_path, "w") as f:
            json.dump(self.data, f, indent=2)

    def get_data(self):
        return self.data

    @staticmethod
    def get_json_filename():
        return "map_info.json"


def setup_styles():
    style = ttk.Style()
    style.configure("SpawnRoom.TFrame", bordercolor="#FFD700", relief="solid", borderwidth=2)
    style.configure("BossRoom.TFrame", bordercolor="#FF4C4C", relief="solid", borderwidth=2)
