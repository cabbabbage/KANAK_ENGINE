import os
import json
import tkinter as tk
from tkinter import ttk, messagebox
from pages.assets_editor import AssetEditor  # Ensure this class is defined properly

class MapAssetsPage(ttk.Frame):
    def __init__(self, parent):
        super().__init__(parent)
        self.data = {"assets": []}
        self.json_path = None

        def get_asset_list():
            return self.data.get("assets", [])

        def set_asset_list(new_list):
            self.data["assets"] = new_list

        def save_callback():
            if not self.json_path:
                return
            try:
                with open(self.json_path, "w") as f:
                    json.dump(self.data, f, indent=2)
            except Exception as e:
                messagebox.showerror("Save Failed", str(e))

        self.editor = AssetEditor(
            self,
            get_asset_list=get_asset_list,
            set_asset_list=set_asset_list,
            save_callback=save_callback
        )
        self.editor.pack(fill=tk.BOTH, expand=True, padx=40, pady=10)

    def load_data(self, data, json_path=None):
        self.data = data or {"assets": []}
        self.json_path = json_path
        self.editor.refresh()  # Update UI with loaded data

    def get_data(self):
        return self.data

    @staticmethod
    def get_json_filename():
        return "map_assets.json"
