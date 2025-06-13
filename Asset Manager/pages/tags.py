import os
import json
import tkinter as tk
from tkinter import ttk, messagebox
from collections import Counter
from pages.range import Range

SRC_DIR = "SRC"

HARDCODED_TAGS = [
    # Nature Types
    "tree", "rock", "log", "fern", "bush", "mushroom", "vines", "roots", "stump", "wildflower", "bone",

    # Textures
    "mossy", "muddy", "wet", "bark", "rough", "soft", "slick", "cracked", "spongy",
    "gritty", "smooth", "coarse", "weathered", "crumbling",

    # Colors
    "green", "brown", "gray", "golden", "deep", "pale", "shadowed", "dappled", "dark", "sunlit",

    # Density / Size
    "dense", "overgrown", "leafy", "fallen",

    # Locations
    "canopy", "undergrowth", "path", "cliffside", "clearing",
    "marsh", "grove", "glade", "swamp", "thicket", "bluff", "ridge", "ravine", "wetland", "meadow"
]

class TagsPage(ttk.Frame):
    def __init__(self, parent):
        super().__init__(parent)
        self.data = {}
        self.json_path = None

        ttk.Label(self, text="Tags (comma separated):").pack(anchor="w", padx=10, pady=(10, 0))

        self.text = tk.Text(self, height=4, wrap="word")
        self.text.pack(fill="x", padx=10, pady=5)

        save_btn = tk.Button(self, text="Save Tags", command=self._save_tags, bg="#007BFF", fg="white")
        save_btn.pack(pady=(0, 10))

        ttk.Label(self, text="Common Tags:", font=("Segoe UI", 10, "bold")).pack(anchor="w", padx=10)

        self.common_frame = tk.Canvas(self, height=200)
        self.scrollable_frame = ttk.Frame(self.common_frame)
        self.scrollbar = ttk.Scrollbar(self, orient="vertical", command=self.common_frame.yview)
        self.common_frame.configure(yscrollcommand=self.scrollbar.set)

        self.common_frame.create_window((0, 0), window=self.scrollable_frame, anchor="nw")
        self.scrollable_frame.bind("<Configure>", lambda e: self.common_frame.configure(scrollregion=self.common_frame.bbox("all")))

        self.common_frame.pack(fill="both", expand=True, padx=10, pady=(0, 10))
        self.scrollbar.pack(side="right", fill="y")

        self.tag_usage = {}  # tag -> usage count

        self.spawn_range = Range(self, min_bound=0, max_bound=1000, set_min=500, set_max=500, label="Spawn Range")
        self.spawn_range.pack(fill="x", padx=10, pady=(0, 10))

    def load(self, json_path):
        self.json_path = json_path
        if not os.path.exists(json_path):
            return
        with open(json_path) as f:
            self.data = json.load(f)

        tag_list = self.data.get("tags", [])
        self.text.delete("1.0", tk.END)
        self.text.insert(tk.END, ", ".join(tag_list))

        self.spawn_range.set(self.data.get("rnd_spawn_min", 300), self.data.get("rnd_spawn_max", 400))

        self._load_common_tags()

    def _load_common_tags(self):
        self.tag_usage.clear()
        for widget in self.scrollable_frame.winfo_children():
            widget.destroy()

        tag_counter = Counter()
        for folder in os.listdir(SRC_DIR):
            folder_path = os.path.join(SRC_DIR, folder)
            if not os.path.isdir(folder_path):
                continue
            info_path = os.path.join(folder_path, "info.json")
            if os.path.exists(info_path):
                try:
                    with open(info_path) as f:
                        info = json.load(f)
                        tags = info.get("tags", [])
                        tag_counter.update([tag.strip().lower() for tag in tags])
                except:
                    continue

        self.tag_usage = dict(tag_counter)
        all_tags_sorted = sorted(self.tag_usage.items(), key=lambda item: item[1], reverse=True)
        used_tags = set(self.tag_usage)
        for tag in HARDCODED_TAGS:
            if tag not in used_tags:
                all_tags_sorted.append((tag, 0))

        cols = 5
        for idx, (tag, _) in enumerate(all_tags_sorted):
            btn = ttk.Button(self.scrollable_frame, text=tag, width=18, command=lambda t=tag: self._add_tag(t))
            row = idx // cols
            col = idx % cols
            btn.grid(row=row, column=col, padx=2, pady=2, sticky="w")

    def _add_tag(self, tag):
        current = self.text.get("1.0", tk.END).strip()
        tags = [t.strip() for t in current.split(",") if t.strip()]
        if tag not in tags:
            tags.append(tag)
            self.text.delete("1.0", tk.END)
            self.text.insert("1.0", ", ".join(tags))
    
    def remove_tag(self, tag):
        current = self.text.get("1.0", tk.END).strip()
        tags = [t.strip() for t in current.split(",") if t.strip()]
        if tag in tags:
            tags.remove(tag)
            self.text.delete("1.0", tk.END)
            self.text.insert("1.0", ", ".join(tags))


    def _save_tags(self):
        if not self.json_path:
            return

        raw = self.text.get("1.0", tk.END).strip()
        new_tags = [t.strip() for t in raw.split(",") if t.strip()]
        self.data["tags"] = new_tags

        self.data["rnd_spawn_min"] = self.spawn_range.get_min()
        self.data["rnd_spawn_max"] = self.spawn_range.get_max()

        try:
            with open(self.json_path, "w") as f:
                json.dump(self.data, f, indent=2)
        except Exception as e:
            messagebox.showerror("Save Failed", str(e))

        # === Update tag_map.csv ===
        asset_folder = os.path.dirname(self.json_path)
        parent_dir = os.path.abspath(os.path.join(asset_folder, ".."))
        tag_map_path = os.path.join(parent_dir, "tag_map.csv")

        asset_name = os.path.basename(asset_folder)
        if not asset_name:
            print("[Tag Save] Could not determine asset name from folder path.")
            return

        import csv

        # Read the existing CSV into a dict: tag -> list of assets
        tag_rows = {}  # tag : set of asset names
        if os.path.exists(tag_map_path):
            with open(tag_map_path, newline='') as f:
                reader = csv.reader(f)
                for row in reader:
                    if not row: continue
                    tag = row[0].strip()
                    assets = [cell.strip() for cell in row[1:] if cell.strip()]
                    tag_rows[tag] = set(assets)

        # Make sure each tag in new_tags is represented
        for tag in new_tags:
            if tag not in tag_rows:
                tag_rows[tag] = set()
            tag_rows[tag].add(asset_name)

        # Remove asset name from tags it no longer has
        for tag, assets in list(tag_rows.items()):
            if tag not in new_tags:
                assets.discard(asset_name)
            if not assets:
                del tag_rows[tag]  # Delete tag row entirely if empty

        # Sort by tag name
        sorted_rows = sorted(tag_rows.items(), key=lambda x: x[0].lower())

        # Write it back
        with open(tag_map_path, "w", newline='') as f:
            writer = csv.writer(f)
            for tag, assets in sorted_rows:
                row = [tag] + sorted(assets)
                writer.writerow(row)

        self._load_common_tags()
