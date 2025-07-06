import os
import json
import tkinter as tk
from tkinter import ttk, messagebox, simpledialog
from pages.range import Range
from pages.assets_editor import AssetEditor
from pages.batch_asset_editor import BatchAssetEditor


class RoomsPage(ttk.Frame):
    def __init__(self, parent):
        super().__init__(parent)
        self.current_room_path = None
        self.room_data = None
        self.rooms_dir = None
        self._suspend_save = False

        # ─── Left pane: room list and Add Room button at bottom ────────────────
        left_frame = ttk.Frame(self)
        left_frame.pack(side=tk.LEFT, fill=tk.Y, padx=10, pady=10)

        self.room_list = tk.Listbox(left_frame, height=15)
        self.room_list.pack(side=tk.TOP, fill=tk.BOTH, expand=True)
        self.room_list.bind("<<ListboxSelect>>", self._on_select_room)

        add_room_btn = tk.Button(
            left_frame,
            text="Add Room",
            bg="#007BFF",
            fg="white",
            font=("Segoe UI", 11, "bold"),
            command=self._add_room,
            width=15
        )
        add_room_btn.pack(side=tk.BOTTOM, pady=(10, 0))

        # ─── Editor pane (initially hidden) ───────────────────────────────────
        self.editor_frame = ttk.Frame(self)
        self.editor_frame.pack_forget()

        self._build_editor()


    def load_data(self, data=None, json_path=None):
        if not json_path:
            return
        base_folder = os.path.dirname(json_path)
        self.rooms_dir = os.path.join(base_folder, "rooms")
        os.makedirs(self.rooms_dir, exist_ok=True)
        self._refresh_room_list()

    def _build_editor(self):
        self.name_var = tk.StringVar()
        ttk.Label(self.editor_frame, text="Room Name:").pack(anchor="w")
        name_entry = ttk.Entry(self.editor_frame, textvariable=self.name_var, state="readonly")
        name_entry.pack(fill=tk.X, pady=4)

        # Room Width slider + autosave on change
        self.width_range = Range(self.editor_frame, min_bound=700, max_bound=2500, label="Room Width")
        self.width_range.pack(fill=tk.X, pady=6)
        self.width_range.var_min.trace_add("write", lambda *a: self._on_field_change())
        self.width_range.var_max.trace_add("write", lambda *a: self._on_field_change())

        # Room Height slider + autosave on change
        self.height_range = Range(self.editor_frame, min_bound=700, max_bound=2500, label="Room Height")
        self.height_range.pack(fill=tk.X, pady=6)
        self.height_range.var_min.trace_add("write", lambda *a: self._on_field_change())
        self.height_range.var_max.trace_add("write", lambda *a: self._on_field_change())

        # Edge Smoothness slider + autosave on change
        self.edge_smoothness = Range(
            self.editor_frame, min_bound=0, max_bound=100,
            label="Edge Smoothness", force_fixed=True
        )
        self.edge_smoothness.pack(fill=tk.X, pady=6)
        # autosave whenever the fixed value changes
        self.edge_smoothness.var_min.trace_add("write", lambda *a: self._on_field_change())
        self.edge_smoothness.var_max.trace_add("write", lambda *a: self._on_field_change())


        # Geometry dropdown + autosave when changed
        ttk.Label(self.editor_frame, text="Geometry:").pack(anchor="w", pady=(8, 0))
        self.geometry_var = tk.StringVar()
        self.geometry_dropdown = ttk.Combobox(
            self.editor_frame, textvariable=self.geometry_var, state="readonly",
            values=["Random", "Circle", "Square"]
        )
        self.geometry_dropdown.pack(fill=tk.X, pady=2)
        self.geometry_dropdown.bind(
            "<<ComboboxSelected>>",
            lambda e: (self._on_field_change(), self._toggle_height_state())
)

        # Spawn / Boss checkboxes with exclusive logic + autosave
        self.spawn_var = tk.BooleanVar()
        self.boss_var = tk.BooleanVar()
        self._last_changed = "spawn"
        self.spawn_checkbox = ttk.Checkbutton(
            self.editor_frame, text="Is Spawn", variable=self.spawn_var,
            command=lambda: (self._checkbox_logic("spawn"), self._on_field_change())
        )
        self.spawn_checkbox.pack(anchor="w", pady=(6, 2))
        self.boss_checkbox = ttk.Checkbutton(
            self.editor_frame, text="Is Boss", variable=self.boss_var,
            command=lambda: (self._checkbox_logic("boss"), self._on_field_change())
        )
        self.boss_checkbox.pack(anchor="w", pady=(0, 6))

        # Basic Assets editor
        ttk.Label(self.editor_frame, text="Basic Assets", font=("Segoe UI", 11, "bold"))\
            .pack(anchor="w", pady=(10, 4))
        self.asset_editor = AssetEditor(
            self.editor_frame,
            get_asset_list=lambda: self.room_data["assets"],
            set_asset_list=lambda val: self.room_data.__setitem__("assets", val),
            save_callback=self._save_json,
            positioning=True,
            current_path=""
        )
        self.asset_editor.pack(fill=tk.BOTH, expand=True, padx=8, pady=(0, 20))
        self.asset_editor.config(width=600, height=600)

        # Batch Asset Editor
        ttk.Label(self.editor_frame, text="Batch Asset Editor", font=("Segoe UI", 11, "bold"))\
            .pack(anchor="w", pady=(0, 4))
        self.batch_editor = BatchAssetEditor(self.editor_frame, save_callback=self._save_json)
        self.batch_editor.pack(fill=tk.BOTH, expand=True, padx=8, pady=(0, 10))
        self.batch_editor.config(width=600, height=600)


    def _add_asset_to_editor(self):
        asset = {
            "name": "new_asset",
            "tag": False,
            "positioning": {"type": "Random"},
            "collisions": [],
            "interactions": [],
            "attacks": [],
            "children": []
        }
        self.room_data["assets"].append(asset)
        self.asset_editor.load_assets()
        self._save_json()

    def _on_field_change(self, *args):
        if not self._suspend_save:
            self._save_json()

    def _checkbox_logic(self, changed):
        self._last_changed = changed
        if self.spawn_var.get() and self.boss_var.get():
            if changed == "spawn":
                self.boss_var.set(False)
            else:
                self.spawn_var.set(False)

    def _refresh_room_list(self):
        self.room_list.delete(0, tk.END)
        if not self.rooms_dir:
            return
        os.makedirs(self.rooms_dir, exist_ok=True)
        for file in sorted(os.listdir(self.rooms_dir)):
            if file.endswith(".json"):
                try:
                    with open(os.path.join(self.rooms_dir, file)) as f:
                        data = json.load(f)
                        self.room_list.insert(tk.END, data.get("name", file[:-5]))
                except:
                    self.room_list.insert(tk.END, file[:-5])

    def _on_select_room(self, event):
        selection = self.room_list.curselection()
        if not selection or not self.rooms_dir:
            return

        # Save the previous room first (if one was loaded)
        if self.current_room_path and self.room_data:
            try:
                self.room_data["assets"] = self.asset_editor.get_assets()
                self.room_data["batch_assets"] = self.batch_editor.save()
                self.room_data["min_width"], self.room_data["max_width"] = self.width_range.get()
                self.room_data["min_height"], self.room_data["max_height"] = self.height_range.get()
                self.room_data["edge_smoothness"] = self.edge_smoothness.get()[0]
                self.room_data["geometry"] = self.geometry_var.get()
                self.room_data["is_spawn"] = self.spawn_var.get()
                self.room_data["is_boss"] = self.boss_var.get()
                self.room_data["inherits_map_assets"] = self.asset_editor.inherit_state

                with open(self.current_room_path, "w") as outf:
                    json.dump(self.room_data, outf, indent=2)
            except Exception as e:
                messagebox.showerror("Save Failed", f"Could not save previous room:\n{e}")

        # Get the newly selected room name and path
        name = self.room_list.get(selection[0])
        path = os.path.join(self.rooms_dir, f"{name}.json")
        if not os.path.exists(path):
            return

        try:
            with open(path) as inf:
                data = json.load(inf)

            # Ensure required keys exist
            if not isinstance(data.get("assets"), list):
                data["assets"] = []
            if not isinstance(data.get("batch_assets"), dict):
                data["batch_assets"] = {
                    "has_batch_assets": False,
                    "grid_spacing_min": 100,
                    "grid_spacing_max": 100,
                    "jitter_min": 0,
                    "jitter_max": 0,
                    "batch_assets": []
                }
            if "inherits_map_assets" not in data:
                data["inherits_map_assets"] = False

            self.room_data = data
            self.current_room_path = path

            self._load_editor()
            self.editor_frame.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=10)

        except Exception as e:
            messagebox.showerror("Error loading room", str(e))


    def _load_editor(self):
        # ─── Suspend autosaves while we populate everything ─────────────────────
        self._suspend_save = True

        # ─── 1) Load BatchAssetEditor first ────────────────────────────────────
        self.batch_editor.load(self.room_data.get("batch_assets", {}))

        # ─── 2) Populate simple room fields ───────────────────────────────────
        self.name_var.set(self.room_data.get("name", ""))
        self.width_range.set(
            self.room_data.get("min_width", 0),
            self.room_data.get("max_width", 0)
        )
        self.height_range.set(
            self.room_data.get("min_height", 0),
            self.room_data.get("max_height", 0)
        )
        self.edge_smoothness.set(
            self.room_data.get("edge_smoothness", 0),
            self.room_data.get("edge_smoothness", 0)
        )
        self.geometry_var.set(self.room_data.get("geometry", "Random"))
        self.spawn_var.set(self.room_data.get("is_spawn", False))
        self.boss_var.set(self.room_data.get("is_boss", False))

        # ─── 3) Load Basic AssetEditor ─────────────────────────────────────────
        self.asset_editor.current_path = self.current_room_path
        self.asset_editor.inherit_state = self.room_data.get("inherits_map_assets", False)
        self.asset_editor.inherit_var.set(self.asset_editor.inherit_state)
        self.asset_editor.load_assets()

        # ─── Re-enable autosaves ───────────────────────────────────────────────
        self._suspend_save = False




    def _toggle_height_state(self):
        """Grey out height when circle geometry is selected."""
        if self.geometry_var.get() == "Circle":
            self.height_range.disable()
        else:
            self.height_range.enable()


    def _add_room(self):
        name = simpledialog.askstring("New Room", "Enter room name:")
        if not name:
            return
        name = name.strip()
        safe_name = "".join(c for c in name if c.isalnum() or c in ("_", "-")).rstrip()
        path = os.path.join(self.rooms_dir, f"{safe_name}.json")
        if os.path.exists(path):
            messagebox.showerror("Error", f"A room named '{safe_name}' already exists.")
            return
        data = {
            "name": safe_name,
            "min_width": 100,
            "max_width": 200,
            "min_height": 100,
            "max_height": 200,
            "edge_smoothness": 0.5,
            "geometry": "Random",
            "is_spawn": False,
            "is_boss": False,
            "inherits_map_assets": False,
            "assets": [],
            "batch_assets": {}
        }

        with open(path, "w") as f:
            json.dump(data, f, indent=2)
        self._refresh_room_list()
        try:
            idx = self.room_list.get(0, tk.END).index(safe_name)
            self.room_list.selection_clear(0, tk.END)
            self.room_list.selection_set(idx)
            self._on_select_room(None)
        except ValueError:
            pass

    def _save_json(self):
        if not self.current_room_path or not self.room_data:
            return
        self.room_data["min_width"], self.room_data["max_width"] = self.width_range.get()
        self.room_data["min_height"], self.room_data["max_height"] = self.height_range.get()
        self.room_data["edge_smoothness"] = self.edge_smoothness.get()[0]
        self.room_data["geometry"] = self.geometry_var.get()
        self.room_data["is_spawn"] = self.spawn_var.get()
        self.room_data["is_boss"] = self.boss_var.get()
        self.room_data["inherits_map_assets"] = self.asset_editor.inherit_state
        self.room_data["batch_assets"] = self.batch_editor.save()
        try:
            with open(self.current_room_path, "w") as f:
                json.dump(self.room_data, f, indent=2)
        except Exception as e:
            messagebox.showerror("Save Failed", str(e))

    @staticmethod
    def get_json_filename():
        return "rooms.json"
