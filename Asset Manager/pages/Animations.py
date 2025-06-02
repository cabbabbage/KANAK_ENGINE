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
        return scroll_frame

    def _build_buttons(self):
        footer = ttk.Frame(self)
        footer.pack(fill="x", side="bottom", pady=10)
        BlueButton(footer, "Add New Animation", command=self._prompt_new).pack(side="left", padx=20)
        BlueButton(footer, "Save All", command=self.save_all).pack(side="right", padx=20)

    def _load_existing(self):
        info_path = os.path.join(self.asset_folder, "info.json")
        if not os.path.isfile(info_path):
            return

        with open(info_path, "r") as f:
            data = json.load(f)
        animations = data.get("animations", {})

        for trigger, anim_data in animations.items():
            if trigger == "default" or anim_data is None:
                continue
            self._add_editor(trigger, anim_data)

    def _prompt_new(self):
        available = [t for t in self.triggers_df["trigger_name"] if t not in self.anim_configs]
        if not available:
            messagebox.showinfo("No triggers", "All trigger types already used.")
            return

        top = tk.Toplevel(self)
        top.title("Select Trigger Type")
        top.geometry("400x300")

        tk.Label(top, text="Choose a trigger type:").pack(padx=12, pady=10)
        listbox = tk.Listbox(top, height=min(10, len(available)), selectmode=tk.SINGLE)
        for item in available:
            listbox.insert(tk.END, item)
        listbox.pack(padx=12, pady=6, expand=True, fill="both")

        def on_select():
            selection = listbox.curselection()
            if selection:
                trigger = listbox.get(selection[0])
                self._add_editor(trigger, {})
                top.destroy()

        BlueButton(top, "Add", command=on_select, x=0, y=0)

    def _add_editor(self, trigger, anim_data):
        if trigger in self.anim_configs:
            return

        wrapper = ttk.Frame(self.scroll_frame)
        wrapper.pack(fill="x", pady=6, padx=6)

        def delete():
            wrapper.destroy()
            self.anim_configs.pop(trigger, None)

        tk.Button(wrapper, text="X", command=delete, fg="red").pack(side="left")

        editor = AnimationEditor(wrapper)
        editor.pack(fill="x", expand=True, padx=8)
        editor.load(trigger, anim_data, self.asset_folder)

        self.anim_configs[trigger] = editor

    def save_all(self):
        info_path = os.path.join(self.asset_folder, "info.json")
        data = {}
        if os.path.isfile(info_path):
            with open(info_path, "r") as f:
                data = json.load(f)

        data["animations"] = data.get("animations", {})

        for trigger, editor in self.anim_configs.items():
            data["animations"][trigger] = editor.save()

        data["available_animations"] = list(self.anim_configs.keys())

        with open(info_path, "w") as f:
            json.dump(data, f, indent=4)

        print("[AnimationsPage] Saved animations:", list(self.anim_configs.keys()))

    def save(self):
        self.save_all()

    def load(self, info_path):
        self.asset_folder = os.path.dirname(info_path)
        self._load_existing()
