import os
import json
import tkinter as tk
from tkinter import ttk
from pages.search_rooms import SearchRoomsFrame

class RoomInfo(ttk.LabelFrame):
    """
    UI element for configuring a single room within a map layer.
    Now supports:
      - min_instances, max_instances
      - required_child selection (supports multiple)
    """
    def __init__(
        self, parent, name,
        min_instances=0, max_instances=0,
        required_children=None
    ):
        self.room_name = name
        self.name_label_var = tk.StringVar(value=name)
        self.required_children = required_children or []

        label = ttk.Label(parent, textvariable=self.name_label_var,
                          font=("Segoe UI", 11, "bold"))
        super().__init__(parent, labelwidget=label, padding=(6,4))

        self.min_var = tk.IntVar(value=min_instances)
        self.max_var = tk.IntVar(value=max_instances)

        self._build_ui()
        self.load({
            "name": name,
            "min_instances": min_instances,
            "max_instances": max_instances,
            "required_children": self.required_children
        })

    def _build_ui(self):
        btn = ttk.Button(self, text="Delete", command=self._on_delete)
        btn.pack(anchor="ne", padx=4, pady=2)

        f1 = ttk.Frame(self); f1.pack(fill="x", padx=4, pady=2)
        ttk.Label(f1, text="Min Instances:").pack(side="left")
        self.min_spin = tk.Spinbox(
            f1, from_=0, to=9999,
            textvariable=self.min_var, width=5,
            command=self._on_value_change
        )
        self.min_spin.pack(side="left", padx=4)
        self.min_spin.bind("<FocusOut>", lambda e: self._on_value_change())
        self.min_spin.bind("<Return>",   lambda e: self._on_value_change())

        f2 = ttk.Frame(self); f2.pack(fill="x", padx=4, pady=2)
        ttk.Label(f2, text="Max Instances:").pack(side="left")
        self.max_spin = tk.Spinbox(
            f2, from_=0, to=9999,
            textvariable=self.max_var, width=5,
            command=self._on_value_change
        )
        self.max_spin.pack(side="left", padx=4)
        self.max_spin.bind("<FocusOut>", lambda e: self._on_value_change())
        self.max_spin.bind("<Return>",   lambda e: self._on_value_change())

        f3 = ttk.LabelFrame(self, text="Required Children")
        f3.pack(fill="x", padx=4, pady=4)

        self.child_list_frame = ttk.Frame(f3)
        self.child_list_frame.pack(fill="x", padx=4, pady=2)
        self._refresh_required_child_list()

        ttk.Button(f3, text="Add Child", command=self._on_add_required_child).pack(anchor="w", padx=4, pady=2)

    def _refresh_required_child_list(self):
        for widget in self.child_list_frame.winfo_children():
            widget.destroy()
        for child in self.required_children:
            f = ttk.Frame(self.child_list_frame)
            f.pack(fill="x", pady=1, anchor="w")
            ttk.Label(f, text=child).pack(side="left")
            ttk.Button(f, text="✕", width=2, command=lambda c=child: self._remove_required_child(c)).pack(side="left", padx=4)

    def _remove_required_child(self, name):
        if name in self.required_children:
            self.required_children.remove(name)
            self._refresh_required_child_list()
            self._on_value_change()

    def _on_add_required_child(self):
        top = tk.Toplevel(self)
        top.title("Select Required Child")
        layer = self.master.master
        sr = SearchRoomsFrame(
            top,
            rooms_dir=layer.rooms_dir,
            on_select=lambda nm: self._on_required_selected(nm, top)
        )
        sr.pack(fill="both", expand=True)

    def _on_required_selected(self, name, popup):
        if name not in self.required_children:
            self.required_children.append(name)
            self._refresh_required_child_list()
            self._on_value_change()
        popup.destroy()

    def _on_delete(self):
        layer = self.master.master
        if hasattr(layer, 'rooms'):
            try:
                layer.rooms.remove(self)
            except ValueError:
                pass
        self.destroy()
        if hasattr(layer, 'save'):
            layer.save()

    def _on_value_change(self):
        layer = self.master.master
        if hasattr(layer, 'save'):
            layer.save()

    def get_data(self):
        return {
            "name": self.room_name,
            "min_instances": self.min_var.get(),
            "max_instances": self.max_var.get(),
            "required_children": self.required_children
        }

    def load(self, data):
        self.min_var.set(data.get("min_instances", 0))
        self.max_var.set(data.get("max_instances", 0))
        self.required_children = data.get("required_children", [])
        self._refresh_required_child_list()