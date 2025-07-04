# === File: AnimationsPage.py ===
import os
import json
import tkinter as tk
from tkinter import ttk, messagebox
from pages.AnimationConfig import AnimationEditor
from pages.button import BlueButton
import pandas as pd
import csv

class AnimationsPage(ttk.Frame):
    def __init__(self, master, asset_folder):
        super().__init__(master)
        self.asset_folder = asset_folder
        self.anim_configs = {}

        self.triggers_df = pd.read_csv("Asset Manager/triggers.csv")
        self.used_triggers = set()

        # build scrollable UI and buttons
        self.scroll_frame = self._build_scrollable_ui()
        self._build_buttons()

        # load or initialize animations
        self._load_existing()

    def _build_scrollable_ui(self):
        container = ttk.Frame(self)
        container.pack(fill="both", expand=True)

        canvas = tk.Canvas(container)
        scrollbar = ttk.Scrollbar(container, orient="vertical", command=canvas.yview)
        scroll_frame = ttk.Frame(canvas)
        scroll_frame.bind(
            "<Configure>",
            lambda e: canvas.configure(scrollregion=canvas.bbox("all"))
        )

        canvas.create_window((0, 0), window=scroll_frame, anchor="nw")
        canvas.configure(yscrollcommand=scrollbar.set)
        canvas.pack(side="left", fill="both", expand=True)
        scrollbar.pack(side="right", fill="y")

        return scroll_frame

    def _build_buttons(self):
        footer = ttk.Frame(self)
        footer.pack(fill="x", side="bottom", pady=10)
        BlueButton(footer, "Add New Animation", command=self._add_empty_editor).pack(side="left", padx=20)
        BlueButton(footer, "Save All", command=self.save_all).pack(side="right", padx=20)

    def _load_existing(self):
        # clear existing editors
        for widget in self.scroll_frame.winfo_children():
            widget.destroy()
        self.anim_configs.clear()

        # load or create info.json
        info_path = os.path.join(self.asset_folder, "info.json")
        data = {}
        if os.path.isfile(info_path):
            with open(info_path, "r") as f:
                data = json.load(f)
        
        # ensure animations dict exists and includes default
        animations = data.get("animations")
        if not isinstance(animations, dict) or "default" not in animations:
            animations = data["animations"] = {"default": {"path": None, "end_path": None}}
            try:
                with open(info_path, "w") as f:
                    json.dump(data, f, indent=4)
            except Exception as e:
                messagebox.showerror("Error", f"Could not initialize animations: {e}")

        # create an editor for each animation trigger
        for trigger, anim_data in animations.items():
            # skip any malformed entries
            if not isinstance(anim_data, dict):
                anim_data = {"path": None, "end_path": None}
            self._add_editor(trigger, anim_data)

    def _add_empty_editor(self):
        # always allow default in dropdown
        self._add_editor("", {})

    def _add_editor(self, trigger, anim_data):
        temp_id = trigger or f"__temp_{len(self.anim_configs)}"
        if temp_id in self.anim_configs:
            return

        wrapper = ttk.Frame(self.scroll_frame)
        wrapper.pack(fill="x", pady=6, padx=6)

        # delete button for this editor
        def delete():
            wrapper.destroy()
            self.anim_configs.pop(temp_id, None)
        tk.Button(wrapper, text="X", command=delete, fg="red").pack(side="left")

        # animation editor widget
        editor = AnimationEditor(wrapper)
        editor.pack(fill="x", expand=True, padx=8)
        editor.load(trigger or "default", anim_data, self.asset_folder)

        self.anim_configs[temp_id] = editor

    def save_all(self):
        info_path = os.path.join(self.asset_folder, "info.json")
        data = {}
        if os.path.isfile(info_path):
            with open(info_path, "r") as f:
                data = json.load(f)

        # gather all saved animations
        animations_block = {}
        for temp_id, editor in self.anim_configs.items():
            saved = editor.save()
            real_trigger = editor.trigger_name.strip() or "default"
            if isinstance(saved, dict):
                animations_block[real_trigger] = saved

        # ensure default always present
        if "default" not in animations_block:
            animations_block["default"] = {"path": None, "end_path": None}

        data["animations"] = animations_block

        # add triggers to tags
        existing_tags = data.get("tags", [])
        for trigger in animations_block:
            if trigger not in existing_tags:
                existing_tags.append(trigger)
        data["tags"] = existing_tags

        # write back JSON
        try:
            with open(info_path, "w") as f:
                json.dump(data, f, indent=4)
        except Exception as e:
            messagebox.showerror("Save Failed", str(e))

        print("[AnimationsPage] Saved animations:", list(animations_block.keys()))

    def save(self):
        self.save_all()

    def load(self, info_path):
        # wrapper for page load
        self.asset_folder = os.path.dirname(info_path)
        self._load_existing()
