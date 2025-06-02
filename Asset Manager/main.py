# === File: main.py ===
import tkinter as tk
from map_manager_main import MapManagerApp

if __name__ == "__main__":
    root = tk.Tk()
    root.withdraw()
    MapManagerApp()
    root.mainloop()
