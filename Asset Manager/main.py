# main.py

import os
import json
import subprocess
import shutil
import tkinter as tk
from tkinter import ttk, messagebox

from pages.basic_info      import BasicInfoPage
from pages.on_interaction  import OnInteractionPage
from pages.on_attack       import OnAttackPage
from pages.on_collision    import OnCollisionPage
from pages.size            import SizePage
from pages.passa           import PassabilityPage
from pages.overlay         import ImageOverlayPage
from pages.child           import ChildAssetsPage
from pages.spacing         import SpacingThresholdPage
from pages.fixed_child     import FixedChildAssetsPage

ASSET_DIR = "SRC"

class AssetOrganizerApp(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("Asset Organizer")
        try:
            self.state('zoomed')
        except tk.TclError:
            self.attributes('-zoomed', True)

        # Enforce black text by default
        style = ttk.Style(self)
        for w in ('TLabel','TButton','TCheckbutton','TEntry',
                  'TMenubutton','TSpinbox','TScale'):
            style.configure(w, foreground='black')

        # State
        self.assets = self._scan_assets()
        self.current_asset = None
        self.pages = {}
        self.tab_titles = {}   # map page -> original title

        # Layout
        paned = ttk.Panedwindow(self, orient=tk.HORIZONTAL)
        paned.pack(fill=tk.BOTH, expand=True)

        # Left panel: asset list + New Asset button
        left = ttk.Frame(paned, width=200)
        paned.add(left, weight=1)
        self.listbox = tk.Listbox(left, font=("Segoe UI",12), fg='black')
        self.listbox.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)

        # Context menu for listbox
        self.context_menu = tk.Menu(self, tearoff=0)
        self.context_menu.add_command(label="Copy Name", command=self._copy_asset_name)
        self.context_menu.add_command(label="Duplicate", command=self._duplicate_asset)
        self.context_menu.add_separator()
        self.context_menu.add_command(label="Delete", command=self._delete_asset)
        self.listbox.bind("<Button-3>", self._on_right_click)
        self.listbox.bind("<<ListboxSelect>>", self._on_asset_select)

        ttk.Button(left, text="New Asset", command=self._new_asset)\
            .pack(fill=tk.X, padx=5, pady=(0,10))

        # Right panel: notebook with pages
        right = ttk.Frame(paned)
        paned.add(right, weight=4)
        self.notebook = ttk.Notebook(right)
        self.notebook.pack(fill=tk.BOTH, expand=True)

        # Create pages
        basic = BasicInfoPage(self.notebook, on_rename_callback=self._on_asset_renamed)
        self._add_tab(basic, "Basic Info")

        def add_page(cls, title):
            page = cls(self.notebook)
            self._add_tab(page, title)

        add_page(SizePage,             "Sizing")
        add_page(PassabilityPage,      "Passability")
        add_page(SpacingThresholdPage, "Spacing")
        add_page(OnInteractionPage,    "On-Interaction")
        add_page(OnAttackPage,         "On-Attack")
        add_page(OnCollisionPage,      "On-Collision")
        add_page(ImageOverlayPage,     "Image Overlay")
        add_page(ChildAssetsPage,      "Region-Based Child Assets")
        add_page(FixedChildAssetsPage, "Fixed-Spawn Child Assets")

        # Wrap each page.save to record visits (✓) and persist visited_tabs
        for title, page in self.pages.items():
            orig = page.save
            def make_wrap(t=title, p=page, o=orig):
                def wrapped():
                    o()
                    path = self._asset_json_path()
                    if path:
                        with open(path,'r') as f:
                            data = json.load(f)
                        visited = set(data.get("visited_tabs", []))
                        if t not in visited:
                            visited.add(t)
                            data["visited_tabs"] = sorted(visited)
                            with open(path,'w') as f:
                                json.dump(data, f, indent=2)
                    # update tab label to include ✓
                    self.notebook.tab(p, text=f"{t} ✓")
                return wrapped
            page.save = make_wrap()

        # Basic Info’s save also re-enables all tabs
        def save_basic_and_enable():
            basic.save()
            self._update_tab_states()
        basic.save = save_basic_and_enable

        # Enable/disable on tab change
        self.notebook.bind("<<NotebookTabChanged>>", self._update_tab_states)

        # Finalize button
        footer = ttk.Frame(self)
        footer.pack(fill=tk.X, side=tk.BOTTOM, padx=12, pady=8)
        ttk.Button(footer, text="Finalize & Export",
                   command=self._run_finalize).grid(row=0, column=1, sticky="e")

        # Initial load
        self._refresh_asset_list()
        if self.assets:
            self.listbox.selection_set(0)
            self._on_asset_select(None)

    def _add_tab(self, page, title):
        self.notebook.add(page, text=title)
        self.pages[title] = page
        self.tab_titles[page] = title

    def _scan_assets(self):
        if not os.path.isdir(ASSET_DIR):
            return []
        return [d for d in sorted(os.listdir(ASSET_DIR))
                if os.path.isdir(os.path.join(ASSET_DIR, d))]

    def _refresh_asset_list(self):
        self.assets = self._scan_assets()
        self.listbox.delete(0, tk.END)
        for idx, name in enumerate(self.assets):
            self.listbox.insert(tk.END, name)
            inf = os.path.join(ASSET_DIR, name, "info.json")
            if os.path.isfile(inf) and json.load(open(inf)).get("child_only", False):
                self.listbox.itemconfig(idx, fg='green')

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

        # load pages
        for page in self.pages.values():
            page.load(path)

        # restore visited marks
        visited = []
        if path and os.path.isfile(path):
            visited = json.load(open(path)).get("visited_tabs", [])
        for page, orig_title in self.tab_titles.items():
            text = f"{orig_title} ✓" if orig_title in visited else orig_title
            self.notebook.tab(page, text=text)

        # show Basic Info first
        self.notebook.select(self.pages["Basic Info"])
        self._update_tab_states()

    def _asset_json_path(self):
        if not self.current_asset:
            return None
        return os.path.join(ASSET_DIR, self.current_asset, "info.json")

    def _update_tab_states(self, _=None):
        data = {}
        p = self._asset_json_path()
        if p and os.path.isfile(p):
            data = json.load(open(p))
        basic_ok = bool(data.get("asset_name")
                        and data.get("default_animation",{}).get("frames_path"))
        for title, page in self.pages.items():
            state = "normal" if title == "Basic Info" or basic_ok else "disabled"
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

    def _run_finalize(self):
        try:
            subprocess.run(["python", "finalize.py"], check=True)
            messagebox.showinfo("Done", "Export complete: see SRC_final/manifest.json")
        except subprocess.CalledProcessError as e:
            messagebox.showerror("Error", f"Finalize failed:\n{e}")

if __name__ == "__main__":
    AssetOrganizerApp().mainloop()
