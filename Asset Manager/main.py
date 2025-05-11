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

ASSET_DIR = "SRC"

class AssetOrganizerApp(tk.Tk):
    def __init__(self):
        super().__init__()

        # — Title & maximize —
        self.title("Asset Organizer")
        try:
            self.state('zoomed')
        except tk.TclError:
            self.attributes('-zoomed', True)

        # — Enforce black text —
        style = ttk.Style(self)
        for w in ('TLabel','TButton','TCheckbutton','TEntry',
                  'TMenubutton','TSpinbox','TScale'):
            style.configure(w, foreground='black')

        # --- state ---
        self.assets = self._scan_assets()
        self.current_asset = self.assets[0] if self.assets else None
        self._right_click_index = None

        # --- layout ---
        paned = ttk.Panedwindow(self, orient=tk.HORIZONTAL)
        paned.pack(fill=tk.BOTH, expand=True)

        # left: asset list + New Asset button
        left = ttk.Frame(paned, width=200)
        paned.add(left, weight=1)
        self.listbox = tk.Listbox(left, font=("Segoe UI",12), fg='black')
        for name in self.assets:
            self.listbox.insert(tk.END, name)
        self.listbox.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        if self.assets:
            self.listbox.selection_set(0)

        # Right-click context menu
        self.context_menu = tk.Menu(self, tearoff=0)
        self.context_menu.add_command(label="Copy Name", command=self._copy_asset_name)
        self.context_menu.add_command(label="Duplicate", command=self._duplicate_asset)
        self.context_menu.add_separator()
        self.context_menu.add_command(label="Delete", command=self._delete_asset)

        self.listbox.bind("<Button-3>", self._on_right_click)
        self.listbox.bind("<<ListboxSelect>>", self._on_asset_select)

        ttk.Button(left, text="New Asset", command=self._new_asset)\
            .pack(fill=tk.X, padx=5, pady=(0,10))

        # right: notebook with tabs
        right = ttk.Frame(paned)
        paned.add(right, weight=4)
        self.notebook = ttk.Notebook(right)
        self.notebook.pack(fill=tk.BOTH, expand=True)

        # --- pages ---
        self.pages = {}
        basic = BasicInfoPage(self.notebook, on_rename_callback=self._on_asset_renamed)
        self.notebook.add(basic, text="Basic Info"); self.pages["Basic Info"] = basic
        size_tab = SizePage(self.notebook)
        self.notebook.add(size_tab, text="Sizing"); self.pages["Sizing"] = size_tab
        pass_tab = PassabilityPage(self.notebook)
        self.notebook.add(pass_tab, text="Passability"); self.pages["Passability"] = pass_tab
        spacing_tab = SpacingThresholdPage(self.notebook)
        self.notebook.add(spacing_tab, text="Spacing"); self.pages["Spacing"] = spacing_tab
        interaction = OnInteractionPage(self.notebook)
        self.notebook.add(interaction, text="On-Interaction"); self.pages["On-Interaction"] = interaction
        attack = OnAttackPage(self.notebook)
        self.notebook.add(attack, text="On-Attack"); self.pages["On-Attack"] = attack
        collision = OnCollisionPage(self.notebook)
        self.notebook.add(collision, text="On-Collision"); self.pages["On-Collision"] = collision
        overlay_tab = ImageOverlayPage(self.notebook)
        self.notebook.add(overlay_tab, text="Image Overlay"); self.pages["Image Overlay"] = overlay_tab
        child_tab = ChildAssetsPage(self.notebook)
        self.notebook.add(child_tab, text="Child Assets"); self.pages["Child Assets"] = child_tab

        self.notebook.bind("<<NotebookTabChanged>>", self._on_tab_changed)

        # initial load
        self._load_current_asset()
        self._update_tab_states()

        # finalize button
        footer = ttk.Frame(self)
        footer.pack(fill=tk.X, side=tk.BOTTOM, padx=12, pady=8)
        footer.columnconfigure(0, weight=1)
        btn_finalize = ttk.Button(footer, text="Finalize & Export",
                                  command=self._run_finalize, style='TButton')
        btn_finalize.grid(row=0, column=1, sticky="e")

    def _scan_assets(self):
        if not os.path.isdir(ASSET_DIR):
            return []
        return [d for d in sorted(os.listdir(ASSET_DIR))
                if os.path.isdir(os.path.join(ASSET_DIR, d))]

    def _on_right_click(self, event):
        idx = self.listbox.nearest(event.y)
        if idx is None: return
        self._right_click_index = idx
        self.listbox.selection_clear(0, tk.END)
        self.listbox.selection_set(idx)
        self.current_asset = self.assets[idx]
        self.context_menu.tk_popup(event.x_root, event.y_root)

    def _copy_asset_name(self):
        name = self.assets[self._right_click_index]
        self.clipboard_clear()
        self.clipboard_append(name)

    def _duplicate_asset(self):
        src_name = self.assets[self._right_click_index]
        src_folder = os.path.join(ASSET_DIR, src_name)
        # find next suffix
        i = 1
        while True:
            dst_name = f"{src_name}_{i}"
            dst_folder = os.path.join(ASSET_DIR, dst_name)
            if not os.path.exists(dst_folder):
                shutil.copytree(src_folder, dst_folder)
                break
            i += 1
        self._refresh_asset_list()
        messagebox.showinfo("Duplicated", f"{src_name} → {dst_name}")

    def _delete_asset(self):
        name = self.assets[self._right_click_index]
        if not messagebox.askyesno("Delete", f"Really delete '{name}'?"):
            return
        shutil.rmtree(os.path.join(ASSET_DIR, name))
        self._refresh_asset_list()

    def _refresh_asset_list(self):
        self.assets = self._scan_assets()
        self.listbox.delete(0, tk.END)
        for a in self.assets:
            self.listbox.insert(tk.END, a)
        # select first
        if self.assets:
            self.listbox.selection_set(0)
            self.current_asset = self.assets[0]
            self._load_current_asset()
            self._update_tab_states()

    def _on_asset_select(self, _):
        sel = self.listbox.curselection()
        if not sel: return
        self.current_asset = self.assets[sel[0]]
        self._load_current_asset()
        self._update_tab_states()

    def _on_tab_changed(self, _):
        self._update_tab_states()

    def _asset_json_path(self):
        if not self.current_asset:
            return None
        return os.path.join(ASSET_DIR, self.current_asset, "info.json")

    def _load_current_asset(self):
        path = self._asset_json_path()
        for page in self.pages.values():
            page.load(path)
        self.notebook.select(0)

    def _new_asset(self):
        idx = 1
        while True:
            name = f"new_asset_{idx}"
            folder = os.path.join(ASSET_DIR, name)
            if not os.path.exists(folder):
                os.makedirs(folder)
                break
            idx += 1
        open(os.path.join(folder, "info.json"), "w").write("{}")
        self._refresh_asset_list()

    def _on_asset_renamed(self, old_name, new_name):
        idx = self.assets.index(old_name)
        self.assets[idx] = new_name
        self.listbox.delete(idx)
        self.listbox.insert(idx, new_name)
        self.listbox.selection_clear(0, tk.END)
        self.listbox.selection_set(idx)
        self.current_asset = new_name

    def _update_tab_states(self):
        if not self.current_asset:
            for p in self.pages.values():
                self.notebook.tab(p, state="disabled")
            return
        info_path = self._asset_json_path()
        basic_ok = False
        try:
            with open(info_path) as f:
                d = json.load(f)
            basic_ok = bool(d.get("asset_name") and d.get("default_animation",{}).get("on_start"))
        except:
            pass
        for title, page in self.pages.items():
            state = "normal" if title=="Basic Info" or basic_ok else "disabled"
            self.notebook.tab(page, state=state)

    def _run_finalize(self):
        try:
            subprocess.run(["python","finalize.py"], check=True)
            messagebox.showinfo("Done","Export complete: see SRC_final/manifest.json")
        except subprocess.CalledProcessError as e:
            messagebox.showerror("Error", f"Finalize failed:\n{e}")

if __name__ == "__main__":
    AssetOrganizerApp().mainloop()
