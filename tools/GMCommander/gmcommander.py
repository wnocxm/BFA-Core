#!/usr/bin/env python3
"""BFA-Core GM Commander: SOAP GM konsolu + veritabani bagli waypoint editoru."""
import json
import os
import queue
import threading
import tkinter as tk
from tkinter import ttk, messagebox, filedialog

from soap_client import SoapClient, SoapError
from coords import parse_coords

try:
    from db import Database, path_to_sql
    DB_IMPORT_ERROR = None
except ImportError as e:  # pymysql kurulu degil
    Database, path_to_sql, DB_IMPORT_ERROR = None, None, e

APP_DIR = os.path.dirname(os.path.abspath(__file__))
CONFIG_PATH = os.path.join(APP_DIR, "config.json")

DEFAULT_CONFIG = {
    "soap": {"host": "127.0.0.1", "port": 7878, "user": "1#1", "password": ""},
    "db": {"host": "127.0.0.1", "port": 3306, "user": "root", "password": "",
           "world_db": "world", "characters_db": "characters"},
    "auto_reload_after_save": True,
    "quick_commands": [
        ["Sunucu bilgisi", "server info"],
        ["Herkese duyuru", "announce <mesaj>"],
        ["Ekran bildirimi", "notify <mesaj>"],
        ["Herkesi kaydet", "saveall"],
        ["Waypoint reload", "reload waypoint_data"],
        ["Creature reload", "reload creature_template <entry>"],
        ["Hesap olustur", "bnetaccount create <email> <sifre>"],
        ["GM seviyesi ver", "account set gmlevel <hesap> 3 -1"],
        ["Oyuncu at (kick)", "kick <karakter>"],
        ["Hesap banla", "ban account <hesap> <sure> <sebep>"],
        ["Mail ile item", "send items <karakter> \"Konu\" \"Metin\" <itemId>:<adet>"],
        ["Para gonder", "send money <karakter> \"Konu\" \"Metin\" <bakir>"],
        ["Karakteri isinla", "tele name <karakter> <yer>"],
        ["Creature ara", "lookup creature <isim>"],
        ["Item ara", "lookup item <isim>"],
        ["Sunucu kapat (60sn)", "server shutdown 60"],
    ],
}

MOVE_TYPES = ["0 - Yuru", "1 - Kos", "2 - Yere in", "3 - Havalan"]


def load_config():
    cfg = json.loads(json.dumps(DEFAULT_CONFIG))
    if os.path.exists(CONFIG_PATH):
        try:
            with open(CONFIG_PATH, encoding="utf-8") as f:
                user = json.load(f)
            for k, v in user.items():
                if isinstance(v, dict) and isinstance(cfg.get(k), dict):
                    cfg[k].update(v)
                else:
                    cfg[k] = v
        except Exception as e:
            messagebox.showwarning("Ayarlar", f"config.json okunamadi, varsayilanlar kullaniliyor:\n{e}")
    return cfg


def save_config(cfg):
    with open(CONFIG_PATH, "w", encoding="utf-8") as f:
        json.dump(cfg, f, indent=2, ensure_ascii=False)


