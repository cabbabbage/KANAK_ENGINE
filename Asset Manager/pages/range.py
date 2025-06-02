import tkinter as tk
from tkinter import ttk

class Range(ttk.Frame):
    def __init__(self, parent, min_bound=0, max_bound=100, set_min=None, set_max=None, force_fixed=False, label=None):
        super().__init__(parent)

        self.min_bound = min_bound
        self.max_bound = max_bound
        self.force_fixed = force_fixed
        self.label = label

        if set_min is None:
            set_min = min_bound
        if set_max is None:
            set_max = max_bound

        

        self.var_random = tk.BooleanVar()
        self.var_min = tk.IntVar(value=set_min)
        self.var_max = tk.IntVar(value=set_max)

        self.var_random.set(set_min != set_max and not force_fixed)
        if not self.var_random.get():
            self.var_min.set(self.var_max.get())

        if self.label:
            ttk.Label(self, text=self.label, font=('Segoe UI', 10, 'bold')).pack(anchor='w', pady=(0, 4))

        if not self.force_fixed:
            self.check = ttk.Checkbutton(self, text="Random Range", variable=self.var_random, command=self._update_mode)
            self.check.pack(anchor='w', pady=(0, 4))

        self.slider_frame = ttk.Frame(self)
        self.slider_frame.pack(fill=tk.X, expand=True)

        self._draw_sliders()

    def _clear_sliders(self):
        for child in self.slider_frame.winfo_children():
            child.destroy()


    def set_fixed(self):
        self.force_fixed = True


    def _draw_sliders(self):
        self._clear_sliders()

        if self.var_random.get():
            self._add_slider("Min:", self.var_min, self.min_bound, self.max_bound)
            self._add_slider("Max:", self.var_max, self.min_bound, self.max_bound)
        else:
            self._add_slider("Value:", self.var_max, self.min_bound, self.max_bound)

    def _add_slider(self, label_text, var, from_, to_):
        row = ttk.Frame(self.slider_frame)
        row.pack(fill=tk.X, pady=2)

        label = ttk.Label(row, text=label_text)
        label.pack(side=tk.LEFT)

        value_label = ttk.Label(row, text=str(var.get()), width=4)
        value_label.pack(side=tk.RIGHT)

        slider = ttk.Scale(
            row, from_=from_, to=to_, orient=tk.HORIZONTAL, variable=var,
            command=lambda val: value_label.config(text=str(int(float(val))))
        )
        slider.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=(8, 4))

    def _update_mode(self):
        if self.force_fixed:
            return
        if self.var_random.get():
            self.var_min.set(self.var_max.get())
        else:
            self.var_max.set(self.var_max.get())
        self._draw_sliders()

    def get(self):
        if self.var_random.get():
            return int(self.var_min.get()), int(self.var_max.get())
        else:
            v = int(self.var_max.get())
            return v, v

    def get_min(self):
        return self.get()[0]

    def get_max(self):
        return self.get()[1]

    def set(self, min_val, max_val):
        self.var_random.set(min_val != max_val and not self.force_fixed)
        self.var_min.set(min_val)
        self.var_max.set(max_val)
        self._draw_sliders()
