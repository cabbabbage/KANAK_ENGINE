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

        paned = ttk.Panedwindow(self, orient=tk.HORIZONTAL)
        paned.pack(fill=tk.BOTH, expand=True)

        left = ttk.Frame(paned, width=200)
        paned.add(left, weight=1)

        self.listbox = tk.Listbox(left, font=("Segoe UI", 12), fg="black")
        self.listbox.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        self.listbox.bind("<<ListboxSelect>>", self._on_map_select)
        self.listbox.bind("<Button-3>", self._show_context_menu)

        self.context_menu = tk.Menu(self, tearoff=0)
        self.context_menu.add_command(label="Delete", command=self._delete_selected_map)

        new_btn = tk.Button(left, text="New Map", bg="#007BFF", fg="white", font=("Segoe UI", 10, "bold"),
                            command=self._new_map)
        new_btn.pack(fill=tk.X, padx=5, pady=(0, 5))

        asset_btn = tk.Button(left, text="Switch to Asset Manager", bg="#D9534F", fg="white",
                              font=("Segoe UI", 10, "bold"), command=self._open_asset_manager)
        asset_btn.pack(fill=tk.X, padx=5, pady=(0, 10))

        right = ttk.Frame(paned)
        paned.add(right, weight=4)

        self.notebook = ttk.Notebook(right)
        self.notebook.pack(fill=tk.BOTH, expand=True)

        for title, cls in TABS.items():
            page = cls(self.notebook)
            self.notebook.add(page, text=title)
            self.pages[title] = page

        self._refresh_map_list()
        if self.maps:
            self.listbox.selection_set(0)
            self._on_map_select(None)

    def _scan_maps(self):
        if not os.path.isdir(MAPS_DIR):
            os.makedirs(MAPS_DIR)
        return [d for d in sorted(os.listdir(MAPS_DIR)) if os.path.isdir(os.path.join(MAPS_DIR, d))]

    def _refresh_map_list(self):
        self.maps = self._scan_maps()
        self.listbox.delete(0, tk.END)
        for name in self.maps:
            self.listbox.insert(tk.END, name)

    def _on_map_select(self, _):
        sel = self.listbox.curselection()
        if not sel:
            return
        self.current_map = self.maps[sel[0]]
        for title, page in self.pages.items():
            json_filename = page.get_json_filename()
            json_path = os.path.join(MAPS_DIR, self.current_map, json_filename)

            try:
                if isinstance(page, TrailsPage):
                    page.load_data(None, json_path)
                else:
                    if os.path.exists(json_path):
                        with open(json_path, "r") as f:
                            data = json.load(f)
                        page.load_data(data, json_path)
                    else:
                        print(f"[MapManager] Missing file: {json_path}")
            except Exception as e:
                print(f"[MapManager] Failed to load {json_filename}: {e}")

    def _new_map(self):
        while True:
            raw_name = simpledialog.askstring("New Map", "Enter a name for the new map:")
            if raw_name is None:
                return  # Cancelled
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
        self.listbox.selection_clear(0, tk.END)
        self.listbox.selection_set(idx)
        self._on_map_select(None)

    def _show_context_menu(self, event):
        widget = event.widget
        if widget == self.listbox:
            index = widget.nearest(event.y)
            widget.selection_clear(0, tk.END)
            widget.selection_set(index)
            self.context_menu.post(event.x_root, event.y_root)

    def _delete_selected_map(self):
        sel = self.listbox.curselection()
        if not sel:
            return
        map_name = self.maps[sel[0]]
        confirm = messagebox.askyesno("Confirm Delete", f"Are you sure you want to delete map '{map_name}'?")
        if not confirm:
            return

        folder_path = os.path.join(MAPS_DIR, map_name)
        try:
            import shutil
            shutil.rmtree(folder_path)
        except Exception as e:
            messagebox.showerror("Error", f"Failed to delete map: {e}")
            return

        self._refresh_map_list()
        self.current_map = None
        self.listbox.selection_clear(0, tk.END)

    def _open_asset_manager(self):
        from asset_manager_main import AssetOrganizerApp
        AssetOrganizerApp()
        self.destroy()

if __name__ == "__main__":
    root = tk.Tk()
    root.withdraw()
    MapManagerApp()
    root.mainloop()
