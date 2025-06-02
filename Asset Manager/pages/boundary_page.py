import os
import json
import tkinter as tk
from tkinter import ttk, messagebox
from pages.assets_editor import AssetEditor  # Reusable asset editor

class BoundaryPage(ttk.Frame):
    def __init__(self, parent):
        super().__init__(parent)
        self.data = {"assets": []}
        self.json_path = None

        # Asset Editor Component
        self.asset_editor = AssetEditor(
            parent=self,
            get_asset_list=lambda: self.data.get("assets", []),
            set_asset_list=lambda new_assets: self.data.update({"assets": new_assets}),
            save_callback=self._save_json
        )
        self.asset_editor.pack(fill=tk.BOTH, expand=True, padx=20, pady=10)

    def load_data(self, data, json_path=None):
        self.data = data or {"assets": []}
        self.json_path = json_path
        self.asset_editor.reload()

    def get_data(self):
        return {"assets": self.asset_editor.get_assets()}

    def _save_json(self):
        if not self.json_path:
            return
        try:
            with open(self.json_path, "w") as f:
                json.dump(self.get_data(), f, indent=2)
        except Exception as e:
            messagebox.showerror("Save Failed", str(e))

    @staticmethod
    def get_json_filename():
        return "map_boundary.json"
