import os
import random
import math
import tkinter as tk
from tkinter import ttk, messagebox
from PIL import Image, ImageTk

from pages.range import Range
from pages.search import AssetSearchWindow

SRC_DIR = "SRC"

class BatchAssetEditor(ttk.Frame):
    def _bind_range_save(self, rw: Range):
        """
        Wire up a Range widget so that any change in its min/max/random
        immediately triggers a save callback.
        """
        rw.var_min.trace_add("write", lambda *_: self._trigger_save())
        rw.var_max.trace_add("write", lambda *_: self._trigger_save())
        if hasattr(rw, "var_random"):
            rw.var_random.trace_add("write", lambda *_: self._trigger_save())

    def __init__(self, parent, save_callback=None):
        super().__init__(parent)
        self.save_callback = save_callback
        self._suspend_save = False

        self.asset_data = []
        self.sliders = []

        outer = ttk.Frame(self, padding=1, relief="solid", borderwidth=1)
        outer.pack(fill=tk.BOTH, expand=True)

        # Grid Spacing range
        self.grid_spacing = Range(
            outer,
            label="Grid Spacing",
            min_bound=0, max_bound=400,
            set_min=100, set_max=100
        )
        self.grid_spacing.pack(fill=tk.X, padx=10, pady=(6, 2))
        self._bind_range_save(self.grid_spacing)

        # Jitter range
        self.jitter = Range(
            outer,
            label="Jitter",
            min_bound=0, max_bound=200,
            set_min=0, set_max=0
        )
        self.jitter.pack(fill=tk.X, padx=10, pady=(0, 8))
        self._bind_range_save(self.jitter)

        # Asset sliders container
        self.asset_list_frame = ttk.Frame(outer)
        self.asset_list_frame.pack(fill=tk.BOTH, expand=True, padx=10)

        # Add Asset button
        btns = ttk.Frame(outer)
        btns.pack(fill=tk.X, padx=10, pady=6)
        ttk.Button(btns, text="Add Asset", command=self._add_asset).pack(side=tk.LEFT)

    def _trigger_save(self, *_):
        if not self._suspend_save and self.save_callback:
            self.save_callback()

    def _add_asset(self):
        window = AssetSearchWindow(self)
        window.wait_window()
        res = getattr(window, "selected_result", None)
        if not res: return
        kind, name = res
        is_tag = (kind == "tag")
        if any(d["name"]==name and d.get("tag")==is_tag for d in self.asset_data):
            return

        color = "#%06x" % random.randint(0, 0xFFFFFF)

        if not self.asset_data:
            # first asset: null->50, new->50
            self.asset_data = [
                {"name":"null","tag":False,"percent":50,"color":"#CCCCCC"},
                {"name":name,"tag":is_tag,"percent":50,"color":color}
            ]
        elif len(self.asset_data)==1 and self.asset_data[0]["name"]=="null":
            self.asset_data[0].update(percent=50)
            self.asset_data.append({"name":name,"tag":is_tag,"percent":50,"color":color})
        else:
            self.asset_data.append({"name":name,"tag":is_tag,"percent":5,"color":color})
            self._rebalance_percentages()

        self._refresh_ui()
        self._trigger_save()

    def _rebalance_percentages(self):
        total = sum(a["percent"] for a in self.asset_data)
        n = len(self.asset_data)
        if total==0:
            for a in self.asset_data:
                a["percent"] = 100//n
        else:
            scaled = [round(a["percent"]*100/total) for a in self.asset_data]
            diff = 100 - sum(scaled)
            if diff:
                # adjust largest
                i = max(range(n), key=lambda i:scaled[i])
                scaled[i]+=diff
            for i,a in enumerate(self.asset_data):
                a["percent"]=max(1,scaled[i])

    def _adjust_others(self, idx, new_val):
        n = len(self.asset_data)
        if n<=1:
            self.asset_data[idx]["percent"]=100
            return

        old = [a["percent"] for a in self.asset_data]
        old_total = sum(old) - old[idx]
        self.asset_data[idx]["percent"] = new_val
        remaining = 100 - new_val

        # proportional distribution
        props = []
        for j in range(n):
            if j==idx: continue
            if old_total>0:
                v = old[j]*remaining/old_total
            else:
                v = remaining/(n-1)
            props.append(v)

        # floor and track fractions
        floored = [math.floor(v) for v in props]
        fracs = [v - math.floor(v) for v in props]
        s = sum(floored)
        diff = remaining - s

        # allocate diff by largest fractions (or smallest if negative)
        if diff>0:
            order = sorted(range(len(fracs)), key=lambda i:fracs[i], reverse=True)
            for k in order[:diff]:
                floored[k]+=1
        elif diff<0:
            order = sorted(range(len(fracs)), key=lambda i:fracs[i])
            for k in order[: -diff]:
                floored[k]=max(1,floored[k]-1)

        # ensure all >=1
        for k in range(len(floored)):
            floored[k]=max(1,floored[k])
        # final fix if still off
        s = sum(floored)
        diff = remaining - s
        for _ in range(abs(diff)):
            k = random.randrange(len(floored))
            if diff>0:
                floored[k]+=1
            elif floored[k]>1:
                floored[k]-=1

        # write back
        it=0
        for j in range(n):
            if j==idx: continue
            self.asset_data[j]["percent"] = floored[it]
            it+=1

    def _remove_asset(self, idx):
        if 0<=idx<len(self.asset_data):
            del self.asset_data[idx]
        if not self.asset_data:
            self.asset_data=[{"name":"null","tag":False,"percent":100,"color":"#CCCCCC"}]
        else:
            self._rebalance_percentages()
        self._refresh_ui()
        self._trigger_save()

    def _refresh_ui(self):
        for w in self.asset_list_frame.winfo_children():
            w.destroy()
        self.sliders.clear()

        n = len(self.asset_data)
        for i,asset in enumerate(self.asset_data):
            frame = ttk.Frame(self.asset_list_frame)
            frame.pack(fill=tk.X,pady=4)

            left = ttk.Frame(frame)
            left.pack(side=tk.LEFT,padx=(0,10))
            if asset.get("tag") or asset["name"]=="null":
                lbl = tk.Label(left,text="#",font=("Segoe UI",28,"bold"),width=2)
                lbl.pack()
            else:
                path = os.path.join(SRC_DIR,asset["name"],"default","0.png")
                if os.path.exists(path):
                    try:
                        img=Image.open(path)
                        img.thumbnail((48,48),Image.Resampling.LANCZOS)
                        ph=ImageTk.PhotoImage(img)
                        lbl=tk.Label(left,image=ph); lbl.image=ph; lbl.pack()
                    except:
                        tk.Label(left,text="?").pack()
                else:
                    tk.Label(left,text="?").pack()

            ttk.Label(frame,text=asset["name"],width=20).pack(side=tk.LEFT)

            slider = tk.Scale(frame,from_=1,to=100,orient=tk.HORIZONTAL,
                              resolution=1,length=200,
                              bg=asset["color"],
                              troughcolor=asset["color"])
            slider.set(asset["percent"])
            slider.pack(side=tk.LEFT,padx=6,fill=tk.X,expand=True)

            # on release adjust others
            def on_release(event=None, idx=i, s=slider):
                val = s.get()
                max_allowed = 100 - (n-1)*1
                val = max(1,min(val,max_allowed))
                s.set(val)
                self._adjust_others(idx,val)
                self._refresh_ui()
                self._trigger_save()

            slider.bind("<ButtonRelease-1>", on_release)

            # remove button
            ttk.Button(frame,text="Delete",width=3,command=lambda idx=i:self._remove_asset(idx)
            ).pack(side=tk.RIGHT,padx=(10,0))

            self.sliders.append((frame,slider))
    def save(self):
        return {
            "has_batch_assets": any(a["name"]!="null" for a in self.asset_data),
            "grid_spacing_min": self.grid_spacing.get_min(),
            "grid_spacing_max": self.grid_spacing.get_max(),
            "jitter_min": self.jitter.get_min(),
            "jitter_max": self.jitter.get_max(),
            "batch_assets": [
                {"name":a["name"],"tag":a.get("tag",False),
                 "percent":a["percent"],"color":a["color"]}
                for a in self.asset_data
            ]
        }
    def load(self,data=None):
        self._suspend_save=True
        self.asset_data=[]; self.sliders.clear()
        for w in self.asset_list_frame.winfo_children(): w.destroy()
        if not isinstance(data,dict): data={}
        self.grid_spacing.set(data.get("grid_spacing_min",100),
                              data.get("grid_spacing_max",100))
        self.jitter.set(data.get("jitter_min",0),
                        data.get("jitter_max",0))
        loaded=data.get("batch_assets",[])
        if isinstance(loaded,list) and loaded:
            self.asset_data=[{
                "name":a["name"],"tag":a.get("tag",False),
                "percent":a.get("percent",0),
                "color":a.get("color","#%06x"%random.randint(0,0xFFFFFF))
            } for a in loaded]
            self._rebalance_percentages()
        else:
            self.asset_data=[{"name":"null","tag":False,"percent":100,"color":"#CCCCCC"}]
        self._refresh_ui()
        self._suspend_save=False