class App(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("BFA-Core GM Commander")
        self.geometry("1200x780")
        self.minsize(950, 620)
        self.cfg = load_config()
        self.db = Database(self.cfg["db"]) if Database else None
        self._results = queue.Queue()
        self.after(50, self._poll_results)

        self.status = tk.StringVar(value="Hazir")
        nb = ttk.Notebook(self)
        nb.pack(fill="both", expand=True, padx=6, pady=(6, 0))
        ttk.Label(self, textvariable=self.status, anchor="w", relief="sunken").pack(fill="x", side="bottom")

        self.console = ConsoleTab(nb, self)
        self.waypoints = WaypointTab(nb, self)
        self.creatures = CreatureTab(nb, self)
        self.sql = SqlTab(nb, self)
        self.settings = SettingsTab(nb, self)
        nb.add(self.console, text="  GM Konsolu (SOAP)  ")
        nb.add(self.waypoints, text="  Waypoint Editoru  ")
        nb.add(self.creatures, text="  Creature Ara  ")
        nb.add(self.sql, text="  SQL  ")
        nb.add(self.settings, text="  Ayarlar  ")
        self.nb = nb

        if DB_IMPORT_ERROR:
            self.set_status("pymysql kurulu degil: 'pip install pymysql' (DB ozellikleri kapali)")

    # --- arka plan isleri: UI donmasin ---
    def run_async(self, func, on_ok=None, on_err=None, busy="Calisiyor..."):
        self.set_status(busy)

        def worker():
            try:
                res = func()
                self._results.put((on_ok, res, None))
            except Exception as e:  # noqa
                self._results.put((on_err, None, e))

        threading.Thread(target=worker, daemon=True).start()

    def _poll_results(self):
        try:
            while True:
                cb, res, err = self._results.get_nowait()
                if err is not None:
                    self.set_status(f"Hata: {err}")
                    if cb:
                        cb(err)
                    else:
                        messagebox.showerror("Hata", str(err))
                else:
                    self.set_status("Tamam")
                    if cb:
                        cb(res)
        except queue.Empty:
            pass
        self.after(50, self._poll_results)

    def set_status(self, text):
        self.status.set(text)

    def soap(self):
        s = self.cfg["soap"]
        return SoapClient(s["host"], s["port"], s["user"], s["password"])

    def require_db(self):
        if not self.db:
            messagebox.showerror("Veritabani", "pymysql kurulu degil.\nKomut: pip install pymysql")
            return False
        if not self.db.world:
            messagebox.showerror("Veritabani", "Once Ayarlar sekmesinden veritabanina baglanin.")
            return False
        return True


# ======================================================================
class ConsoleTab(ttk.Frame):
    def __init__(self, master, app):
        super().__init__(master, padding=6)
        self.app = app
        self.history, self.hidx = [], 0

        left = ttk.Frame(self)
        left.pack(side="left", fill="both", expand=True)
        self.out = tk.Text(left, wrap="word", state="disabled", font=("Consolas", 10),
                           bg="#111", fg="#ddd", insertbackground="#ddd")
        sb = ttk.Scrollbar(left, command=self.out.yview)
        self.out.configure(yscrollcommand=sb.set)
        self.out.tag_configure("cmd", foreground="#6cf")
        self.out.tag_configure("err", foreground="#f66")
        self.out.tag_configure("ok", foreground="#ddd")
        bar = ttk.Frame(left)
        bar.pack(side="bottom", fill="x", pady=(6, 0))
        sb.pack(side="right", fill="y")
        self.out.pack(fill="both", expand=True)

        ttk.Label(bar, text=" . ").pack(side="left")
        self.entry = ttk.Entry(bar, font=("Consolas", 11))
        self.entry.pack(side="left", fill="x", expand=True)
        self.entry.bind("<Return>", lambda e: self.send())
        self.entry.bind("<Up>", lambda e: self._hist(-1))
        self.entry.bind("<Down>", lambda e: self._hist(1))
        ttk.Button(bar, text="Gonder", command=self.send).pack(side="left", padx=4)
        ttk.Button(bar, text="Temizle", command=self.clear).pack(side="left")

        right = ttk.LabelFrame(self, text="Hazir komutlar (tikla: kutuya yazar)", padding=4)
        right.pack(side="right", fill="y", padx=(6, 0))
        for label, cmd in app.cfg.get("quick_commands", []):
            ttk.Button(right, text=label, width=24,
                       command=lambda c=cmd: self.put(c)).pack(fill="x", pady=1)
        self.write("SOAP konsolu. Komutlari basinda nokta olmadan veya noktali yazabilirsiniz.\n"
                   "Ornek: server info   |   announce Merhaba   |   reload waypoint_data\n\n")
        self.entry.focus_set()

    def put(self, cmd):
        self.entry.delete(0, "end")
        self.entry.insert(0, cmd)
        self.entry.focus_set()
        if "<" in cmd:  # ilk yer tutucuyu sec
            i = cmd.index("<")
            self.entry.selection_range(i, cmd.index(">", i) + 1)
            self.entry.icursor(cmd.index(">", i) + 1)

    def _hist(self, d):
        if not self.history:
            return "break"
        self.hidx = max(0, min(len(self.history), self.hidx + d))
        self.entry.delete(0, "end")
        if self.hidx < len(self.history):
            self.entry.insert(0, self.history[self.hidx])
        return "break"

    def write(self, text, tag="ok"):
        self.out.configure(state="normal")
        self.out.insert("end", text, tag)
        self.out.see("end")
        self.out.configure(state="disabled")

    def clear(self):
        self.out.configure(state="normal")
        self.out.delete("1.0", "end")
        self.out.configure(state="disabled")

    def send(self, cmd=None):
        cmd = (cmd if cmd is not None else self.entry.get()).strip()
        if not cmd:
            return
        if cmd not in self.history[-1:]:
            self.history.append(cmd)
        self.hidx = len(self.history)
        self.entry.delete(0, "end")
        self.run(cmd)

    def run(self, cmd, on_ok=None):
        self.write(f"> {cmd}\n", "cmd")
        client = self.app.soap()

        def ok(res):
            self.write((res.rstrip() or "(bos yanit)") + "\n\n")
            if on_ok:
                on_ok(res)

        def err(e):
            self.write(f"HATA: {e}\n\n", "err")

        self.app.run_async(lambda: client.execute(cmd), ok, err, busy=f"SOAP: {cmd}")


# ======================================================================
class WaypointTab(ttk.Frame):
    FIELDS = [("position_x", "X", float), ("position_y", "Y", float), ("position_z", "Z", float),
              ("orientation", "Orientation", float), ("delay", "Bekleme (ms)", int),
              ("move_type", "Hareket", int), ("action", "Action (script id)", int),
              ("action_chance", "Action sansi %", int)]

    def __init__(self, master, app):
        super().__init__(master, padding=6)
        self.app = app
        self.points = []
        self.creature = None
        self.dirty = False

        # --- ust bar: path + creature ---
        top = ttk.Frame(self)
        top.pack(fill="x")
        ttk.Label(top, text="Path ID:").pack(side="left")
        self.path_var = tk.StringVar()
        e = ttk.Entry(top, textvariable=self.path_var, width=12)
        e.pack(side="left", padx=4)
        e.bind("<Return>", lambda ev: self.load_path())
        ttk.Button(top, text="Yukle", command=self.load_path).pack(side="left")
        ttk.Button(top, text="Path listesi...", command=self.pick_path).pack(side="left", padx=4)
        ttk.Button(top, text="Yeni bos path", command=self.new_path).pack(side="left")

        ttk.Separator(top, orient="vertical").pack(side="left", fill="y", padx=10)
        ttk.Label(top, text="Creature GUID:").pack(side="left")
        self.guid_var = tk.StringVar()
        g = ttk.Entry(top, textvariable=self.guid_var, width=14)
        g.pack(side="left", padx=4)
        g.bind("<Return>", lambda ev: self.find_creature())
        ttk.Button(top, text="Bul", command=self.find_creature).pack(side="left")
        self.creature_lbl = ttk.Label(top, text="", foreground="#06c")
        self.creature_lbl.pack(side="left", padx=8)

        cbar = ttk.Frame(self)
        cbar.pack(fill="x", pady=(4, 0))
        ttk.Button(cbar, text="Creature'in path'ini yukle", command=self.load_creature_path).pack(side="left")
        ttk.Button(cbar, text="Bu path'i creature'a ata", command=self.assign).pack(side="left", padx=4)
        ttk.Button(cbar, text="Creature'dan path'i kaldir", command=self.unassign).pack(side="left")
        self.move_first = tk.BooleanVar(value=True)
        ttk.Checkbutton(cbar, text="Spawn'i 1. noktaya tasi", variable=self.move_first).pack(side="left", padx=8)
        ttk.Button(cbar, text="Creature konumunu nokta olarak ekle",
                   command=self.add_creature_pos).pack(side="left")

        body = ttk.PanedWindow(self, orient="horizontal")
        body.pack(fill="both", expand=True, pady=6)

        # --- sol: tablo ---
        left = ttk.Frame(body)
        cols = ["point"] + [f[0] for f in self.FIELDS]
        heads = ["#", "X", "Y", "Z", "O", "Bekleme", "Hareket", "Action", "Sans"]
        self.tree = ttk.Treeview(left, columns=cols, show="headings", selectmode="extended")
        for c, h in zip(cols, heads):
            self.tree.heading(c, text=h)
            self.tree.column(c, width=48 if c in ("point", "move_type", "action", "action_chance") else 86,
                             anchor="e")
        ysb = ttk.Scrollbar(left, command=self.tree.yview)
        self.tree.configure(yscrollcommand=ysb.set)
        self.tree.pack(side="left", fill="both", expand=True)
        ysb.pack(side="left", fill="y")
        self.tree.bind("<<TreeviewSelect>>", lambda e: self._on_select())
        self.tree.bind("<Delete>", lambda e: self.delete_points())
        body.add(left, weight=3)

        # --- sag: form + onizleme ---
        right = ttk.Frame(body)
        body.add(right, weight=2)
        form = ttk.LabelFrame(right, text="Nokta", padding=6)
        form.pack(fill="x")
        self.vars = {}
        for i, (key, label, _t) in enumerate(self.FIELDS):
            ttk.Label(form, text=label).grid(row=i // 2, column=(i % 2) * 2, sticky="e", padx=(0, 4), pady=2)
            if key == "move_type":
                v = tk.StringVar(value=MOVE_TYPES[0])
                w = ttk.Combobox(form, textvariable=v, values=MOVE_TYPES, width=12, state="readonly")
            else:
                v = tk.StringVar(value="100" if key == "action_chance" else "0")
                w = ttk.Entry(form, textvariable=v, width=14)
            w.grid(row=i // 2, column=(i % 2) * 2 + 1, sticky="w", pady=2)
            self.vars[key] = v

        btns = ttk.Frame(right)
        btns.pack(fill="x", pady=4)
        for txt, cmd in [("Ekle (sona)", self.add_point), ("Secilenin altina ekle", self.insert_point),
                         ("Guncelle", self.update_point), ("Sil", self.delete_points),
                         ("Yukari", lambda: self.move(-1)), ("Asagi", lambda: self.move(1)),
                         ("Ters cevir", self.reverse)]:
            n = len(btns.grid_slaves())
            ttk.Button(btns, text=txt, command=cmd).grid(row=n // 4, column=n % 4, sticky="ew", padx=1, pady=1)
        for i in range(4):
            btns.columnconfigure(i, weight=1)

        bulk = ttk.LabelFrame(right, text="Toplu duzenleme (secili noktalar, secim yoksa hepsi)", padding=4)
        bulk.pack(fill="x")
        ttk.Button(bulk, text="Bekleme ata", command=lambda: self.bulk_set("delay")).pack(side="left")
        ttk.Button(bulk, text="Hareket tipi ata", command=lambda: self.bulk_set("move_type")).pack(side="left", padx=2)
        ttk.Button(bulk, text="Z'ye ekle (+/-)", command=self.bulk_z).pack(side="left")
        ttk.Button(bulk, text="Orientation'lari hesapla", command=self.calc_orientation).pack(side="left", padx=2)

        paste = ttk.LabelFrame(right, text="Koordinat yapistir (.gps ciktisi veya 'x y z [o]' satirlari)",
                               padding=4)
        paste.pack(fill="both", expand=False, pady=4)
        self.paste_txt = tk.Text(paste, height=5, font=("Consolas", 9))
        self.paste_txt.pack(fill="x")
        pr = ttk.Frame(paste)
        pr.pack(fill="x", pady=(4, 0))
        ttk.Button(pr, text="Listeye ekle", command=self.add_pasted).pack(side="left")
        ttk.Label(pr, text="  Karakter:").pack(side="left")
        self.char_var = tk.StringVar()
        ttk.Entry(pr, textvariable=self.char_var, width=14).pack(side="left", padx=2)
        ttk.Button(pr, text="Konumunu nokta olarak ekle", command=self.add_char_pos).pack(side="left")

        prev = ttk.LabelFrame(right, text="Onizleme (ustten gorunum, yesil = 1. nokta)", padding=2)
        prev.pack(fill="both", expand=True)
        self.canvas = tk.Canvas(prev, bg="#1b1f24", highlightthickness=0, height=200)
        self.canvas.pack(fill="both", expand=True)
        self.canvas.bind("<Configure>", lambda e: self.draw())
        self.canvas.bind("<Button-1>", self._canvas_click)

        # --- alt bar ---
        bottom = ttk.Frame(self)
        bottom.pack(fill="x")
        ttk.Button(bottom, text="DB'ye KAYDET", command=self.save).pack(side="left")
        self.auto_reload = tk.BooleanVar(value=app.cfg.get("auto_reload_after_save", True))
        ttk.Checkbutton(bottom, text="Kaydettikten sonra SOAP ile 'reload waypoint_data'",
                        variable=self.auto_reload).pack(side="left", padx=6)
        ttk.Button(bottom, text="SQL disa aktar...", command=self.export_sql).pack(side="left", padx=4)
        ttk.Button(bottom, text="Path'i DB'den sil", command=self.delete_path).pack(side="left")
        self.info = ttk.Label(bottom, text="")
        self.info.pack(side="right")
        self.refresh()

    # ----- yardimcilar -----
    def _path_id(self):
        try:
            v = int(self.path_var.get())
            if v <= 0:
                raise ValueError
            return v
        except ValueError:
            messagebox.showerror("Path ID", "Gecerli bir Path ID girin (pozitif sayi).")
            return None

    def _read_form(self):
        p = {}
        try:
            for key, label, t in self.FIELDS:
                raw = self.vars[key].get().strip().replace(",", ".")
                if key == "move_type":
                    raw = raw.split(" ")[0]
                p[key] = t(float(raw)) if t is int else t(raw or 0)
        except ValueError:
            messagebox.showerror("Nokta", f"'{label}' alani gecersiz.")
            return None
        return p

    def _fill_form(self, p):
        for key, _l, _t in self.FIELDS:
            if key == "move_type":
                mt = int(p[key])
                self.vars[key].set(MOVE_TYPES[mt] if 0 <= mt < len(MOVE_TYPES) else str(mt))
            elif key.startswith("position") or key == "orientation":
                self.vars[key].set(f"{float(p[key]):.4f}")
            else:
                self.vars[key].set(str(p[key]))

    def _selected(self):
        return sorted(int(i) for i in self.tree.selection())

    def _new_point(self, x, y, z, o=0.0):
        p = self._read_form() or {}
        return {"position_x": x, "position_y": y, "position_z": z, "orientation": o,
                "delay": p.get("delay", 0), "move_type": p.get("move_type", 0),
                "action": 0, "action_chance": 100}

    def _mark(self, dirty=True, select=None):
        self.dirty = dirty
        self.refresh(select=select)

    def refresh(self, select=None):
        self.tree.delete(*self.tree.get_children())
        for i, p in enumerate(self.points):
            self.tree.insert("", "end", iid=str(i), values=(
                i + 1, f"{p['position_x']:.3f}", f"{p['position_y']:.3f}", f"{p['position_z']:.3f}",
                f"{p['orientation']:.3f}", p["delay"], p["move_type"], p["action"], p["action_chance"]))
        if select:
            sel = [str(i) for i in select if 0 <= i < len(self.points)]
            if sel:
                self.tree.selection_set(sel)
                self.tree.see(sel[-1])
        self.info.config(text=f"{len(self.points)} nokta" + ("  (kaydedilmedi *)" if self.dirty else ""))
        self.draw()

    def _on_select(self):
        sel = self._selected()
        if len(sel) == 1:
            self._fill_form(self.points[sel[0]])
        self.draw()

    def draw(self):
        c = self.canvas
        c.delete("all")
        pts = self.points
        if not pts:
            c.create_text(10, 10, anchor="nw", fill="#888", text="Nokta yok")
            return
        w, h = max(c.winfo_width(), 50), max(c.winfo_height(), 50)
        # WoW: +X kuzey, +Y bati -> ekranda yukari = +X, sol = +Y
        xs = [p["position_x"] for p in pts]
        ys = [p["position_y"] for p in pts]
        span = max(max(xs) - min(xs), max(ys) - min(ys), 1.0)
        pad = 18
        scale = (min(w, h) - 2 * pad) / span
        cx, cy = (max(xs) + min(xs)) / 2, (max(ys) + min(ys)) / 2

        def sc(p):
            return w / 2 - (p["position_y"] - cy) * scale, h / 2 - (p["position_x"] - cx) * scale

        self._screen = [sc(p) for p in pts]
        sel = set(self._selected())
        if len(pts) > 1:
            c.create_line(*[v for xy in self._screen for v in xy], fill="#5a8", width=2)
        if self.creature:
            cr = self.creature
            sx, sy = sc({"position_x": cr["position_x"], "position_y": cr["position_y"]})
            c.create_rectangle(sx - 5, sy - 5, sx + 5, sy + 5, outline="#fc3")
        for i, (sx, sy) in enumerate(self._screen):
            col = "#3f6" if i == 0 else ("#f55" if i == len(pts) - 1 else "#9cf")
            r = 6 if i in sel else 4
            c.create_oval(sx - r, sy - r, sx + r, sy + r, fill=col, outline="#fff" if i in sel else "")
            if len(pts) <= 60:
                c.create_text(sx + 7, sy - 7, text=str(i + 1), fill="#ccc", font=("Arial", 8))
        c.create_text(w - 4, 4, anchor="ne", fill="#666", text=f"{span:.0f} yd   K↑")

    def _canvas_click(self, ev):
        if not getattr(self, "_screen", None):
            return
        d = [((sx - ev.x) ** 2 + (sy - ev.y) ** 2, i) for i, (sx, sy) in enumerate(self._screen)]
        dist, i = min(d)
        if dist < 120:
            self.tree.selection_set(str(i))
            self.tree.see(str(i))

    # ----- nokta islemleri -----
    def add_point(self):
        p = self._read_form()
        if p:
            self.points.append(p)
            self._mark(select=[len(self.points) - 1])

    def insert_point(self):
        p = self._read_form()
        if not p:
            return
        sel = self._selected()
        idx = (sel[-1] + 1) if sel else len(self.points)
        self.points.insert(idx, p)
        self._mark(select=[idx])

    def update_point(self):
        sel = self._selected()
        if len(sel) != 1:
            messagebox.showinfo("Guncelle", "Tek bir nokta secin.")
            return
        p = self._read_form()
        if p:
            self.points[sel[0]] = p
            self._mark(select=sel)

    def delete_points(self):
        sel = self._selected()
        if not sel:
            return
        for i in reversed(sel):
            del self.points[i]
        self._mark(select=[min(sel[0], len(self.points) - 1)])

    def move(self, d):
        sel = self._selected()
        if not sel or (d < 0 and sel[0] == 0) or (d > 0 and sel[-1] == len(self.points) - 1):
            return
        order = sel if d < 0 else list(reversed(sel))
        for i in order:
            self.points[i + d], self.points[i] = self.points[i], self.points[i + d]
        self._mark(select=[i + d for i in sel])

    def reverse(self):
        self.points.reverse()
        self._mark()

    def _targets(self):
        sel = self._selected()
        return sel if sel else list(range(len(self.points)))

    def bulk_set(self, key):
        p = self._read_form()
        if not p:
            return
        sel = self._selected()
        for i in self._targets():
            self.points[i][key] = p[key]
        self._mark(select=sel)

    def bulk_z(self):
        from tkinter.simpledialog import askfloat
        dz = askfloat("Z duzelt", "Z degerine eklenecek miktar (ornek 0.5 veya -1):", parent=self)
        if dz is None:
            return
        sel = self._selected()
        for i in self._targets():
            self.points[i]["position_z"] += dz
        self._mark(select=sel)

    def calc_orientation(self):
        """Her noktanin yonunu bir sonraki noktaya bakacak sekilde hesaplar."""
        import math
        n = len(self.points)
        sel = self._selected()
        for i in self._targets():
            j = i + 1 if i + 1 < n else (i - 1 if i > 0 else None)
            if j is None:
                continue
            a, b = self.points[i], self.points[j]
            ang = math.atan2(b["position_y"] - a["position_y"], b["position_x"] - a["position_x"])
            if j < i:
                ang += math.pi
            self.points[i]["orientation"] = round(ang % (2 * math.pi), 4)
        self._mark(select=sel)

    def add_pasted(self):
        coords = parse_coords(self.paste_txt.get("1.0", "end"))
        if not coords:
            messagebox.showinfo("Yapistir", "Koordinat bulunamadi.\nOrnek: 'X: 123.4 Y: 56.7 Z: 8.9' "
                                            "veya '123.4 56.7 8.9 1.57'")
            return
        start = len(self.points)
        for x, y, z, o in coords:
            self.points.append(self._new_point(x, y, z, o))
        self.paste_txt.delete("1.0", "end")
        self._mark(select=list(range(start, len(self.points))))
        self.app.set_status(f"{len(coords)} nokta eklendi")

    def add_char_pos(self):
        name = self.char_var.get().strip()
        if not name or not self.app.require_db():
            return
        client = self.app.soap()
        db = self.app.db

        def job():
            # konumun DB'ye yazilmasi icin once kaydet
            try:
                client.execute("saveall")
            except SoapError:
                pass
            return db.character_position(name)

        def ok(row):
            if not row:
                messagebox.showerror("Karakter", f"'{name}' bulunamadi.")
                return
            self.points.append(self._new_point(row["position_x"], row["position_y"], row["position_z"],
                                               row["orientation"]))
            self._mark(select=[len(self.points) - 1])
            self.app.set_status(f"{name} konumu eklendi (map {row['map']})")

        self.app.run_async(job, ok, busy="Karakter konumu aliniyor...")

    def add_creature_pos(self):
        if not self.creature:
            messagebox.showinfo("Creature", "Once bir creature bulun.")
            return
        c = self.creature
        self.points.append(self._new_point(c["position_x"], c["position_y"], c["position_z"], c["orientation"]))
        self._mark(select=[len(self.points) - 1])

    # ----- path / DB -----
    def _confirm_discard(self):
        return not self.dirty or messagebox.askyesno("Kaydedilmemis", "Kaydedilmemis degisiklikler kaybolacak. "
                                                                       "Devam edilsin mi?")

    def new_path(self):
        if not self._confirm_discard():
            return
        self.points = []
        if self.app.db and self.app.db.world:
            guid = int(self.guid_var.get()) if self.guid_var.get().strip().isdigit() else 0
            self.app.run_async(lambda: self.app.db.next_free_path_id(guid),
                               lambda pid: self.path_var.set(str(pid)))
        self._mark(False)

    def load_path(self, pid=None):
        pid = pid or self._path_id()
        if not pid or not self.app.require_db() or not self._confirm_discard():
            return

        def ok(rows):
            self.path_var.set(str(pid))
            self.points = [{k: (float(r[k]) if k.startswith("pos") or k == "orientation" else int(r[k]))
                            for k, _l, _t in self.FIELDS} for r in rows]
            self._mark(False)
            self.app.set_status(f"Path {pid}: {len(rows)} nokta yuklendi" if rows else
                                f"Path {pid} bos / yok - yeni olarak olusturulacak")

        self.app.run_async(lambda: self.app.db.get_path(pid), ok, busy=f"Path {pid} yukleniyor...")

    def pick_path(self):
        if not self.app.require_db():
            return
        PathPicker(self, self.app, self.load_path)

    def save(self):
        pid = self._path_id()
        if not pid or not self.app.require_db():
            return
        if not self.points and not messagebox.askyesno("Kaydet", "Liste bos. Path DB'den silinsin mi?"):
            return
        pts = [dict(p) for p in self.points]
        reload_after = self.auto_reload.get()

        def ok(_):
            self._mark(False)
            self.app.set_status(f"Path {pid} kaydedildi ({len(pts)} nokta)")
            if reload_after:
                self.app.console.run("reload waypoint_data")

        self.app.run_async(lambda: self.app.db.save_path(pid, pts), ok, busy="Kaydediliyor...")

    def delete_path(self):
        pid = self._path_id()
        if not pid or not self.app.require_db():
            return
        if not messagebox.askyesno("Sil", f"Path {pid} veritabanindan tamamen silinsin mi?"):
            return
        self.app.run_async(lambda: self.app.db.delete_path(pid),
                           lambda _: (self.points.clear(), self._mark(False),
                                      self.app.set_status(f"Path {pid} silindi")))

    def export_sql(self):
        pid = self._path_id()
        if not pid or path_to_sql is None:
            return
        sql = path_to_sql(pid, self.points)
        if self.creature:
            g = self.creature["guid"]
            sql += (f"INSERT INTO `creature_addon` (`guid`,`path_id`) VALUES ({g},{pid}) "
                    f"ON DUPLICATE KEY UPDATE `path_id`={pid};\n"
                    f"UPDATE `creature` SET `MovementType`=2, `spawndist`=0 WHERE `guid`={g};\n")
        f = filedialog.asksaveasfilename(defaultextension=".sql", initialfile=f"waypoint_{pid}.sql",
                                         filetypes=[("SQL", "*.sql")])
        if f:
            with open(f, "w", encoding="utf-8") as fh:
                fh.write(sql)
            self.app.set_status(f"Kaydedildi: {f}")

    # ----- creature -----
    def set_creature(self, row):
        self.creature = row
        if row:
            self.guid_var.set(str(row["guid"]))
            self.creature_lbl.config(text=f"{row['name'] or '?'} (entry {row['entry']}, map {row['map']}) | "
                                          f"path_id={row['path_id']} | MovementType={row['MovementType']}")
        else:
            self.creature_lbl.config(text="")
        self.draw()

    def find_creature(self, then=None):
        if not self.app.require_db():
            return
        try:
            guid = int(self.guid_var.get())
        except ValueError:
            messagebox.showerror("GUID", "Gecerli bir creature GUID girin.")
            return

        def ok(row):
            if not row:
                self.set_creature(None)
                messagebox.showerror("Creature", f"GUID {guid} bulunamadi.")
                return
            self.set_creature(row)
            if then:
                then(row)

        self.app.run_async(lambda: self.app.db.find_creature(guid), ok)

    def load_creature_path(self):
        def go(row):
            if row["path_id"]:
                self.load_path(int(row["path_id"]))
            else:
                if messagebox.askyesno("Path yok", "Bu creature'in path'i yok. Yeni path olusturulsun mu?"):
                    self.new_path()
        if self.creature and str(self.creature["guid"]) == self.guid_var.get().strip():
            go(self.creature)
        else:
            self.find_creature(go)

    def assign(self):
        pid = self._path_id()
        if not pid or not self.creature:
            if pid:
                messagebox.showinfo("Ata", "Once creature'i bulun.")
            return
        if self.dirty:
            messagebox.showinfo("Ata", "Once path'i kaydedin.")
            return
        first = None
        if self.move_first.get() and self.points:
            p = self.points[0]
            first = (p["position_x"], p["position_y"], p["position_z"], p["orientation"])
        guid = self.creature["guid"]

        def ok(_):
            self.app.set_status(f"Path {pid} -> creature {guid} atandi. Oyunda creature'i respawn edin.")
            self.find_creature()

        self.app.run_async(lambda: self.app.db.assign_path(guid, pid, first), ok)

    def unassign(self):
        if not self.creature:
            return
        guid = self.creature["guid"]
        if messagebox.askyesno("Kaldir", f"Creature {guid} artik path kullanmasin mi? (MovementType=0)"):
            self.app.run_async(lambda: self.app.db.unassign_path(guid), lambda _: self.find_creature())


class PathPicker(tk.Toplevel):
    def __init__(self, master, app, on_pick):
        super().__init__(master)
        self.title("Path listesi")
        self.geometry("320x460")
        self.app, self.on_pick = app, on_pick
        bar = ttk.Frame(self, padding=4)
        bar.pack(fill="x")
        self.q = tk.StringVar()
        e = ttk.Entry(bar, textvariable=self.q)
        e.pack(side="left", fill="x", expand=True)
        e.bind("<Return>", lambda ev: self.search())
        ttk.Button(bar, text="Ara", command=self.search).pack(side="left", padx=4)
        self.tree = ttk.Treeview(self, columns=("id", "n"), show="headings")
        self.tree.heading("id", text="Path ID")
        self.tree.heading("n", text="Nokta")
        self.tree.pack(fill="both", expand=True, padx=4, pady=4)
        self.tree.bind("<Double-1>", lambda ev: self.pick())
        ttk.Label(self, text="Bos arama = son 300 path. Cift tikla = yukle.").pack()
        self.search()

    def search(self):
        def ok(rows):
            self.tree.delete(*self.tree.get_children())
            for r in rows:
                self.tree.insert("", "end", values=(r["id"], r["points"]))
        self.app.run_async(lambda: self.app.db.list_paths(self.q.get().strip()), ok)

    def pick(self):
        sel = self.tree.selection()
        if sel:
            pid = int(self.tree.item(sel[0])["values"][0])
            self.destroy()
            self.on_pick(pid)


# ======================================================================
class CreatureTab(ttk.Frame):
    COLS = ["guid", "entry", "name", "map", "x", "y", "z", "MovementType", "path_id"]

    def __init__(self, master, app):
        super().__init__(master, padding=6)
        self.app = app
        bar = ttk.Frame(self)
        bar.pack(fill="x")
        ttk.Label(bar, text="Isim, entry veya GUID:").pack(side="left")
        self.q = tk.StringVar()
        e = ttk.Entry(bar, textvariable=self.q, width=30)
        e.pack(side="left", padx=4)
        e.bind("<Return>", lambda ev: self.search())
        ttk.Button(bar, text="Ara", command=self.search).pack(side="left")
        ttk.Button(bar, text="Secileni waypoint editorunde ac", command=self.open_in_editor).pack(side="left", padx=8)
        self.tree = ttk.Treeview(self, columns=self.COLS, show="headings")
        for c in self.COLS:
            self.tree.heading(c, text=c)
            self.tree.column(c, width=220 if c == "name" else 80, anchor="w" if c == "name" else "e")
        self.tree.pack(fill="both", expand=True, pady=6)
        self.tree.bind("<Double-1>", lambda ev: self.open_in_editor())

    def search(self):
        t = self.q.get().strip()
        if not t or not self.app.require_db():
            return

        def ok(rows):
            self.tree.delete(*self.tree.get_children())
            for r in rows:
                self.tree.insert("", "end", values=[r[c] for c in self.COLS])
            self.app.set_status(f"{len(rows)} sonuc")

        self.app.run_async(lambda: self.app.db.search_creatures(t), ok)

    def open_in_editor(self):
        sel = self.tree.selection()
        if not sel:
            return
        guid = self.tree.item(sel[0])["values"][0]
        wp = self.app.waypoints
        wp.guid_var.set(str(guid))
        self.app.nb.select(wp)
        wp.load_creature_path()


# ======================================================================
class SqlTab(ttk.Frame):
    def __init__(self, master, app):
        super().__init__(master, padding=6)
        self.app = app
        bar = ttk.Frame(self)
        bar.pack(fill="x")
        self.target = tk.StringVar(value="world")
        ttk.Radiobutton(bar, text="world", variable=self.target, value="world").pack(side="left")
        ttk.Radiobutton(bar, text="characters", variable=self.target, value="characters").pack(side="left")
        ttk.Button(bar, text="Calistir (Ctrl+Enter)", command=self.run).pack(side="left", padx=8)
        ttk.Label(bar, text="Dikkat: sorgular dogrudan veritabaninda calisir.", foreground="#c60").pack(side="left")
        self.txt = tk.Text(self, height=7, font=("Consolas", 10))
        self.txt.pack(fill="x", pady=4)
        self.txt.insert("1.0", "SELECT * FROM waypoint_data ORDER BY id DESC LIMIT 50;")
        self.txt.bind("<Control-Return>", lambda e: (self.run(), "break")[1])
        frame = ttk.Frame(self)
        frame.pack(fill="both", expand=True)
        self.tree = ttk.Treeview(frame, show="headings")
        xs = ttk.Scrollbar(frame, orient="horizontal", command=self.tree.xview)
        ys = ttk.Scrollbar(frame, command=self.tree.yview)
        self.tree.configure(xscrollcommand=xs.set, yscrollcommand=ys.set)
        ys.pack(side="right", fill="y")
        xs.pack(side="bottom", fill="x")
        self.tree.pack(fill="both", expand=True)

    def run(self):
        sql = self.txt.get("1.0", "end").strip().rstrip(";")
        if not sql or not self.app.require_db():
            return
        first = sql.split(None, 1)[0].lower()
        if first in ("delete", "update", "drop", "truncate", "alter", "insert", "replace") and \
                not messagebox.askyesno("Onay", f"Bu bir '{first.upper()}' sorgusu. Calistirilsin mi?"):
            return
        target = self.target.get()

        def ok(res):
            cols, data = res
            self.tree.delete(*self.tree.get_children())
            if cols is None:
                self.tree["columns"] = ()
                self.app.set_status(f"{data} satir etkilendi")
                return
            self.tree["columns"] = cols
            for c in cols:
                self.tree.heading(c, text=c)
                self.tree.column(c, width=110, anchor="w")
            for r in data[:5000]:
                self.tree.insert("", "end", values=[r[c] for c in cols])
            self.app.set_status(f"{len(data)} satir")

        self.app.run_async(lambda: self.app.db.raw(sql, target), ok)


# ======================================================================
class SettingsTab(ttk.Frame):
    def __init__(self, master, app):
        super().__init__(master, padding=12)
        self.app = app
        self.v = {}
        soap = ttk.LabelFrame(self, text="SOAP (worldserver.conf: SOAP.Enabled = 1)", padding=8)
        soap.pack(fill="x")
        self._fields(soap, "soap", [("host", "Host"), ("port", "Port"), ("user", "Hesap (GM 3)"),
                                    ("password", "Sifre")])
        sb = ttk.Frame(soap)
        sb.grid(row=10, column=0, columnspan=4, sticky="w", pady=(6, 0))
        ttk.Button(sb, text="SOAP test et (server info)", command=self.test_soap).pack(side="left")

        db = ttk.LabelFrame(self, text="Veritabani (MySQL / MariaDB)", padding=8)
        db.pack(fill="x", pady=10)
        self._fields(db, "db", [("host", "Host"), ("port", "Port"), ("user", "Kullanici"),
                                ("password", "Sifre"), ("world_db", "World DB"),
                                ("characters_db", "Characters DB (opsiyonel)")])
        dbb = ttk.Frame(db)
        dbb.grid(row=10, column=0, columnspan=4, sticky="w", pady=(6, 0))
        ttk.Button(dbb, text="Baglan", command=self.connect_db).pack(side="left")
        self.db_state = ttk.Label(dbb, text="Bagli degil", foreground="#c00")
        self.db_state.pack(side="left", padx=10)

        ttk.Button(self, text="Ayarlari kaydet (config.json)", command=self.save).pack(anchor="w")
        ttk.Label(self, foreground="#666", justify="left", text=(
            "Not: config.json sifreleri duz metin olarak saklar; dosyayi paylasmayin.\n"
            "SOAP hesabi 'account set gmlevel <hesap> 3 -1' ile yetkilendirilmis olmali "
            "(BFA'da hesap adi genelde 1#1 bicimindedir).")).pack(anchor="w", pady=8)

    def _fields(self, parent, section, fields):
        for i, (key, label) in enumerate(fields):
            ttk.Label(parent, text=label).grid(row=i // 2, column=(i % 2) * 2, sticky="e", padx=(8, 4), pady=3)
            var = tk.StringVar(value=str(self.app.cfg[section].get(key, "")))
            ttk.Entry(parent, textvariable=var, width=28, show="*" if key == "password" else "").grid(
                row=i // 2, column=(i % 2) * 2 + 1, sticky="w", pady=3)
            self.v[(section, key)] = var

    def apply(self):
        for (section, key), var in self.v.items():
            val = var.get().strip()
            self.app.cfg[section][key] = int(val) if key == "port" and val.isdigit() else val
        self.app.cfg["auto_reload_after_save"] = self.app.waypoints.auto_reload.get()

    def save(self):
        self.apply()
        try:
            save_config(self.app.cfg)
            self.app.set_status("Ayarlar kaydedildi")
        except OSError as e:
            messagebox.showerror("Kaydet", str(e))

    def test_soap(self):
        self.apply()
        self.app.nb.select(self.app.console)
        self.app.console.run("server info")

    def connect_db(self):
        self.apply()
        if not self.app.db:
            messagebox.showerror("Veritabani", f"pymysql kurulu degil: {DB_IMPORT_ERROR}\n\npip install pymysql")
            return
        self.app.db.cfg = self.app.cfg["db"]

        def ok(_):
            names = self.app.cfg["db"]["world_db"]
            if self.app.db.chars:
                names += " + " + self.app.cfg["db"]["characters_db"]
            self.db_state.config(text=f"Bagli: {names}", foreground="#080")

        def err(e):
            self.db_state.config(text="Baglanti hatasi", foreground="#c00")
            messagebox.showerror("Veritabani", str(e))

        self.app.run_async(self.app.db.connect, ok, err, busy="Veritabanina baglaniliyor...")


if __name__ == "__main__":
    App().mainloop()
