# === File 2: AnimationsPage.py ===
import os
import json
import tkinter as tk
from tkinter import ttk, messagebox
from pages.AnimationConfig import AnimationEditor
from pages.button import BlueButton
import pandas as pd

class AnimationsPage(ttk.Frame):
    def __init__(self, master, asset_folder):
        super().__init__(master)
        self.asset_folder = asset_folder
        self.anim_configs = {}

        self.triggers_df = pd.read_csv("Asset Manager/triggers.csv")
        self.used_triggers = set()

        self.scroll_frame = self._build_scrollable_ui()
        self._build_buttons()
        self._load_existing()

    def _build_scrollable_ui(self):
        container = ttk.Frame(self)
        container.pack(fill="both", expand=True)

        canvas = tk.Canvas(container)
        scrollbar = ttk.Scrollbar(container, orient="vertical", command=canvas.yview)
        scroll_frame = ttk.Frame(canvas)
        scroll_frame.bind("<Configure>", lambda e: canvas.configure(scrollregion=canvas.bbox("all")))

        canvas.create_window((0, 20), window=scroll_frame, anchor="nw")
        canvas.configure(yscrollcommand=scrollbar.set)
        canvas.pack(side="left", fill="both", expand=True)
        scrollbar.pack(side="right", fill="y")
        self.canvas = canvas
        self.scroll_frame = scroll_frame
        return scroll_frame

    def _build_buttons(self):
        footer = ttk.Frame(self)
        footer.pack(fill="x", side="bottom", pady=10)
        BlueButton(footer, "Add New Animation", command=self._add_empty_editor).pack(side="left", padx=20)
        BlueButton(footer, "Save All", command=self.save_all).pack(side="right", padx=20)

    def _load_existing(self):
        for widget in self.scroll_frame.winfo_children():
            widget.destroy()
        self.anim_configs.clear()

        info_path = os.path.join(self.asset_folder, "info.json")
        if not os.path.isfile(info_path):
            return

        with open(info_path, "r") as f:
            data = json.load(f)
        animations = data.get("animations", {})

        for trigger, anim_data in animations.items():
            if anim_data is None:
                continue
            self._add_editor(trigger, anim_data)


    def _add_empty_editor(self):
        self._add_editor("", {})

    def _add_editor(self, trigger, anim_data):
        temp_id = trigger or f"__temp_{len(self.anim_configs)}"
        if temp_id in self.anim_configs:
            return

        wrapper = ttk.Frame(self.scroll_frame)
        wrapper.pack(fill="x", pady=6, padx=6)

        def delete():
            wrapper.destroy()
            self.anim_configs.pop(temp_id, None)
            # Remove tag from info.json
            info_path = os.path.join(self.asset_folder, "info.json")
            if os.path.isfile(info_path):
                with open(info_path, "r") as f:
                    data = json.load(f)
                tags = data.get("tags", [])
                if trigger in tags:
                    tags.remove(trigger)
                    data["tags"] = tags
                    with open(info_path, "w") as f:
                        json.dump(data, f, indent=4)

        tk.Button(wrapper, text="X", command=delete, fg="red").pack(side="left")

        editor = AnimationEditor(wrapper)
        editor.pack(fill="x", expand=True, padx=8)
        editor.load(trigger, anim_data, self.asset_folder)

        self.anim_configs[temp_id] = editor


    def save_all(self):
        info_path = os.path.join(self.asset_folder, "info.json")
        data = {}
        if os.path.isfile(info_path):
            with open(info_path, "r") as f:
                data = json.load(f)

        animations_block = {}
        for temp_id, editor in self.anim_configs.items():
            saved = editor.save()
            real_trigger = editor.trigger_name.strip()
            if saved and real_trigger:
                animations_block[real_trigger] = saved

        data["animations"] = animations_block

        # Add triggers to tags if not already present
        existing_tags = data.get("tags", [])
        for trigger in animations_block:
            if trigger not in existing_tags:
                existing_tags.append(trigger)
        data["tags"] = existing_tags

        with open(info_path, "w") as f:
            json.dump(data, f, indent=4)

        print("[AnimationsPage] Saved animations:", list(animations_block.keys()))

    def save(self):
        self.save_all()

    def load(self, info_path):
        self.asset_folder = os.path.dirname(info_path)
        self._load_existing()
