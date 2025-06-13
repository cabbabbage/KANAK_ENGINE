import os
import json
import tkinter as tk
from tkinter import ttk, simpledialog, messagebox

from pages.map_info_page     import MapInfoPage
from pages.rooms_page        import RoomsPage
from pages.trails_page       import TrailsPage
from pages.boundary_page     import BoundaryPage
from pages.map_assets_page   import MapAssetsPage

MAPS_DIR = "MAPS"
TABS = {
    "Map Info": MapInfoPage,
    "Rooms": RoomsPage,
    "Trails": TrailsPage,
    "Boundary": BoundaryPage,
    "Map-Wide Assets": MapAssetsPage
}


class MapManagerApp(tk.Toplevel):
    def __init__(self):
        super().__init__()
        self.title("Map Manager")
        try:
            self.state("zoomed")
        except tk.TclError:
            self.geometry("1000x700")

        self.maps = self._scan_maps()
        self.current_map = None
        self.pages = {}

        style = ttk.Style(self)
        for w in ('TLabel','TButton','TCheckbutton','TEntry','TMenubutton','TSpinbox','TScale'):
            style.configure(w, foreground='black')

        paned = ttk.Panedwindow(self, orient=tk.HORIZONTAL)
        paned.pack(fill=tk.BOTH, expand=True)

        left = ttk.Frame(paned, width=120)
        paned.add(left, weight=0)

        top_btns = ttk.Frame(left)
        top_btns.pack(fill=tk.X, pady=(8, 4), padx=5)

        switch_btn = tk.Button(top_btns, text="Switch to Asset Manager", bg="#D9534F", fg="white",
                                font=("Segoe UI", 11, "bold"), command=self._open_asset_manager)
        switch_btn.pack(fill=tk.X, pady=(0, 4))

        new_btn = tk.Button(top_btns, text="New Map", bg="#007BFF", fg="white",
                            font=("Segoe UI", 11, "bold"), command=self._new_map)
        new_btn.pack(fill=tk.X, pady=(0, 6))

        self.list_canvas = tk.Canvas(left, borderwidth=0, highlightthickness=0, width=220)
        self.list_scroll = ttk.Scrollbar(left, orient="vertical", command=self.list_canvas.yview)
        self.list_canvas.configure(yscrollcommand=self.list_scroll.set)
        self.list_canvas.pack(side="left", fill="both", expand=True)
        self.list_scroll.pack(side="right", fill="y")

        self.list_inner = ttk.Frame(self.list_canvas)
        self.list_window = self.list_canvas.create_window((0, 0), window=self.list_inner, anchor="nw")
        self.list_inner.bind("<Configure>", lambda e: self.list_canvas.configure(scrollregion=self.list_canvas.bbox("all")))
        self.list_canvas.bind_all("<MouseWheel>", self._on_mousewheel_assets)

        self.map_buttons = {}

        right = ttk.Frame(paned)
        paned.add(right, weight=1)

        canvas = tk.Canvas(right)
        scrollbar = ttk.Scrollbar(right, orient="vertical", command=canvas.yview)
        canvas.configure(yscrollcommand=scrollbar.set)
        canvas.pack(side="left", fill="both", expand=True)
        scrollbar.pack(side="right", fill="y")

        scrollable_frame = ttk.Frame(canvas)
        scroll_window = canvas.create_window((0, 0), window=scrollable_frame, anchor="nw")

        scrollable_frame.bind("<Configure>", lambda e: canvas.configure(scrollregion=canvas.bbox("all")))
        scrollable_frame.bind_all("<MouseWheel>", self._on_mousewheel_assets)

        self.notebook = ttk.Notebook(scrollable_frame)
        self.notebook.pack(fill=tk.BOTH, expand=True)

        for title, cls in TABS.items():
            page = cls(self.notebook)
            self.notebook.add(page, text=title)
            self.pages[title] = page

        self._refresh_map_list()
        if self.maps:
            first = next(iter(self.map_buttons.values()))
            first.invoke()

    def _on_mousewheel_assets(self, event):
        self.list_canvas.yview_scroll(int(-1*(event.delta/120)), "units")



    def _bind_mousewheel(self, bind):
        method = self.list_canvas.bind_all if bind else self.list_canvas.unbind_all
        method("<MouseWheel>", self._on_mousewheel_list)

    def _on_mousewheel_list(self, event):
        self.list_canvas.yview_scroll(int(-1*(event.delta/120)), "units")

    def _scan_maps(self):
        if not os.path.isdir(MAPS_DIR):
            os.makedirs(MAPS_DIR)
        return [d for d in sorted(os.listdir(MAPS_DIR)) if os.path.isdir(os.path.join(MAPS_DIR, d))]

    def _refresh_map_list(self):
        self.maps = self._scan_maps()
        for widget in self.list_inner.winfo_children():
            widget.destroy()
        self.map_buttons.clear()

        # Inside _refresh_map_list
        for name in self.maps:
            btn = tk.Button(
                self.list_inner,
                text=name,
                anchor="w",
                width=30,  # Standardized width
                font=("Segoe UI", 9),
                command=lambda n=name: self._select_map(n),
                height=1  # Button height can be adjusted here
            )
            btn.pack(fill="x", padx=4, pady=1)  # Adjust vertical spacing as needed
            self.map_buttons[name] = btn


    def _select_map(self, name):
        self.current_map = name
        for btn in self.map_buttons.values():
            btn.configure(bg="#f0f0f0")
        self.map_buttons[name].configure(bg="#CCE5FF")

        for title, page in self.pages.items():
            json_filename = page.get_json_filename()
            json_path = os.path.join(MAPS_DIR, name, json_filename)

            try:
                if isinstance(page, TrailsPage):
                    page.load_data(None, json_path)
                else:
                    if os.path.exists(json_path):
                        with open(json_path, "r") as f:
                            data = json.load(f)
                        page.load_data(data, json_path)
            except Exception as e:
                print(f"[MapManager] Failed to load {json_filename}: {e}")

    def _new_map(self):
        from tkinter import simpledialog
        while True:
            raw_name = simpledialog.askstring("New Map", "Enter a name for the new map:")
            if raw_name is None:
                return
            safe_name = "".join(c for c in raw_name if c.isalnum() or c in ("_", "-")).strip()
            if not safe_name:
                messagebox.showerror("Invalid Name", "Map name must contain letters, numbers, dashes or underscores.")
                continue
            folder = os.path.join(MAPS_DIR, safe_name)
            if os.path.exists(folder):
                messagebox.showerror("Already Exists", f"A map named '{safe_name}' already exists.")
                continue
            break

        os.makedirs(folder, exist_ok=True)
        for page_cls in TABS.values():
            path = os.path.join(folder, page_cls.get_json_filename())
            if not os.path.exists(path):
                with open(path, "w") as f:
                    f.write("{}")

        self._refresh_map_list()
        idx = self.maps.index(safe_name)
        list(self.map_buttons.values())[idx].invoke()

    def _open_asset_manager(self):
        from asset_manager_main import AssetOrganizerApp
        AssetOrganizerApp()
        self.destroy()
