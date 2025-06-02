import os
import json
import shutil
import tkinter as tk
from tkinter import ttk, messagebox
from pages.AnimationConfig import AnimationEditor
from pages.basic_info      import BasicInfoPage
from pages.size            import SizePage
from pages.Animations      import AnimationsPage
from pages.passa           import PassabilityPage
from pages.overlay         import ImageOverlayPage
from pages.child           import ChildAssetsPage
from pages.spacing         import SpacingThresholdPage


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
        self.current_asset = None
        self.pages = {}

        paned = ttk.Panedwindow(self, orient=tk.HORIZONTAL)
        paned.pack(fill=tk.BOTH, expand=True)

        left = ttk.Frame(paned, width=200)
        paned.add(left, weight=0)
        self.listbox = tk.Listbox(left, font=("Segoe UI",12), fg='black')
        self.listbox.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)

        self.context_menu = tk.Menu(self, tearoff=0)
        self.context_menu.add_command(label="Copy Name", command=self._copy_asset_name)
        self.context_menu.add_command(label="Duplicate", command=self._duplicate_asset)
        self.context_menu.add_separator()
        self.context_menu.add_command(label="Delete", command=self._delete_asset)
        self.listbox.bind("<Button-3>", self._on_right_click)
        self.listbox.bind("<<ListboxSelect>>", self._on_asset_select)

        new_btn = tk.Button(left, text="New Asset", bg="#007BFF", fg="white", font=("Segoe UI", 10, "bold"),
                            command=self._new_asset)
        new_btn.pack(fill=tk.X, padx=5, pady=(0, 10))

        asset_btn = tk.Button(left, text="Switch to Map Manager", bg="#D9534F", fg="white",
                              font=("Segoe UI", 10, "bold"), command=self._open_asset_manager)
        asset_btn.pack(fill=tk.X, padx=5, pady=(0, 10))

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

        basic = BasicInfoPage(self.notebook, on_rename_callback=self._on_asset_renamed)
        self._add_tab(basic, "Basic Info")

        def add_page(cls, title):
            if cls == AnimationsPage:
                asset_path = os.path.join(ASSET_DIR, self.current_asset or "")
                page = cls(self.notebook, asset_folder=asset_path)
                self.animations_tab = page
            else:
                page = cls(self.notebook)
            self._add_tab(page, title)

        add_page(SizePage,             "Sizing")
        add_page(PassabilityPage,      "Passability")
        add_page(SpacingThresholdPage, "Spacing")
        add_page(AnimationsPage,       "Animations")
        #add_page(ImageOverlayPage,     "Image Overlay")
        add_page(ChildAssetsPage,      "Region-Based Child Assets")

        self.notebook.bind("<<NotebookTabChanged>>", self._update_tab_states)

        self._refresh_asset_list()
        if self.assets:
            self.listbox.selection_set(0)
            self._on_asset_select(None)

    def _add_tab(self, page, title):
        self.notebook.add(page, text=title)
        self.pages[title] = page

    def _scan_assets(self):
        if not os.path.isdir(ASSET_DIR):
            return []
        return [d for d in sorted(os.listdir(ASSET_DIR))
                if os.path.isdir(os.path.join(ASSET_DIR, d))]

    def _refresh_asset_list(self):
        self.assets = self._scan_assets()
        self.listbox.delete(0, tk.END)
        for name in self.assets:
            self.listbox.insert(tk.END, name)

    def _on_right_click(self, event):
        idx = self.listbox.nearest(event.y)
        if idx is not None:
            self.listbox.selection_clear(0, tk.END)
            self.listbox.selection_set(idx)
            self.current_asset = self.assets[idx]
            self.context_menu.tk_popup(event.x_root, event.y_root)

    def _copy_asset_name(self):
        sel = self.listbox.curselection()
        if not sel:
            return
        name = self.assets[sel[0]]
        self.clipboard_clear()
        self.clipboard_append(name)

    def _duplicate_asset(self):
        sel = self.listbox.curselection()
        if not sel:
            return
        src = self.assets[sel[0]]
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
        sel = self.listbox.curselection()
        if not sel:
            return
        name = self.assets[sel[0]]
        if not messagebox.askyesno("Delete", f"Really delete '{name}'?"):
            return
        shutil.rmtree(os.path.join(ASSET_DIR, name))
        self._refresh_asset_list()

    def _on_asset_select(self, _):
        sel = self.listbox.curselection()
        if not sel:
            return
        self.current_asset = self.assets[sel[0]]
        path = self._asset_json_path()

        if hasattr(self, "animations_tab"):
            self.animations_tab.asset_folder = os.path.join(ASSET_DIR, self.current_asset)
            self.animations_tab._load_existing()

        for page in self.pages.values():
            page.load(path)

        self.notebook.select(self.pages["Basic Info"])
        self._update_tab_states()

    def _asset_json_path(self):
        if not self.current_asset:
            return None
        return os.path.join(ASSET_DIR, self.current_asset, "info.json")

    def _open_asset_manager(self):
        from map_manager_main import MapManagerApp
        MapManagerApp()
        self.destroy()

    def _update_tab_states(self, _=None):
        data = {}
        p = self._asset_json_path()
        if p and os.path.isfile(p):
            data = json.load(open(p))

        core_keys = ["asset_name", "asset_type", "animations", "available_animations"]
        has_core_fields = all(k in data and data[k] for k in core_keys)

        for title, page in self.pages.items():
            state = "normal" if title == "Basic Info" or has_core_fields else "disabled"
            self.notebook.tab(page, state=state)

    def _new_asset(self):
        i = 1
        while True:
            name = f"new_asset_{i}"
            folder = os.path.join(ASSET_DIR, name)
            if not os.path.exists(folder):
                os.makedirs(folder)
                open(os.path.join(folder, "info.json"), "w").write("{}")
                break
            i += 1
        self._refresh_asset_list()
        idx = self.assets.index(name)
        self.listbox.selection_clear(0, tk.END)
        self.listbox.selection_set(idx)
        self._on_asset_select(None)

    def _on_asset_renamed(self, old, new):
        self._refresh_asset_list()
        idx = self.assets.index(new)
        self.listbox.selection_clear(0, tk.END)
        self.listbox.selection_set(idx)
        self._on_asset_select(None)

if __name__ == "__main__":
    AssetOrganizerApp().mainloop()