# pages/child_assets.py

import os
import json
import tkinter as tk
from tkinter import ttk, messagebox
from pages.boundary import BoundaryConfigurator

ASSET_DIR = "SRC"

class ChildAssetsPage(ttk.Frame):
    def __init__(self, parent):
        super().__init__(parent)

        # — Theme & fonts —
        self.FONT            = ('Segoe UI', 14)
        self.FONT_BOLD       = ('Segoe UI', 18, 'bold')
        self.MAIN_COLOR      = "#005f73"
        self.SECONDARY_COLOR = "#ee9b00"

        # — State —
        self.asset_path    = None
        self.child_frames  = []
        self.asset_names   = []
        self.child_only_var = tk.BooleanVar(value=False)

        # — Styles —
        style = ttk.Style(self)
        style.configure('Main.TButton',
                        font=self.FONT, padding=6,
                        background=self.MAIN_COLOR,
                        foreground='black')
        style.map('Main.TButton',
                  background=[('active', self.SECONDARY_COLOR)])
        style.configure('LargeBold.TLabel',
                        font=self.FONT_BOLD,
                        foreground=self.SECONDARY_COLOR)
        style.configure('Large.TLabel', font=self.FONT)
        style.configure('Large.TCheckbutton', font=self.FONT)

        # — Layout —

        # Child-only checkbox
        ttk.Checkbutton(self,
                        text="This is a child-only asset",
                        variable=self.child_only_var,
                        command=self._on_child_only_toggle,
                        style='Large.TCheckbutton')\
            .grid(row=0, column=0, columnspan=4,
                  sticky="w", padx=12, pady=(10,4))

        # Title
        ttk.Label(self, text="Child Assets", style='LargeBold.TLabel')\
            .grid(row=1, column=0, columnspan=4,
                  pady=(4,20), padx=12)

        # Container for child rows
        self.container = ttk.Frame(self)
        self.container.grid(row=2, column=0, columnspan=4,
                            sticky="nsew", padx=12)
        self.columnconfigure(1, weight=1)

        # Add Child button
        self.btn_add = ttk.Button(self,
                                  text="Add Child",
                                  command=self._add_child_row,
                                  style='Main.TButton')
        self.btn_add.grid(row=3, column=0, columnspan=4,
                          pady=(0,12))

        # Save button
        self.btn_save = ttk.Button(self,
                                   text="Save",
                                   command=self.save,
                                   style='Main.TButton')
        self.btn_save.grid(row=4, column=0, columnspan=4,
                           pady=(0,12))

    def _on_child_only_toggle(self):
        disabled = self.child_only_var.get()
        state = 'disabled' if disabled else 'normal'
        self.btn_add.config(state=state)
        for entry in self.child_frames:
            for widget in entry['frame'].winfo_children():
                try:
                    widget.config(state=state)
                except tk.TclError:
                    pass

    def _add_child_row(self, data=None):
        row = len(self.child_frames)
        frm = ttk.Frame(self.container, relief='ridge', padding=12)
        frm.grid(row=row, column=0, sticky="ew", pady=8)
        frm.columnconfigure(1, weight=1)

        # Asset dropdown
        ttk.Label(frm, text="Asset:", style='Large.TLabel')\
            .grid(row=0, column=0, sticky="w")
        asset_var = tk.StringVar(value=(data or {}).get('asset', ''))
        om = ttk.OptionMenu(frm, asset_var, asset_var.get(), *self.asset_names)
        om.grid(row=0, column=1, columnspan=3, sticky="we", padx=6)

        # Spawn Area
        ttk.Label(frm, text="Spawn Area:", style='Large.TLabel')\
            .grid(row=1, column=0, sticky="w", pady=4)
        btn_area = ttk.Button(frm, text="Draw…",
                              command=lambda: self._draw_area(row),
                              style='Main.TButton')
        btn_area.grid(row=1, column=1, sticky="w", padx=6)
        area_label = ttk.Label(frm, text="(none)", style='Large.TLabel')
        area_label.grid(row=1, column=2, columnspan=2, sticky="w")

        # Z-Order
        ttk.Label(frm, text="Z-Order:", style='Large.TLabel')\
            .grid(row=2, column=0, sticky="w", pady=4)
        zopts = {'same': "Same Z", '+1': "In Front", '-1': "Behind"}
        # load incoming key or label
        incoming = (data or {}).get('zmode', 'same')
        current_label = zopts.get(incoming, incoming)
        z_var = tk.StringVar(value=current_label)
        om_z = ttk.OptionMenu(frm,
                              z_var,
                              current_label,
                              *zopts.values())
        om_z.grid(row=2, column=1, sticky="w", padx=6)
        frm._zopts = zopts  # stash mapping for save()

        # Min / Max count
        min_var = tk.IntVar(value=(data or {}).get('min', 1))
        max_var = tk.IntVar(value=(data or {}).get('max', 1))
        ttk.Label(frm, text="Min #:", style='Large.TLabel')\
            .grid(row=3, column=0, sticky="w", pady=4)
        ttk.Spinbox(frm, from_=0, to=100, textvariable=min_var, width=6)\
            .grid(row=3, column=1, sticky="w", padx=6)
        ttk.Label(frm, text="Max #:", style='Large.TLabel')\
            .grid(row=3, column=2, sticky="w", pady=4)
        ttk.Spinbox(frm, from_=0, to=100, textvariable=max_var, width=6)\
            .grid(row=3, column=3, sticky="w", padx=6)

        # Skew slider
        skew_var = tk.DoubleVar(value=(data or {}).get('skew', 0.0))
        ttk.Label(frm, text="Skew:", style='Large.TLabel')\
            .grid(row=4, column=0, sticky="w", pady=4)
        ttk.Scale(frm, from_=-1.0, to=1.0, variable=skew_var,
                  orient='horizontal')\
            .grid(row=4, column=1, columnspan=3,
                  sticky="we", padx=6, pady=4)

        # Place on Top checkbox
        on_top_var = tk.BooleanVar(value=(data or {}).get('on_top', False))
        ttk.Checkbutton(frm, text="Place on Top",
                        variable=on_top_var,
                        style='Large.TCheckbutton')\
            .grid(row=5, column=0, columnspan=4,
                  sticky="w", pady=(8,4))

        # Remove button
        btn_rm = ttk.Button(frm, text="Remove",
                            command=lambda: self._remove_child_row(row))
        btn_rm.grid(row=0, column=3, sticky="e")

        self.child_frames.append({
            'frame':  frm,
            'vars': {
                'asset':  asset_var,
                'area':   None,
                'zmode':  z_var,
                'min':    min_var,
                'max':    max_var,
                'skew':   skew_var,
                'on_top': on_top_var
            },
            'widgets': {
                'area_label': area_label
            }
        })

        # if loading existing geometry
        if data and data.get('area_geo'):
            self.child_frames[row]['vars']['area'] = data['area_geo']
            area_label.config(text="(configured)")

        # apply child-only state
        self._on_child_only_toggle()

    def _remove_child_row(self, idx):
        entry = self.child_frames[idx]
        entry['frame'].destroy()
        self.child_frames.pop(idx)
        for i, e in enumerate(self.child_frames):
            e['frame'].grid_configure(row=i)

    def _draw_area(self, idx):
        entry = self.child_frames[idx]
        asset_dir = os.path.dirname(self.asset_path)
        default_folder = os.path.join(asset_dir, 'default')
        if not os.path.isdir(default_folder):
            messagebox.showerror(
                "Error", f"No default frames in:\n{default_folder}")
            return

        def callback(geo):
            entry['vars']['area'] = geo
            entry['widgets']['area_label'].config(text="(configured)")

        BoundaryConfigurator(
            self,
            base_folder=default_folder,
            callback=callback
        )

    def load(self, info_path):
        """Load list of child entries and child_only flag, pruning invalid ones,
        and rebuild the dropdown to only show other child-only assets."""
        self.asset_path = info_path
        if not info_path:
            return

        # Determine current asset name
        current = os.path.basename(os.path.dirname(info_path))

        # Rebuild list of assets that are marked child_only=True (excluding self)
        self.asset_names = []
        for name in sorted(os.listdir(ASSET_DIR)):
            folder = os.path.join(ASSET_DIR, name)
            infof  = os.path.join(folder, "info.json")
            if name == current or not os.path.isdir(folder) or not os.path.isfile(infof):
                continue
            with open(infof, 'r') as f:
                info = json.load(f)
            if info.get("child_only", False):
                self.asset_names.append(name)

        # Load this asset’s own JSON
        with open(info_path, 'r') as f:
            data = json.load(f)

        # Restore its child_only flag
        self.child_only_var.set(data.get('child_only', False))

        # Clear existing UI rows
        for entry in self.child_frames:
            entry['frame'].destroy()
        self.child_frames.clear()

        base = os.path.dirname(info_path)
        kept = []

        # Rebuild rows, only keep those whose referenced asset still has child_only=True
        for child in data.get('child_assets', []):
            child_name = child.get('asset')
            child_info = os.path.join(ASSET_DIR, child_name, "info.json")
            valid = False
            if os.path.isfile(child_info):
                with open(child_info, 'r') as cf:
                    ci = json.load(cf)
                if ci.get('child_only', False):
                    valid = True

            if not valid:
                # orphaned: delete its geometry file
                fn = child.get('area_file')
                if fn:
                    try: os.remove(os.path.join(base, fn))
                    except: pass
                continue

            # still valid → recreate its UI row and keep
            self._add_child_row(child)
            kept.append(child)

        # Immediately write back pruned list so info.json stays in sync
        data['child_assets'] = kept
        with open(info_path, 'w') as f:
            json.dump(data, f, indent=2)

        # Finally, apply disabled/enabled based on child_only checkbox
        self._on_child_only_toggle()


    def save(self):
        """Write back child_only and child_assets (with area_file only) to info.json."""
        if not self.asset_path:
            messagebox.showerror("Error", "No asset selected.")
            return

        base = os.path.dirname(self.asset_path)
        # load existing to detect orphaned files
        with open(self.asset_path, 'r') as f:
            info = json.load(f)
        old_files = {c.get('area_file') for c in info.get('child_assets', [])}

        new_children = []
        used_files = set()

        for i, entry in enumerate(self.child_frames):
            v = entry['vars']
            geo = v['area'] or {}
            filename = f"child_{i}_area.json"

            # write out geometry JSON
            with open(os.path.join(base, filename), 'w') as gf:
                json.dump(geo, gf, indent=2)
            used_files.add(filename)

            # invert label→key for Z-Order
            zopts = entry['frame']._zopts
            inv   = {label:key for key,label in zopts.items()}
            zkey  = inv.get(v['zmode'].get(), 'same')

            new_children.append({
                'asset':     v['asset'].get(),
                'area_file': filename,
                'zmode':     zkey,
                'min':       v['min'].get(),
                'max':       v['max'].get(),
                'skew':      v['skew'].get(),
                'on_top':    v['on_top'].get()
            })

        # clean up any orphaned geometry files
        for fn in old_files - used_files:
            try:
                os.remove(os.path.join(base, fn))
            except OSError:
                pass

        # write back only references & flags
        info['child_only']   = self.child_only_var.get()
        info['child_assets'] = new_children
        with open(self.asset_path, 'w') as f:
            json.dump(info, f, indent=2)

        messagebox.showinfo("Saved", "Child-assets saved.")
