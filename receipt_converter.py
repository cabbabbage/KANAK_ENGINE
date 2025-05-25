import os
import io
import subprocess
import tkinter as tk
from tkinter import filedialog, messagebox
from functools import partial

from PIL import Image, ImageTk, ImageEnhance
import fitz  # PyMuPDF

from pdf2image import convert_from_path
from pdf2image.exceptions import PDFInfoNotInstalledError

from reportlab.pdfgen import canvas as pdf_canvas
from reportlab.lib.pagesizes import letter
from reportlab.lib.utils import ImageReader



images_to_convert = []
thumbnails = {}


BASE_LOG_DIR = os.path.join(os.path.expanduser("~"), ".receipt_converter")
os.makedirs(BASE_LOG_DIR, exist_ok=True)
LOG_FILE = os.path.join(BASE_LOG_DIR, "converted_images.txt")


max_size_mb = None  

FONT = ("Helvetica", 11)
PRIMARY_COLOR = "#ffc0cb"
SECONDARY_COLOR = "#e0bbff"
CONVERT_COLOR = "#b2f2bb"
THUMB_SIZE = (80, 80)

def refresh_carousel(carousel_inner, placeholder_label, canvas):
    for widget in carousel_inner.winfo_children():
        widget.destroy()

    if not images_to_convert:
        placeholder_label.pack(expand=True)
        canvas.config(scrollregion=canvas.bbox("all"))
        return
    else:
        placeholder_label.pack_forget()

    for path in images_to_convert:
        try:
            thumb_frame = tk.Frame(carousel_inner, bd=1, relief="solid", bg="white")
            thumb_frame.pack(side="left", padx=5, pady=5)

            img = Image.open(path)
            img.thumbnail(THUMB_SIZE)
            thumbnails[path] = ImageTk.PhotoImage(img)

            label_img = tk.Label(thumb_frame, image=thumbnails[path])
            label_img.pack()

            del_btn = tk.Button(thumb_frame, text="❌", command=lambda p=path: remove_image(p, carousel_inner, placeholder_label, canvas),
                                bg="white", fg="red", bd=0, font=("Helvetica", 10))
            del_btn.place(relx=1.0, rely=0.0, anchor='ne')
        except Exception as e:
            print(f"Could not preview {path}: {e}")

    carousel_inner.update_idletasks()
    canvas.config(scrollregion=canvas.bbox("all"))

def remove_image(path, carousel_inner, placeholder_label, canvas):
    if path in images_to_convert:
        images_to_convert.remove(path)
        thumbnails.pop(path, None)
        refresh_carousel(carousel_inner, placeholder_label, canvas)

def add_images(paths, carousel_inner, placeholder_label, canvas):
    # Deduplicate using set
    new_paths = [p for p in paths if p.lower().endswith(('.png', '.jpg', '.jpeg', '.bmp', '.tiff', '.gif'))]
    all_paths = set(images_to_convert + new_paths)
    images_to_convert.clear()
    images_to_convert.extend(sorted(all_paths))  # sort for consistency
    refresh_carousel(carousel_inner, placeholder_label, canvas)

def select_folder(carousel_inner, placeholder_label, canvas):
    folder_path = filedialog.askdirectory(title="Select a folder with images")
    if not folder_path:
        return

    recurse = messagebox.askyesno("Search Recursively?",
                                  "Would you like to search this folder recursively (including all subfolders)?")

    search_paths = []
    if recurse:
        for root, _, files in os.walk(folder_path):
            search_paths.extend([os.path.join(root, file) for file in files])
    else:
        search_paths = [os.path.join(folder_path, file) for file in os.listdir(folder_path)]

    add_images(search_paths, carousel_inner, placeholder_label, canvas)

def select_images(carousel_inner, placeholder_label, canvas):
    file_paths = filedialog.askopenfilenames(title="Select image files",
                                             filetypes=[("Image Files", "*.png *.jpg *.jpeg *.bmp *.tiff *.gif")])
    if file_paths:
        add_images(file_paths, carousel_inner, placeholder_label, canvas)


# at top-level, add this so convert_images can see it
max_size_mb = None  


