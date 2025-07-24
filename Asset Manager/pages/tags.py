import os
import json
import tkinter as tk
from tkinter import ttk, messagebox
from collections import Counter, defaultdict
from pages.range import Range

SRC_DIR = "SRC"

class TagsPage(ttk.Frame):
    def __init__(self, parent):
        super().__init__(parent)
        self.data = {}
        self.json_path = None
        self.asset_tag_map = {}
        self.tag_usage = {}
        self.recommended_buttons = []
        self.anti_recommended_buttons = []

        ttk.Label(self, text="Tags (comma separated):").pack(anchor="w", padx=10, pady=(10, 0))
        self.text = tk.Text(self, height=4, wrap="word")
        self.text.pack(fill="x", padx=10, pady=5)

        ttk.Label(self, text="Anti-Tags (comma separated):").pack(anchor="w", padx=10, pady=(10, 0))
        self.anti_text = tk.Text(self, height=2, wrap="word")
        self.anti_text.pack(fill="x", padx=10, pady=(0, 10))

        save_btn = tk.Button(self, text="Save Tags", command=self._save_tags, bg="#007BFF", fg="white")
        save_btn.pack(pady=(0, 10))

        ttk.Label(self, text="Recommended Tags:", font=("Segoe UI", 10, "bold")).pack(anchor="w", padx=10)
        self.common_frame = tk.Canvas(self, height=300)
        self.scrollable_frame = ttk.Frame(self.common_frame)
        self.scrollbar = ttk.Scrollbar(self, orient="vertical", command=self.common_frame.yview)
        self.common_frame.configure(yscrollcommand=self.scrollbar.set)
        self.common_frame.create_window((0, 0), window=self.scrollable_frame, anchor="nw")
        self.scrollable_frame.bind("<Configure>", lambda e: self.common_frame.configure(scrollregion=self.common_frame.bbox("all")))
        self.common_frame.pack(fill="both", expand=True, padx=10, pady=(0, 10))
        self.scrollbar.pack(side="right", fill="y")

        ttk.Label(self, text="Recommended Anti-Tags:", font=("Segoe UI", 10, "bold")).pack(anchor="w", padx=10)
        self.anti_frame = ttk.Frame(self)
        self.anti_frame.pack(fill="both", expand=True, padx=10, pady=(0, 10))

        self.spawn_range = Range(self, min_bound=0, max_bound=1000, set_min=500, set_max=500, label="Spawn Range")
        self.spawn_range.pack(fill="x", padx=10, pady=(0, 10))

    def load(self, json_path):
        self.json_path = json_path
        if not os.path.exists(json_path):
            return
        with open(json_path) as f:
            self.data = json.load(f)

        tag_list = self.data.get("tags", [])
        anti_list = self.data.get("anti_tags", [])

        self.text.delete("1.0", tk.END)
        self.text.insert(tk.END, ", ".join(tag_list))
        self.anti_text.delete("1.0", tk.END)
        self.anti_text.insert(tk.END, ", ".join(anti_list))

        self.spawn_range.set(self.data.get("rnd_spawn_min", 300), self.data.get("rnd_spawn_max", 400))

        self._scan_asset_tags()
        self._refresh_recommended_tags()

    def _scan_asset_tags(self):
        self.asset_tag_map.clear()
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
                        cleaned = set(tag.strip().lower() for tag in tags if isinstance(tag, str))
                        if cleaned:
                            self.asset_tag_map[folder] = cleaned
                except:
                    continue

        tag_counter = Counter()
        for tags in self.asset_tag_map.values():
            tag_counter.update(tags)
        self.tag_usage = dict(tag_counter)

    def _get_current_tags(self):
        raw = self.text.get("1.0", tk.END).strip()
        return set(tag.strip().lower() for tag in raw.split(",") if tag.strip())

    def _get_current_anti_tags(self):
        raw = self.anti_text.get("1.0", tk.END).strip()
        return set(tag.strip().lower() for tag in raw.split(",") if tag.strip())

    def _find_similar_recommendations(self, limit=30, exclude=None):
        if exclude is None:
            exclude = set()
        current_tags = self._get_current_tags()
        tag_match_scores = defaultdict(int)
        for tags in self.asset_tag_map.values():
            shared = current_tags & tags
            if shared:
                for tag in tags:
                    if tag not in current_tags and tag not in exclude:
                        tag_match_scores[tag] += len(shared)
        prioritized = sorted(tag_match_scores.items(), key=lambda item: (-item[1], -self.tag_usage.get(item[0], 0)))
        recommended = [tag for tag, _ in prioritized]
        already = set(recommended) | current_tags | exclude
        fill = [tag for tag, _ in sorted(self.tag_usage.items(), key=lambda item: -item[1]) if tag not in already]
        final = recommended + fill
        return final[:limit]

    def _refresh_recommended_tags(self):
        for widget in self.recommended_buttons:
            widget.destroy()
        self.recommended_buttons.clear()
        for widget in self.anti_recommended_buttons:
            widget.destroy()
        self.anti_recommended_buttons.clear()

        current_tags = self._get_current_tags()
        current_anti_tags = self._get_current_anti_tags()

        tag_recs = self._find_similar_recommendations(limit=30, exclude=current_anti_tags)
        anti_recs = self._find_similar_anti_recommendations(limit=10, exclude=current_tags)

        for idx, tag in enumerate(tag_recs):
            btn = tk.Button(self.scrollable_frame, text=tag, width=20, height=2, font=("Segoe UI", 10, "bold"),
                            command=lambda t=tag: self._add_tag(t))
            btn.grid(row=idx // 5, column=idx % 5, padx=4, pady=4, sticky="w")
            self.recommended_buttons.append(btn)

        for idx, tag in enumerate(anti_recs):
            btn = tk.Button(self.anti_frame, text=tag, width=20, height=2, font=("Segoe UI", 10, "bold"),
                            command=lambda t=tag: self._add_anti_tag(t))
            btn.grid(row=idx // 5, column=idx % 5, padx=4, pady=4, sticky="w")
            self.anti_recommended_buttons.append(btn)


    def _find_similar_anti_recommendations(self, limit=10, exclude=None):
        if exclude is None:
            exclude = set()

        current_antis = self._get_current_anti_tags()
        current_tags = self._get_current_tags()
        full_exclude = current_antis | current_tags | exclude

        anti_tag_scores = defaultdict(int)

        for folder in os.listdir(SRC_DIR):
            folder_path = os.path.join(SRC_DIR, folder)
            if not os.path.isdir(folder_path):
                continue
            info_path = os.path.join(folder_path, "info.json")
            if os.path.exists(info_path):
                try:
                    with open(info_path) as f:
                        info = json.load(f)
                        anti_tags = info.get("anti_tags", [])
                        anti_tags = set(tag.strip().lower() for tag in anti_tags if isinstance(tag, str))
                        overlap = current_antis & anti_tags
                        if overlap:
                            for tag in anti_tags:
                                if tag not in full_exclude:
                                    anti_tag_scores[tag] += len(overlap)
                except:
                    continue

        prioritized = sorted(anti_tag_scores.items(), key=lambda item: (-item[1], -self.tag_usage.get(item[0], 0)))
        recommended = [tag for tag, _ in prioritized]

        already = set(recommended) | full_exclude
        fill = [tag for tag, _ in sorted(self.tag_usage.items(), key=lambda item: -item[1]) if tag not in already]

        final = recommended + fill
        return final[:limit]



    def _add_tag(self, tag):
        tags = self._get_current_tags()
        if tag not in tags:
            tags.add(tag)
            self.text.delete("1.0", tk.END)
            self.text.insert("1.0", ", ".join(sorted(tags)))
            self._refresh_recommended_tags()

    def _add_anti_tag(self, tag):
        tags = self._get_current_anti_tags()
        if tag not in tags:
            tags.add(tag)
            self.anti_text.delete("1.0", tk.END)
            self.anti_text.insert("1.0", ", ".join(sorted(tags)))
            self._refresh_recommended_tags()

    def remove_tag(self, tag):
        tags = self._get_current_tags()
        if tag in tags:
            tags.remove(tag)
            self.text.delete("1.0", tk.END)
            self.text.insert("1.0", ", ".join(sorted(tags)))
            self._refresh_recommended_tags()

    def remove_anti_tag(self, tag):
        tags = self._get_current_anti_tags()
        if tag in tags:
            tags.remove(tag)
            self.anti_text.delete("1.0", tk.END)
            self.anti_text.insert("1.0", ", ".join(sorted(tags)))
            self._refresh_recommended_tags()

    def _save_tags(self):
        if not self.json_path:
            return
        raw_tags = self.text.get("1.0", tk.END).strip()
        raw_antis = self.anti_text.get("1.0", tk.END).strip()
        self.data["tags"] = [t.strip() for t in raw_tags.split(",") if t.strip()]
        self.data["anti_tags"] = [t.strip() for t in raw_antis.split(",") if t.strip()]
        self.data["rnd_spawn_min"] = self.spawn_range.get_min()
        self.data["rnd_spawn_max"] = self.spawn_range.get_max()
        try:
            with open(self.json_path, "w") as f:
                json.dump(self.data, f, indent=2)
        except Exception as e:
            messagebox.showerror("Save Failed", str(e))
