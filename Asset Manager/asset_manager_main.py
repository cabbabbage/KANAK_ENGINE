import os
import json
import shutil
import tkinter as tk
from tkinter import ttk, messagebox
from PIL import Image, ImageTk
from pages.basic_info import BasicInfoPage
from pages.size import SizePage
from pages.Animations import AnimationsPage
from pages.passa import PassabilityPage
from pages.overlay import ImageOverlayPage
from pages.child import ChildAssetsPage
from pages.spacing import SpacingThresholdPage
from pages.tags import TagsPage
from pages.blend_page import BlendModePage
from tkinter import simpledialog  

ASSET_DIR = "SRC"

class AssetOrganizerApp(tk.Toplevel):
    def __init__(self):
        super().__init__()
        self.title("Asset Organizer")
        try:
            self.state('zoomed')
        except tk.TclError:
            self.attributes('-zoomed', True)

        style = ttk.Style(self)
        for w in ('TLabel','TButton','TCheckbutton','TEntry','TMenubutton','TSpinbox','TScale'):
            style.configure(w, foreground='black')

        self.assets = self._scan_assets()
        self.save_all_tags_to_csv()
        self.current_asset = None
        self.pages = {}
        self.asset_thumbnails = {}

        paned = ttk.Panedwindow(self, orient=tk.HORIZONTAL)
        paned.pack(fill=tk.BOTH, expand=True)

        left = ttk.Frame(paned, width=120)
        paned.add(left, weight=0)

        top_btns = ttk.Frame(left)
        top_btns.pack(fill=tk.X, pady=(8, 4), padx=5)

        asset_btn = tk.Button(top_btns, text="Switch to Map Manager", bg="#D9534F", fg="white",
                              font=("Segoe UI", 11, "bold"), command=self._open_asset_manager)
        asset_btn.pack(fill=tk.X, pady=(0, 4))

        new_btn = tk.Button(top_btns, text="New Asset", bg="#007BFF", fg="white", font=("Segoe UI", 11, "bold"),
                            command=self._new_asset)
        new_btn.pack(fill=tk.X, pady=(0, 6))

        self.asset_frame = tk.Canvas(left, borderwidth=0, highlightthickness=0, width=220)
        self.asset_scroll = ttk.Scrollbar(left, orient="vertical", command=self.asset_frame.yview)
        self.asset_frame.configure(yscrollcommand=self.asset_scroll.set)
        self.asset_frame.pack(side="left", fill="both", expand=True)
        self.asset_scroll.pack(side="right", fill="y")

        self.asset_inner = ttk.Frame(self.asset_frame)
        self.asset_window = self.asset_frame.create_window((0, 0), window=self.asset_inner, anchor="nw")
        self.asset_inner.bind("<Configure>", lambda e: self.asset_frame.configure(scrollregion=self.asset_frame.bbox("all")))
        self.asset_frame.bind_all("<MouseWheel>", self._on_mousewheel_assets)

        self.asset_buttons = {}

        right = ttk.Frame(paned)
        paned.add(right, weight=1)

        canvas = tk.Canvas(right)
        scrollbar = ttk.Scrollbar(right, orient="vertical", command=canvas.yview)
        canvas.configure(yscrollcommand=scrollbar.set)
        canvas.pack(side="left", fill="both", expand=True)
        scrollbar.pack(side="right", fill="y")

        scrollable_frame = ttk.Frame(canvas)
        scroll_window = canvas.create_window((0, 0), window=scrollable_frame, anchor="nw")

        def _on_mousewheel(event):
            canvas.yview_scroll(int(-1*(event.delta/120)), "units")

        scrollable_frame.bind("<Configure>", lambda e: canvas.configure(scrollregion=canvas.bbox("all")))
        scrollable_frame.bind_all("<MouseWheel>", _on_mousewheel)

        self.notebook = ttk.Notebook(scrollable_frame)
        self.notebook.pack(fill=tk.BOTH, expand=True)

        def add_page(cls, title):
            page = cls(self.notebook) if cls != AnimationsPage else cls(self.notebook, asset_folder=os.path.join(ASSET_DIR, self.current_asset or ""))
            self._add_tab(page, title)
            if cls == AnimationsPage:
                self.animations_tab = page

        add_page(BasicInfoPage,         "Basic Info")
        add_page(SizePage,              "Sizing")
        add_page(PassabilityPage,       "Passability")
        add_page(SpacingThresholdPage,  "Spacing")
        add_page(AnimationsPage,        "Animations")
        add_page(ChildAssetsPage,       "Region-Based Child Assets")
        add_page(TagsPage,              "Tags")
        add_page(BlendModePage,         "Blending")

        self._refresh_asset_list()

    def _on_mousewheel_assets(self, event):
        self.asset_frame.yview_scroll(int(-1*(event.delta/120)), "units")

    def _add_tab(self, page, title):
        self.notebook.add(page, text=title)
        self.pages[title] = page

    def save_all_tags_to_csv(self):
        from pages.tags import TagsPage
        for asset_name in self.assets:
            json_path = os.path.join(ASSET_DIR, asset_name, "info.json")
            if not os.path.isfile(json_path):
                continue
            try:
                tags_page = TagsPage(None)
                tags_page.load(json_path)
                tags_page._save_tags()
                print(f"[TagSave] Updated tags for: {asset_name}")
            except Exception as e:
                print(f"[TagSave] Failed for {asset_name}: {e}")

    def _scan_assets(self):
        if not os.path.isdir(ASSET_DIR):
            return []
        return [d for d in sorted(os.listdir(ASSET_DIR)) if os.path.isdir(os.path.join(ASSET_DIR, d))]

    def _refresh_asset_list(self):
        self.assets = self._scan_assets()
        for widget in self.asset_inner.winfo_children():
            widget.destroy()
        self.asset_buttons.clear()

        for name in self.assets:
            image_path = os.path.join(ASSET_DIR, name, "default", "0.png")
            photo = None
            if os.path.exists(image_path):
                try:
                    img = Image.open(image_path)
                    img.thumbnail((24, 24))
                    photo = ImageTk.PhotoImage(img)
                except:
                    pass
            btn = tk.Button(self.asset_inner, text=name, image=photo, compound="left", anchor="w",
                            width=180, command=lambda n=name: self._select_asset_by_name(n))
            btn.image = photo
            btn.pack(fill="x", padx=4, pady=1)
            self.asset_buttons[name] = btn

    def _select_asset_by_name(self, name):
        self.current_asset = name
        path = self._asset_json_path()

        for btn in self.asset_buttons.values():
            btn.configure(bg="#f0f0f0")
        self.asset_buttons[name].configure(bg="#CCE5FF")

        if hasattr(self, "animations_tab"):
            self.animations_tab.asset_folder = os.path.join(ASSET_DIR, self.current_asset)
            self.animations_tab._load_existing()

        for page in self.pages.values():
            page.load(path)

        self.notebook.select(self.pages["Basic Info"])

    def _asset_json_path(self):
        if not self.current_asset:
            return None
        return os.path.join(ASSET_DIR, self.current_asset, "info.json")

    def _copy_asset_name(self):
        if not self.current_asset:
            return
        self.clipboard_clear()
        self.clipboard_append(self.current_asset)

    def _duplicate_asset(self):
        if not self.current_asset:
            return
        src = self.current_asset
        src_folder = os.path.join(ASSET_DIR, src)
        i = 1
        while True:
            dst = f"{src}_{i}"
            dst_folder = os.path.join(ASSET_DIR, dst)
            if not os.path.exists(dst_folder):
                shutil.copytree(src_folder, dst_folder)
                break
            i += 1
        self._refresh_asset_list()
        messagebox.showinfo("Duplicated", f"{src} → {dst}")

    def _delete_asset(self):
        if not self.current_asset:
            return
        name = self.current_asset
        if not messagebox.askyesno("Delete", f"Really delete '{name}'?"):
            return
        shutil.rmtree(os.path.join(ASSET_DIR, name))
        self._refresh_asset_list()

    def _open_asset_manager(self):
        from map_manager_main import MapManagerApp
        MapManagerApp()
        self.destroy()

    def _new_asset(self):
        name = simpledialog.askstring("New Asset", "Enter a name for the new asset:")
        if not name or not name.strip():
            return
        name = name.strip()
        folder = os.path.join(ASSET_DIR, name)
        if os.path.exists(folder):
            messagebox.showerror("Error", f"Asset '{name}' already exists.")
            return

        os.makedirs(folder)
        default_json = {
            "impassable_area": None,
            "asset_name": name,
            "asset_type": "Object",
            "duplicatable": False,
            "duplication_interval_min": 30,
            "duplication_interval_max": 60,
            "min_child_depth": 0,
            "max_child_depth": 2
        }
        with open(os.path.join(folder, "info.json"), "w") as f:
            json.dump(default_json, f, indent=4)

        self._refresh_asset_list()
        self._select_asset_by_name(name)

if __name__ == "__main__":
    AssetOrganizerApp().mainloop()
