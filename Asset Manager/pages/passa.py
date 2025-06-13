import os
import json
import tkinter as tk
from tkinter import ttk, messagebox
from pages.button import BlueButton
from pages.area_ui import AreaUI

class PassabilityPage(ttk.Frame):
    def __init__(self, parent):
        super().__init__(parent)
        self.asset_path = None
        self.is_passable_var = tk.BooleanVar(value=True)

        for c in range(3):
            self.columnconfigure(c, weight=1)

        ttk.Label(self, text="Passability Settings", style='LargeBold.TLabel')\
            .grid(row=0, column=0, columnspan=3, pady=(10, 20), padx=12)

        ttk.Checkbutton(self, text="Is Passable", variable=self.is_passable_var,
                        command=self._on_toggle, style='Large.TCheckbutton')\
            .grid(row=1, column=0, columnspan=3, sticky="w", padx=12, pady=6)

        json_rel_path = os.path.join("impassable_area.json")
        self.area_ui = AreaUI(self, json_rel_path)
        self.area_ui.grid(row=2, column=0, columnspan=3)

        BlueButton(self, "Save", command=self.save, x=0, y=0)

    def _on_toggle(self):
        is_passable = self.is_passable_var.get()
        new_state = "disabled" if is_passable else "normal"
        self.area_ui.btn_configure.config(state=new_state)
        self.area_ui.area_label.config(
            text="(undefaultricted)" if is_passable
            else ("(configured)" if self.area_ui.area_data else "(none)")
        )

        if not is_passable and self.area_ui.area_data:
            self.area_ui._draw_preview()
        else:
            self.area_ui.preview_canvas.delete("all")

        if self.asset_path and os.path.isfile(self.asset_path):
            with open(self.asset_path, 'r') as f:
                data = json.load(f)
            tags = set(data.get("tags", []))
            tags.discard("passable")
            tags.discard("impassable")
            tags.add("passable" if is_passable else "impassable")
            data["tags"] = list(tags)
            with open(self.asset_path, 'w') as f:
                json.dump(data, f, indent=4)

    def load(self, info_path):
        self.asset_path = info_path
        if not info_path:
            return

        asset_dir = os.path.dirname(info_path)
        if not os.path.exists(info_path):
            open(info_path, 'w').close()

        try:
            with open(info_path, 'r') as f:
                data = json.load(f)
        except json.JSONDecodeError:
            messagebox.showerror("Error", f"Malformed JSON: {info_path}")
            return

        tags = set(data.get("tags", []))
        if "passable" in tags:
            self.is_passable_var.set(True)
        elif "impassable" in tags:
            self.is_passable_var.set(False)
        else:
            self.is_passable_var.set(True)
            tags.add("passable")
            data["tags"] = list(tags)
            with open(info_path, 'w') as f:
                json.dump(data, f, indent=4)

        frames_path = os.path.join(asset_dir, "default")
        if os.path.isdir(frames_path):
            self.area_ui.frames_source = frames_path

        self.area_ui.json_path = os.path.join(asset_dir, "impassable_area.json")
        self.area_ui._load_area_json()
        self.area_ui._draw_preview()
        self._on_toggle()

    def save(self):
        if not self.asset_path:
            messagebox.showerror("Error", "No asset selected.")
            return

        asset_dir = os.path.dirname(self.asset_path)
        with open(self.asset_path, 'r') as f:
            info = json.load(f)

        tags = set(info.get("tags", []))
        tags.discard("passable")
        tags.discard("impassable")

        if self.is_passable_var.get():
            tags.add("passable")
            info['impassable_area'] = None
        else:
            tags.add("impassable")
            if not self.area_ui.area_data:
                messagebox.showerror("Error", "Configure an impassable area first.")
                return
            area_file = os.path.join(asset_dir, "impassable_area.json")
            with open(area_file, 'w') as f:
                json.dump(self.area_ui.area_data, f, indent=4)
            info['impassable_area'] = "impassable_area.json"

        info['tags'] = list(tags)
        with open(self.asset_path, 'w') as f:
            json.dump(info, f, indent=4)

        messagebox.showinfo("Saved", "Passability settings saved.")