def create_ui(root=None):
    global max_size_mb

    if root is None:
        root = tk.Tk()

    root.title("Receipt Converter")
    root.geometry("640x480")
    root.configure(bg="white")

    def styled_button(text, command, color):
        return tk.Button(root, text=text, command=command,
                         bg=color, fg="black", font=FONT,
                         activebackground=SECONDARY_COLOR,
                         relief="raised", bd=2, padx=10, pady=5)

    # Top buttons
    styled_button("📁 Select Folder",
                  lambda: select_folder(carousel_inner, placeholder_label, canvas),
                  PRIMARY_COLOR).pack(pady=5)
    styled_button("🖼️ Select Images",
                  lambda: select_images(carousel_inner, placeholder_label, canvas),
                  PRIMARY_COLOR).pack(pady=5)

    # Carousel setup
    carousel_frame = tk.Frame(root, bg="white")
    carousel_frame.pack(pady=10, fill="x")
    left_btn = tk.Button(carousel_frame, text="⬅️", font=("Helvetica",12),
                         command=lambda: canvas.xview_scroll(-3,"units"),
                         bg="white", bd=0)
    left_btn.pack(side="left")
    canvas = tk.Canvas(carousel_frame, height=110, bg="white",
                       highlightthickness=0, xscrollincrement=20)
    canvas.pack(side="left", fill="x", expand=True)
    right_btn = tk.Button(carousel_frame, text="➡️", font=("Helvetica",12),
                          command=lambda: canvas.xview_scroll(3,"units"),
                          bg="white", bd=0)
    right_btn.pack(side="right")
    canvas.configure(xscrollcommand=lambda *args: None)
    carousel_inner = tk.Frame(canvas, bg="white")
    canvas.create_window((0,0), window=carousel_inner, anchor="nw")
    placeholder_label = tk.Label(
        carousel_inner,
        text="Your selected images will appear below.",
        font=("Helvetica", 11, "italic"),
        fg="gray", bg="white",
        justify="center",    # center the text
        anchor="center"      # anchor the label itself in the middle of its space
    )
    placeholder_label.pack(fill="x", pady=5)



    # ↓ Max‐size control moved here ↓
    max_size_mb = tk.DoubleVar(root, value=0.10)
    size_frame = tk.Frame(root, bg="white")
    size_frame.pack(pady=(0, 10))
    tk.Label(size_frame, text="Max size (MB):", font=FONT, bg="white")\
      .pack(side="left")
    tk.Spinbox(
        size_frame,
        from_=0.01, to=100.0, increment=0.01,
        textvariable=max_size_mb,
        format="%.2f",
        width=6,
        font=FONT
    ).pack(side="left", padx=(5,0))

    # Convert button
    styled_button("CONVERT AND SAVE",
                  lambda: convert_images(root),
                  CONVERT_COLOR).pack(pady=5)

    root.mainloop()


