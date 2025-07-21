import os
import json
import tkinter as tk
from tkinter import ttk, messagebox
from pages.map_render import MapRenderer
from pages.map_layer_info import MapLayerInfo


class MapInfoPage(ttk.Frame):
    @staticmethod
    def get_json_filename():
        return "map_info.json"

    def __init__(self, parent):
        super().__init__(parent)
        self.rooms_dir = None
        self.json_path = None
        self.map_data = {}
        self.layers_data = []
        self.layer_widgets = []
        self.factor = 40.0

        ttk.Button(self, text="Add New Layer", command=self._on_add_layer).pack(pady=(5, 10))

        container = ttk.Frame(self)
        container.pack(fill='both', expand=True)
        canvas = tk.Canvas(container, height=300)
        h_scroll = ttk.Scrollbar(container, orient='horizontal', command=canvas.xview)
        v_scroll = ttk.Scrollbar(container, orient='vertical', command=canvas.yview)
        canvas.configure(xscrollcommand=h_scroll.set, yscrollcommand=v_scroll.set)

        canvas.pack(side='left', fill='both', expand=True)
        v_scroll.pack(side='right', fill='y')
        h_scroll.pack(side='bottom', fill='x')

        self.layers_frame = ttk.Frame(canvas)
        canvas.create_window((0, 0), window=self.layers_frame, anchor='nw')
        self.layers_frame.bind('<Configure>', lambda e: canvas.configure(scrollregion=canvas.bbox('all')))

        preview_frame = ttk.Frame(self)
        preview_frame.pack(pady=20)

        self.PREVIEW_SIZE = 500
        self.preview_canvas = tk.Canvas(preview_frame, width=self.PREVIEW_SIZE, height=self.PREVIEW_SIZE, bg="white")
        self.preview_canvas.pack(side='left')

        ttk.Button(preview_frame,
                   text="Generate Random Preview",
                   command=self.calculate_radii).pack(
            side='left',
            padx=10,
            anchor='n'
        )

        self.renderer = None

    def load_data(self, data, json_path):
        self.json_path = json_path
        base = os.path.dirname(json_path)
        self.rooms_dir = os.path.join(base, 'rooms')
        os.makedirs(self.rooms_dir, exist_ok=True)

        try:
            self.map_data = data or {}
            self.layers_data = self.map_data.get('map_layers', [])
            self._render_layers()

            self.renderer = MapRenderer(
                rooms_dir=self.rooms_dir,
                preview_canvas=self.preview_canvas,
                preview_size=self.PREVIEW_SIZE,
                layer_widgets=self.layer_widgets,
                factor=self.factor
            )

            self.calculate_radii()  # calculate and update layer radius right after loading

        except Exception as e:
            messagebox.showerror("Load Failed", f"Could not load map_info.json:\n{e}")
            print(f"Error loading map_info.json: {e}")
            self.map_data = {}
            self.layers_data = []
            self.layer_widgets.clear()
        self.save()    

    def _render_layers(self):
        for w in self.layers_frame.winfo_children():
            w.destroy()
        self.layer_widgets.clear()
        for ld in self.layers_data:
            self._add_layer_widget(ld)

    def _add_layer_widget(self, layer_data):
        layer = MapLayerInfo(
            self.layers_frame,
            level=layer_data.get("level", 0),
            rooms_dir=self.rooms_dir,
            name=layer_data.get("name", ""),
            radius=layer_data.get("radius", 0),
            rooms_data=layer_data.get("rooms", []),
            save_callback=self.save,
            delete_callback=self._on_delete_layer
        )

        if "min_rooms" in layer_data:
            layer.min_rooms_var.set(layer_data["min_rooms"])
        if "max_rooms" in layer_data:
            layer.max_rooms_var.set(layer_data["max_rooms"])

        layer.bg_frame.pack(side='left', fill='y', padx=6, pady=6)
        self.layer_widgets.append(layer)

    def _on_add_layer(self):
        lvl = len(self.layer_widgets)
        new_data = {'level': lvl, 'name': f"layer_{lvl}", 'radius': 0, 'rooms': []}
        self._add_layer_widget(new_data)
        self._reorder_levels()
        self.save()

    def _on_delete_layer(self, lw):
        if lw in self.layer_widgets:
            self.layer_widgets.remove(lw)
        self._reorder_levels()
        self.save()

    def _reorder_levels(self):
        for i, layer in enumerate(self.layer_widgets):
            layer.set_level(i)

    def save(self):
        if not self.renderer:
            return

        self.renderer.calculate_radii()

        for i, lw in enumerate(self.layer_widgets):
            if i < len(self.renderer.layer_radii):
                lw.set_radius(self.renderer.layer_radii[i])

        self.map_data['map_layers'] = [
            self._layer_to_data(lw) for lw in self.layer_widgets
        ]

        # Save map_radius from renderer
        if hasattr(self.renderer, 'map_radius_actual'):
            self.map_data['map_radius'] = self.renderer.map_radius_actual

        with open(self.json_path, 'w') as f:
            json.dump(self.map_data, f, indent=2)

        print(f"Saved map_layers and map_radius to {self.json_path}")


    def _layer_to_data(self, layer):
        data = layer.get_data()
        data["min_rooms"] = layer.min_rooms_var.get()
        data["max_rooms"] = layer.max_rooms_var.get()
        data["radius"] = layer.radius_var.get()
        return data

    def calculate_radii(self):
        if not self.renderer:
            return
        self.renderer.calculate_radii()
        # propagate updated radii back to each layer widget
        for i, lw in enumerate(self.layer_widgets):
            if i < len(self.renderer.layer_radii):
                lw.set_radius(self.renderer.layer_radii[i])
