import os
import json
import tkinter as tk
from tkinter import ttk
from PIL import Image, ImageTk

SRC_DIR = "SRC"

class AssetSearchWindow(tk.Toplevel):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.title("Search Assets")
        self.geometry("500x600")
        self.selected_asset = None
        self.thumb_size = (64, 64)

        # Search input
        entry_frame = ttk.Frame(self)
        entry_frame.pack(fill=tk.X, padx=10, pady=10)

        ttk.Label(entry_frame, text="Search:").pack(side=tk.LEFT)
        self.query_var = tk.StringVar()
        entry = ttk.Entry(entry_frame, textvariable=self.query_var)
        entry.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=(5, 0))
        entry.bind("<KeyRelease>", self.update_results)

        # Scrollable result list
        canvas_frame = ttk.Frame(self)
        canvas_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=(0, 10))

        self.canvas = tk.Canvas(canvas_frame)
        self.scrollbar = ttk.Scrollbar(canvas_frame, orient="vertical", command=self.canvas.yview)
        self.canvas.configure(yscrollcommand=self.scrollbar.set)

        self.result_frame = ttk.Frame(self.canvas)
        self.canvas.create_window((0, 0), window=self.result_frame, anchor="nw")

        self.result_frame.bind("<Configure>", lambda e: self.canvas.configure(scrollregion=self.canvas.bbox("all")))

        self.canvas.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        self.scrollbar.pack(side=tk.RIGHT, fill=tk.Y)

        # Initial load
        self.assets = self._load_assets()
        self.update_results()

    def _load_assets(self):
        results = []
        if not os.path.isdir(SRC_DIR):
            return results
        for name in sorted(os.listdir(SRC_DIR)):
            asset_path = os.path.join(SRC_DIR, name)
            if not os.path.isdir(asset_path):
                continue
            info_path = os.path.join(asset_path, "info.json")
            if not os.path.isfile(info_path):
                continue
            try:
                with open(info_path, "r") as f:
                    info = json.load(f)
                asset_name = info.get("asset_name", name)
                tags = info.get("tags", [])
                results.append({"name": asset_name, "tags": tags, "path": asset_path})
            except Exception as e:
                print(f"[Search] Failed to read {info_path}: {e}")
        return results

    def update_results(self, event=None):
        query = self.query_var.get().lower()
        for widget in self.result_frame.winfo_children():
            widget.destroy()

        matches = []

        for asset in self.assets:
            name = asset["name"]
            tags = asset["tags"]
            if query in name.lower():
                matches.append({"score": 0, "asset": asset})
            elif any(query in tag.lower() for tag in tags):
                matches.append({"score": 1, "asset": asset})

        matches.sort(key=lambda x: x["score"])

        for match in matches:
            self._add_result_widget(match["asset"])

    def _add_result_widget(self, asset):
        frame = ttk.Frame(self.result_frame, relief="ridge", borderwidth=1, padding=5)
        frame.pack(fill=tk.X, pady=4)
        frame.bind("<Button-1>", lambda e, name=asset["name"]: self._on_select(name))

        # Image
        img_path = os.path.join(asset["path"], "default", "0.png")
        if os.path.isfile(img_path):
            try:
                img = Image.open(img_path)
                img.thumbnail(self.thumb_size)
                photo = ImageTk.PhotoImage(img)
                img_label = tk.Label(frame, image=photo)
                img_label.image = photo
                img_label.pack(side=tk.LEFT, padx=5)
                img_label.bind("<Button-1>", lambda e, name=asset["name"]: self._on_select(name))
            except Exception:
                pass

        # Name
        name_label = ttk.Label(frame, text=asset["name"], font=("Segoe UI", 12))
        name_label.pack(side=tk.LEFT, padx=10)
        name_label.bind("<Button-1>", lambda e, name=asset["name"]: self._on_select(name))

    def _on_select(self, asset_name):
        print(f"[Search] Selected asset: {asset_name}")
        self.selected_asset = asset_name
        self.destroy()

# Optional launcher
def launch_search():
    root = tk.Tk()
    root.withdraw()
    window = AssetSearchWindow()
    window.wait_window()
    return getattr(window, "selected_asset", None)

if __name__ == "__main__":
    selected = launch_search()
    print("Returned asset:", selected)