def convert_images(root):
    global max_size_mb

    if not images_to_convert:
        messagebox.showwarning("No images", "No images have been added for conversion.")
        return

    # 1) Ask where to save
    output_dir = filedialog.askdirectory(title="Select a folder to save PDFs")
    if not output_dir:
        return

    # 2) Busy overlay …
    overlay = tk.Toplevel(root)
    overlay.overrideredirect(True)
    overlay.attributes("-alpha", 0.6)
    overlay.configure(bg="gray")
    overlay.geometry(f"{root.winfo_width()}x{root.winfo_height()}+"
                     f"{root.winfo_rootx()}+{root.winfo_rooty()}")
    overlay.lift(root)
    overlay.grab_set()
    tk.Label(overlay, text="Converting…", font=("Helvetica",16),
             bg="gray", fg="white").place(relx=0.5, rely=0.4, anchor="center")
    root.config(cursor="watch")
    root.update()

    try:
        # read the user’s max‐size
        MAX_BYTES = max_size_mb.get() * 1024 * 1024
        PAGE_W, PAGE_H = letter

        success_paths, failed_paths = [], []

        for file_path in images_to_convert:
            try:
                # load, grayscale, scale & pad → final_img (RGB) …
                img = Image.open(file_path).convert("L")
                PAPER_W, PAPER_H = 2480, 3508
                ir, pr = img.width/img.height, PAPER_W/PAPER_H
                if ir > pr:
                    nw, nh = PAPER_W, round(PAPER_W/ir)
                else:
                    nh, nw = PAPER_H, round(PAPER_H*ir)
                img_rs = img.resize((nw, nh), Image.LANCZOS)
                canvas_img = Image.new("L", (PAPER_W, PAPER_H), "white")
                canvas_img.paste(img_rs, ((PAPER_W-nw)//2, (PAPER_H-nh)//2))
                final_img = canvas_img.convert("RGB")

                # compress down to MAX_BYTES
                pdf_bytes = None
                quality = 100
                while True:
                    buf_img = io.BytesIO()
                    final_img.save(buf_img, "JPEG", quality=quality)
                    buf_img.seek(0)

                    buf_pdf = io.BytesIO()
                    c = pdf_canvas.Canvas(buf_pdf, pagesize=letter)
                    c.drawImage(ImageReader(buf_img),
                                0, 0, width=PAGE_W, height=PAGE_H,
                                preserveAspectRatio=True, mask='auto')
                    c.showPage(); c.save()
                    data = buf_pdf.getvalue()

                    if len(data) <= MAX_BYTES:
                        pdf_bytes = data
                        break
                    if quality > 10:
                        quality -= 10
                    else:
                        # downsample image
                        w2 = int(final_img.width * 0.9)
                        h2 = int(final_img.height * 0.9)
                        final_img = final_img.resize((w2, h2), Image.LANCZOS)
                        quality = 100

                # write PDF …
                pdf_fn = os.path.splitext(os.path.basename(file_path))[0] + ".pdf"
                pdf_path = os.path.join(output_dir, pdf_fn)
                with open(pdf_path, "wb") as f:
                    f.write(pdf_bytes)
                success_paths.append((file_path, pdf_path))

            except Exception as e:
                print(f"Failed to convert {file_path}: {e}")
                failed_paths.append(file_path)

        # append to log …
        with open(LOG_FILE, "a") as log:
            for orig, _ in success_paths:
                log.write(orig + "\n")

        # summary & finalize …
        messagebox.showinfo(
            "Conversion Complete",
            f"{len(success_paths)} saved, {len(failed_paths)} failed.\n"
            f"Location: {output_dir}"
        )
        finalize(root, success_paths, failed_paths, output_dir)

    finally:
        overlay.grab_release()
        overlay.destroy()
        root.config(cursor="")
        root.update()








def create_pdf_thumbnail(pdf_path: str, width: int, height: int) -> Image.Image:
    doc  = fitz.open(pdf_path)
    page = doc.load_page(0)
    rect = page.rect
    zoom = min(width/rect.width, height/rect.height)
    mat  = fitz.Matrix(zoom, zoom)
    pix  = page.get_pixmap(matrix=mat, alpha=False)
    img  = Image.frombytes("RGB", [pix.width, pix.height], pix.samples)

    thumb = Image.new("RGB", (width, height), "white")
    x = (width  - img.width )//2
    y = (height - img.height)//2
    thumb.paste(img, (x, y))
    return thumb


def finalize(root, successes, failures, output_dir):
    # --- Clear ---
    for w in root.winfo_children():
        w.destroy()

    # --- Constants ---
    FONT      = ("Helvetica", 11)
    FONT_S    = ("Helvetica",  9)
    SCOLOR    = "#e0bbff"
    DCOLOR    = "#ff6699"
    DELETE_COLOR    = "#ff6699"
    TW, TH    = 140, 180
    root.pdf_thumbs = {}  # hold PhotoImage refs

    # --- Helpers ---
    def open_file(p):
        try:
            if os.name == "nt": os.startfile(p)
            else: subprocess.call(["open" if os.name=="darwin" else "xdg-open", p])
        except: pass



    def edit_pdf(pdf_path):
        try:
            orig_pil = convert_from_path(
                pdf_path,
                first_page=1, last_page=1
            )[0].convert("L").convert("RGB")
        except PDFInfoNotInstalledError:
            messagebox.showerror(
                "Poppler Required",
                "Poppler is required for PDF editing; please install it\n"
                "and add it to your PATH before continuing."
            )
            return
        except Exception as e:
            messagebox.showerror("Error", f"Could not read PDF:\n{e}")
            return

        # Working copy
        working = orig_pil.copy()

        # Build the edit window
        win = tk.Toplevel(root)
        win.title(f"Edit: {os.path.basename(pdf_path)}")
        win.geometry("600x800")

        # Preview label & size label
        preview_lbl = tk.Label(win, bg="white")
        preview_lbl.pack(padx=10, pady=10, fill="both", expand=True)
        size_lbl    = tk.Label(win, text="", font=FONT_S, bg="white")
        size_lbl.pack()

        # Update functions
        def update_size():
            buf = io.BytesIO()
            working.save(buf, "PDF", quality=qual_var.get())
            mb = buf.tell()/(1024*1024)
            size_lbl.config(text=f"New size: {mb:.2f} MB")

        def update_preview():
            thumb = working.resize((TW, TH), Image.LANCZOS)
            photo = ImageTk.PhotoImage(thumb)
            preview_lbl.config(image=photo)
            preview_lbl.image = photo
            update_size()

        # Crop dialog
        def do_crop():
            cw = tk.Toplevel(win)
            cw.title("Crop")
            ratio = TW/TH
            lvar = tk.IntVar(value=0); tvar = tk.IntVar(value=0)
            wvar = tk.IntVar(value=orig_pil.width)
            # Left, Top, Width controls
            for row,(lbl,var,maxv) in enumerate([
                ("Left:", lvar, orig_pil.width),
                ("Top:",  tvar, orig_pil.height),
                ("Width:",wvar, orig_pil.width),
            ]):
                tk.Label(cw, text=lbl).grid(row=row, column=0)
                tk.Spinbox(cw, from_=0, to=maxv, textvariable=var).grid(row=row, column=1)
            def apply_crop():
                nonlocal working
                l,t,w = lvar.get(), tvar.get(), wvar.get()
                h = int(w/ratio)
                canvas = Image.new("RGB", (orig_pil.width, orig_pil.height), "white")
                region = orig_pil.crop((l,t,l+w,t+h))
                canvas.paste(region,(0,0))
                working = canvas
                update_preview()
                cw.destroy()
            tk.Button(cw, text="Apply", command=apply_crop).grid(row=3, columnspan=2,pady=10)

        tk.Button(win, text="📐 Crop", command=do_crop).pack(fill="x", padx=20, pady=5)

        # Brightness slider
        def on_bright(v):
            nonlocal working
            working = ImageEnhance.Brightness(working).enhance(float(v))
            update_preview()
        bscale = tk.Scale(win, label="Brightness", from_=0.1, to=2.0,
                        resolution=0.1, orient="horizontal", command=on_bright)
        bscale.set(1.0); bscale.pack(fill="x", padx=20)

        # Contrast slider
        def on_contrast(v):
            nonlocal working
            working = ImageEnhance.Contrast(working).enhance(float(v))
            update_preview()
        cscale = tk.Scale(win, label="Contrast", from_=0.1, to=2.0,
                        resolution=0.1, orient="horizontal", command=on_contrast)
        cscale.set(1.0); cscale.pack(fill="x", padx=20)

        # Compression slider
        qual_var = tk.IntVar(value=100)
        def on_quality(v):
            update_size()
        qscale = tk.Scale(win, label="Quality (%)", from_=10, to=100,
                        orient="horizontal", variable=qual_var,
                        command=on_quality)
        qscale.pack(fill="x", padx=20, pady=(0,10))

        # Save button
        def do_save():
            working.save(pdf_path, "PDF", quality=qual_var.get())
            win.destroy()
            finalize(root, successes, failures, output_dir)

        tk.Button(win, text="💾 Save Changes",
                bg=SECONDARY_COLOR, font=FONT,
                command=do_save).pack(fill="x", padx=20, pady=20)

        # Initial render
        update_preview()



    def delete_pdf(p, frame):
        try: os.remove(p)
        except: pass
        orig = next((o for o,pp in successes if pp == p), None)
        if orig and os.path.exists(LOG_FILE):
            lines = open(LOG_FILE).readlines()
            with open(LOG_FILE,"w") as f:
                for L in lines:
                    if L.strip() != orig:
                        f.write(L)
            successes[:] = [(o,pp) for (o,pp) in successes if pp != p]
        frame.destroy()

    def delete_current_converted():
        if not os.path.exists(LOG_FILE):
            return
        with open(LOG_FILE, "r") as f:
            all_paths = [line.strip() for line in f if line.strip()]
        if not all_paths:
            return

        sel = tk.Toplevel(root)
        sel.title("Select Originals to Delete")
        sel.geometry("600x400")

        tk.Label(sel, text="Check the files you wish to delete:", font=("Helvetica",11))\
        .pack(pady=5)

        # scrollable canvas/frame
        cvs = tk.Canvas(sel, bg="white", highlightthickness=0)
        sb  = tk.Scrollbar(sel, orient="vertical", command=cvs.yview)
        cvs.configure(yscrollcommand=sb.set)
        sb.pack(side="right", fill="y")
        cvs.pack(side="left", fill="both", expand=True)

        frm = tk.Frame(cvs, bg="white")
        cvs.create_window((0,0), window=frm, anchor="nw")

        # bind mouse wheel for Windows
        def _on_mousewheel(event):
            cvs.yview_scroll(int(-1*(event.delta/120)), "units")
        cvs.bind("<MouseWheel>", _on_mousewheel)
        # bind for Linux
        cvs.bind("<Button-4>", lambda e: cvs.yview_scroll(-1, "units"))
        cvs.bind("<Button-5>", lambda e: cvs.yview_scroll(1, "units"))

        # populate
        session_paths = {orig for orig, _ in successes}
        vars_map = {}
        for path in reversed(all_paths):
            var = tk.BooleanVar(value=(path in session_paths))
            row = tk.Frame(frm, bg="white")
            row.pack(fill="x", padx=10, pady=2)

            tk.Checkbutton(row, variable=var, bg="white").pack(side="left")
            lbl = tk.Label(
                row, text=path, fg="blue", cursor="hand2",
                font=("Helvetica",9,"underline"), bg="white",
                anchor="w", justify="left"
            )
            lbl.pack(side="left", fill="x", expand=True, padx=(5,0))
            lbl.bind("<Button-1>", lambda e, p=path: open_file(p))
            vars_map[path] = var

        frm.update_idletasks()
        cvs.config(scrollregion=cvs.bbox("all"))

        tk.Button(sel, text="Select All",
                command=lambda: [v.set(True) for v in vars_map.values()])\
        .pack(pady=5)

        def on_delete_selected():
            to_delete = [p for p,v in vars_map.items() if v.get()]
            if not to_delete:
                sel.destroy(); return
            if not messagebox.askyesno("Confirm Delete", f"Delete {len(to_delete)} originals?"):
                sel.destroy(); return

            # delete originals & prune log
            for p in to_delete:
                try: os.remove(p)
                except: pass
            with open(LOG_FILE, "r") as f:
                lines = [L for L in f if L.strip() not in to_delete]
            with open(LOG_FILE, "w") as f:
                f.writelines(lines)
            successes[:] = [(o, pdf) for (o, pdf) in successes if o not in to_delete]

            sel.destroy()
            finalize(root, successes, failures, output_dir)

        tk.Button(sel, text="Delete Selected", bg=DELETE_COLOR, fg="white",
                command=on_delete_selected).pack(pady=10)

        
        

    def reset_app():
        images_to_convert.clear()
        thumbnails.clear()
        for w in root.winfo_children(): w.destroy()
        create_ui(root)

    # --- Summary ---
    summary_text = f"{len(failures)} failed conversions." if failures else "All converted successfully."
    tk.Label(root, text=summary_text, font=FONT, bg="white").pack(pady=(10,5))

    if failures:
        fail_canvas = tk.Canvas(root, height=30, bg="white", highlightthickness=0)
        fail_frame  = tk.Frame(fail_canvas, bg="white")
        fail_canvas.create_window((0,0), window=fail_frame, anchor="nw")
        fail_canvas.pack(fill="x", padx=10)
        for p in failures:
            tk.Button(
                fail_frame,
                text=os.path.basename(p),
                font=FONT_S+("underline",),
                fg="blue", bg="white", bd=0,
                cursor="hand2",
                command=partial(open_file,p)
            ).pack(side="left", padx=5)
        fail_frame.update_idletasks()
        fail_canvas.config(scrollregion=fail_canvas.bbox("all"))

    # --- Carousel ---
    # --- Carousel ---
    container = tk.Frame(root, bg="white")
    container.pack(fill="both", expand=True, padx=10, pady=(5,0))

    tk.Button(container, text="⬅️", bg="white", bd=0,
              command=lambda: canvas.xview_scroll(-1, "units"))\
      .pack(side="left", padx=(0,5), pady=5)
    canvas = tk.Canvas(container, bg="white", highlightthickness=0)
    canvas.pack(side="left", fill="both", expand=True, pady=5)
    tk.Button(container, text="➡️", bg="white", bd=0,
              command=lambda: canvas.xview_scroll(1, "units"))\
      .pack(side="right", padx=(5,0), pady=5)

    inner = tk.Frame(canvas, bg="white")
    canvas.create_window((0,0), window=inner, anchor="nw")

    pdfs = sorted(
        os.path.join(output_dir, f)
        for f in os.listdir(output_dir)
        if f.lower().endswith(".pdf")
    )

    for pdf in pdfs:
        card = tk.Frame(inner, width=TW+20, height=TH+100,
                        bd=1, relief="solid", bg="white")
        card.pack_propagate(False)
        card.pack(side="left", padx=10, pady=5)

        # Editable title + size
        name_var = tk.StringVar(value=os.path.basename(pdf))
        title_lbl = tk.Label(card, textvariable=name_var,
                             font=("Helvetica",10,"bold"),
                             bg="white", cursor="hand2")
        title_lbl.pack(pady=(5,2), fill="x")

        mb = os.path.getsize(pdf)/(1024*1024)
        size_lbl = tk.Label(card, text=f"{mb:.2f} MB",
                            font=FONT_S, fg="gray", bg="white")
        size_lbl.pack(pady=(0,5), fill="x")

        def start_rename(event,
                         pdf_path=pdf,
                         var=name_var,
                         lbl=title_lbl,
                         sz_lbl=size_lbl,
                         crd=card,
                         out_dir=output_dir):
            lbl.pack_forget()
            entry = tk.Entry(crd, textvariable=var, font=("Helvetica",10))
            entry.pack(pady=(5,2), fill="x", padx=5, before=sz_lbl)
            entry.focus_set()

            def finish_rename(evt=None,
                              entry=entry,
                              lbl=lbl,
                              var=var,
                              pdf_path=pdf_path,
                              sz_lbl=sz_lbl,
                              crd=crd,
                              out_dir=out_dir):
                new_name = var.get().strip()
                if not new_name.lower().endswith(".pdf"):
                    new_name += ".pdf"
                new_path = os.path.join(out_dir, new_name)
                try:
                    os.rename(pdf_path, new_path)
                except Exception as e:
                    messagebox.showerror("Rename failed", str(e))
                    new_path = pdf_path
                var.set(os.path.basename(new_path))
                for i, (o, p) in enumerate(successes):
                    if p == pdf_path:
                        successes[i] = (o, new_path)
                        break
                entry.destroy()
                lbl.pack(pady=(5,2), fill="x", before=sz_lbl)

            entry.bind("<Return>", finish_rename)
            entry.bind("<FocusOut>", finish_rename)

        title_lbl.bind("<Double-Button-1>", start_rename)

        # Thumbnail + preview
        thumb_img = create_pdf_thumbnail(pdf, TW, TH)
        root.pdf_thumbs[pdf] = ImageTk.PhotoImage(thumb_img)
        preview = tk.Label(card, image=root.pdf_thumbs[pdf],
                           bg="white", cursor="hand2")
        preview.pack(fill="both", expand=True, pady=(0,5))
        preview.bind("<Button-1>", lambda e, p=pdf: open_file(p))

        # Action buttons
        btn_frame = tk.Frame(card, bg="white")
        btn_frame.pack(side="bottom", fill="x", pady=5)
        tk.Button(btn_frame, text="✏️ Edit",
                  command=partial(edit_pdf, pdf),
                  font=FONT_S, bg=SCOLOR, fg="black",
                  relief="raised", bd=1, padx=5, pady=3)\
          .pack(side="left", padx=5, expand=True, fill="x")
        tk.Button(btn_frame, text="🗑️ Delete",
                  command=partial(delete_pdf, pdf, card),
                  font=FONT_S, bg=DCOLOR, fg="white",
                  relief="raised", bd=1, padx=5, pady=3)\
          .pack(side="left", padx=5, expand=True, fill="x")

    inner.update_idletasks()
    canvas.config(scrollregion=canvas.bbox("all"))


    # --- Footer ---
    footer = tk.Frame(root, bg="white")
    footer.pack(side="bottom", fill="x", pady=10)
    tk.Button(footer, text="📂 Open PDF Folder", command=lambda: open_file(output_dir),
              font=FONT, bg="white", relief="raised", bd=2, padx=10, pady=5)\
      .pack(side="left", padx=10)
    tk.Button(footer, text="🧨 Delete Originals", command=delete_current_converted,
              font=FONT, bg=DCOLOR, fg="white",
              relief="raised", bd=2, padx=10, pady=5)\
      .pack(side="left", padx=10)
    tk.Button(footer, text="🔄 Convert More PDFs", command=reset_app,
              font=FONT, bg=SCOLOR, fg="black",
              relief="raised", bd=2, padx=10, pady=5)\
      .pack(side="left", padx=10)

    canvas.configure(xscrollcommand=lambda *a: None)
    canvas.update_idletasks()
    canvas.config(scrollregion=canvas.bbox("all"))



if __name__ == "__main__":
    root = tk.Tk()
    create_ui(root)
    root.mainloop()
