"""Portable desktop helper. Package operations are shared with the CLI."""
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
import json
import os
import tkinter as tk
from tkinter import ttk, filedialog, messagebox, simpledialog
from . import packages, install

WORDS = {
    'de': {
        'start': 'Installation', 'mods': 'Mods', 'create': 'Mod erstellen', 'diagnose': 'Diagnose',
        'game': 'Spielordner', 'profile': 'Mod-Profil', 'browse': 'Auswählen',
        'install': 'Framework installieren', 'remove': 'Framework entfernen', 'zip': 'Mod-ZIP installieren',
        'refresh': 'Aktualisieren', 'check': 'Mod prüfen', 'pack': 'Mod als ZIP packen',
        'template': 'Vorlage', 'identifier': 'Mod-ID', 'new': 'Mod erstellen',
        'open': 'Profilordner öffnen', 'docs': 'Dokumentation öffnen', 'save': 'Spielstand als Kopie übernehmen',
        'ready': 'Bereit', 'working': 'Vorgang läuft', 'done': 'Vorgang abgeschlossen', 'error': 'Fehler',
        'restart': 'Änderungen an installierten Paketen benötigen einen Spielneustart.',
        'supported': 'Windows x64 / Steam | Lua API 1.0.0', 'language': 'English',
        'import': 'GLB importieren', 'preview': 'Modellvorschau', 'scale': 'Maßstab', 'export': 'Inhaltspaket erstellen',
        'type': 'Typ', 'state': 'Status', 'installed': 'Installiert',
        'classic': 'Klassische Mod', 'unmanaged': 'Erkannt, separat verwaltet',
        'preview_help': 'Ziehen zum Drehen | Ursprung und Grenzen entsprechen dem konvertierten Modell',
    },
    'en': {
        'start': 'Installation', 'mods': 'Mods', 'create': 'Create a mod', 'diagnose': 'Diagnostics',
        'game': 'Game directory', 'profile': 'Mod profile', 'browse': 'Browse',
        'install': 'Install framework', 'remove': 'Remove framework', 'zip': 'Install mod ZIP',
        'refresh': 'Refresh', 'check': 'Check mod', 'pack': 'Pack mod ZIP',
        'template': 'Template', 'identifier': 'Mod ID', 'new': 'Create mod',
        'open': 'Open profile folder', 'docs': 'Open documentation', 'save': 'Import a copy of a save',
        'ready': 'Ready', 'working': 'Operation in progress', 'done': 'Operation complete', 'error': 'Error',
        'restart': 'Changes to installed packages require restarting the game.',
        'supported': 'Windows x64 / Steam | Lua API 1.0.0', 'language': 'Deutsch',
        'import': 'Import GLB', 'preview': 'Model preview', 'scale': 'Scale', 'export': 'Create content package',
        'type': 'Type', 'state': 'State', 'installed': 'Installed',
        'classic': 'Classic mod', 'unmanaged': 'Detected, managed separately',
        'preview_help': 'Drag to rotate | Origin and bounds match the converted model',
    },
}


class Helper:
    def __init__(self, root):
        self.root = root
        self.language = 'de'
        self.executor = ThreadPoolExecutor(max_workers=1)
        self.game = tk.StringVar()
        self.profile = tk.StringVar()
        self.identifier = tk.StringVar(value='author.my_mod')
        self.template = tk.StringVar(value='Hello MT2')
        self.state = tk.StringVar()
        self.labels = []
        games = install.steam_games()
        if len(games) == 1:
            self.game.set(str(games[0]))
            receipt = games[0] / install.RECEIPT
            self.profile.set(read['profile'] if receipt.is_file() and (read := packages.read_json(receipt)) else str(install.default_profile(games[0])))
        self.build()

    def word(self, key):
        return WORDS[self.language][key]

    def label(self, parent, key, **kwargs):
        widget = ttk.Label(parent, text=self.word(key), **kwargs)
        self.labels.append((widget, key))
        return widget

    def button(self, parent, key, command):
        widget = ttk.Button(parent, text=self.word(key), command=lambda:self.guard(command))
        self.labels.append((widget, key))
        return widget

    def guard(self, operation):
        try:return operation()
        except Exception as error:
            self.state.set(self.word('error'))
            messagebox.showerror(self.word('error'),str(error),parent=self.root)

    def build(self):
        self.root.title('MT2 Mod Framework')
        self.root.geometry('920x700')
        self.root.minsize(720, 550)
        style = ttk.Style(self.root)
        style.theme_use('clam')
        style.configure('.', background='#162233', foreground='#e4edf6', fieldbackground='#22334a', font=('Segoe UI', 11))
        style.configure('TButton', padding=(12, 8))
        style.map('TButton', background=[('active', '#344b68')])
        style.configure('TNotebook.Tab', background='#22334a', foreground='#e4edf6', padding=(14,8))
        style.map('TNotebook.Tab', background=[('selected','#344b68'),('active','#2a405c')], foreground=[('selected','#ffffff')])
        style.map('TCombobox', fieldbackground=[('readonly','#22334a')], foreground=[('readonly','#e4edf6')])
        style.configure('Treeview', rowheight=30, background='#162233', fieldbackground='#162233')
        self.root.configure(bg='#162233')
        main = ttk.Frame(self.root, padding=24)
        main.pack(fill='both', expand=True)
        title = ttk.Frame(main)
        title.pack(fill='x')
        ttk.Label(title, text='MT2 Mod Framework', font=('Segoe UI', 22, 'bold')).pack(side='left')
        self.button(title, 'language', self.switch_language).pack(side='right')
        self.label(main, 'supported').pack(anchor='w', pady=(6, 18))
        ttk.Label(main, textvariable=self.state).pack(side='bottom', anchor='w', pady=(14, 0))
        self.book = ttk.Notebook(main)
        self.book.pack(fill='both', expand=True)
        self.tabs = {}
        for key in ('start', 'mods', 'create', 'diagnose'):
            frame = ttk.Frame(self.book, padding=18)
            self.book.add(frame, text=self.word(key))
            self.tabs[key] = frame
        setup = self.tabs['start']
        for key, variable in (('game', self.game), ('profile', self.profile)):
            self.label(setup, key).pack(anchor='w')
            row = ttk.Frame(setup)
            row.pack(fill='x', pady=(4, 18))
            ttk.Entry(row, textvariable=variable).pack(side='left', fill='x', expand=True)
            self.button(row, 'browse', lambda v=variable: self.choose(v)).pack(side='right', padx=(8, 0))
        self.button(setup, 'install', self.install_framework).pack(anchor='w', pady=6)
        self.button(setup, 'remove', self.remove_framework).pack(anchor='w', pady=6)
        self.button(setup, 'save', self.import_save).pack(anchor='w', pady=6)
        self.button(setup, 'docs', self.open_docs).pack(anchor='w', pady=6)
        mods = self.tabs['mods']
        self.tree = ttk.Treeview(mods, columns=('version', 'type', 'state'), show='tree headings')
        self.tree.heading('#0', text='Mod')
        for key in ('version', 'type', 'state'):
            self.tree.heading(key, text='Version' if key=='version' else self.word(key))
            self.tree.column(key, width=240 if key=='state' else 120)
        self.tree.pack(fill='both', expand=True)
        row = ttk.Frame(mods)
        row.pack(fill='x', pady=12)
        self.button(row, 'zip', self.add_mod).pack(side='left')
        self.button(row, 'refresh', self.refresh).pack(side='left', padx=8)
        self.button(row, 'open', self.open_profile).pack(side='right')
        self.label(mods, 'restart', wraplength=720).pack(anchor='w')
        create = self.tabs['create']
        self.label(create, 'identifier').pack(anchor='w')
        ttk.Entry(create, textvariable=self.identifier).pack(fill='x', pady=(4, 18))
        self.label(create, 'template').pack(anchor='w')
        ttk.Combobox(create, textvariable=self.template, values=('Hello MT2', 'World Inspector', 'Scenery Pack', 'C++ Plugin'), state='readonly',width=28).pack(anchor='w', pady=(4, 18))
        self.button(create, 'new', self.create_mod).pack(anchor='w', pady=6)
        self.button(create, 'check', self.check_mod).pack(anchor='w', pady=6)
        self.button(create, 'pack', self.pack_mod).pack(anchor='w', pady=6)
        self.button(create, 'import', self.import_glb).pack(anchor='w', pady=6)
        diagnose = self.tabs['diagnose']
        self.button(diagnose, 'refresh', self.diagnose).pack(anchor='w', pady=(0, 12))
        self.output = tk.Text(diagnose, bg='#101824', fg='#e4edf6', insertbackground='#e4edf6', wrap='word', font=('Segoe UI', 10))
        self.output.pack(fill='both', expand=True)
        self.state.set(self.word('ready'))

    def switch_language(self):
        self.language = 'en' if self.language == 'de' else 'de'
        for widget, key in self.labels:
            widget.configure(text=self.word(key))
        for key, frame in self.tabs.items():
            self.book.tab(frame, text=self.word(key))
        for key in ('type','state'):self.tree.heading(key,text=self.word(key))
        self.refresh()
        self.state.set(self.word('ready'))

    def choose(self, variable):
        result = filedialog.askdirectory(parent=self.root)
        if result:
            variable.set(result)

    def paths(self):
        if not self.game.get() or not self.profile.get():
            raise packages.PackageError(self.word('game') + ' / ' + self.word('profile'))
        return Path(self.game.get()), Path(self.profile.get())

    def run(self, operation):
        self.state.set(self.word('working'))
        future = self.executor.submit(operation)

        def finished():
            if not future.done():
                self.root.after(100, finished)
                return
            try:
                result = future.result()
                self.output.delete('1.0', 'end')
                self.output.insert('end', json.dumps(result, ensure_ascii=False, indent=2))
                self.state.set(self.word('done'))
                self.refresh()
            except Exception as error:
                self.state.set(self.word('error'))
                messagebox.showerror(self.word('error'), str(error), parent=self.root)
        self.root.after(100, finished)

    def install_framework(self):
        import sys
        bundled=Path(sys.executable).parent.parent if getattr(sys,'frozen',False) else packages.FRAMEWORK/'dist'
        package = str(bundled) if (bundled/'framework-package.json').is_file() else filedialog.askdirectory(parent=self.root, title='Framework package')
        if package:
            game, profile = self.paths()
            self.run(lambda: install.install(Path(package), game, profile))

    def remove_framework(self):
        game, _ = self.paths()
        self.run(lambda: install.uninstall(game))

    def add_mod(self):
        archive = filedialog.askopenfilename(parent=self.root, filetypes=[('Mod ZIP', '*.zip')])
        if archive:
            _, profile = self.paths()
            self.run(lambda: packages.install_mod(Path(archive), profile))

    def create_mod(self):
        parent = filedialog.askdirectory(parent=self.root)
        if parent:
            identifier = self.identifier.get()
            template = {'Hello MT2':'hello','World Inspector':'inspector','Scenery Pack':'scenery','C++ Plugin':'native'}[self.template.get()]
            self.run(lambda: packages.new_mod(Path(parent) / identifier, identifier, template))

    def check_mod(self):
        directory = filedialog.askdirectory(parent=self.root)
        if directory:
            self.run(lambda: packages.check(Path(directory)))

    def pack_mod(self):
        directory = filedialog.askdirectory(parent=self.root)
        if not directory:
            return
        output = filedialog.asksaveasfilename(parent=self.root, defaultextension='.zip', filetypes=[('Mod ZIP', '*.zip')])
        if output:
            self.run(lambda: packages.pack(Path(directory), Path(output)))

    def refresh(self):
        for item in self.tree.get_children():
            self.tree.delete(item)
        mods = Path(self.profile.get()) / 'mods'
        if not self.profile.get():
            return
        for directory in sorted(mods.iterdir()) if mods.is_dir() else []:
            if directory.is_dir():
                try:
                    info = packages.check(directory)['manifest']
                    kind={'lua':'Lua','native':'C++','content':'Inhaltspaket' if self.language=='de' else 'Content pack'}[info['type']]
                    self.tree.insert('', 'end', text=info['name'], values=(info['version'], kind, self.word('installed')))
                except (OSError, ValueError, KeyError) as error:
                    self.tree.insert('', 'end', text=directory.name, values=('', '', str(error)))
        for item in install.classic_mods(Path(self.game.get()) if self.game.get() else None,Path(self.profile.get())):
            self.tree.insert('', 'end', text=item['name'],values=('',self.word('classic'),self.word('unmanaged')))

    def diagnose(self):
        game, profile = self.paths()
        self.run(lambda: install.doctor(game, profile))

    def open_profile(self):
        _, profile = self.paths()
        profile.mkdir(parents=True, exist_ok=True)
        os.startfile(profile)

    def open_docs(self):
        os.startfile(packages.FRAMEWORK / 'docs' / 'quickstart.html')

    def import_save(self):
        source = filedialog.askopenfilename(parent=self.root, filetypes=[('MT2 save', '*.vrz')])
        if source:
            world = simpledialog.askstring('World', 'Name for the copied world', parent=self.root)
            if world:
                _, profile = self.paths()
                self.run(lambda: install.import_save(Path(source), profile, world))

    def import_glb(self):
        source=filedialog.askopenfilename(parent=self.root,filetypes=[('glTF binary','*.glb')])
        if not source:
            return
        from .glb import GLB,export
        from .preview import render
        from PIL import ImageTk
        reader=GLB(Path(source))
        mesh=reader.mesh()
        dialog=tk.Toplevel(self.root)
        dialog.title(self.word('preview'))
        dialog.configure(bg='#162233')
        body=ttk.Frame(dialog,padding=20)
        body.pack(fill='both',expand=True)
        ttk.Label(body,text=Path(source).name,font=('Segoe UI',16,'bold')).pack(anchor='w')
        stats=ttk.Label(body,text=(f'{mesh.triangles} Dreiecke | {len(mesh.fragments)} Materialien | {len(mesh.images)} Texturen' if self.language=='de' else f'{mesh.triangles} triangles | {len(mesh.fragments)} materials | {len(mesh.images)} textures'))
        stats.pack(anchor='w',pady=(4,12))
        canvas=tk.Canvas(body,width=440,height=440,bg='#101824',highlightthickness=0)
        canvas.pack()
        view={'yaw':0.7,'pitch':0.35,'photo':None,'drag':None}
        def draw():
            if not dialog.winfo_exists():return
            view['photo']=ImageTk.PhotoImage(render(mesh,view['yaw'],view['pitch']))
            canvas.delete('all');canvas.create_image(0,0,anchor='nw',image=view['photo'])
        def drag_start(event):view['drag']=(event.x,event.y)
        def drag_end(event):
            if view['drag'] is not None:
                x,y=view['drag'];view['yaw']+=(event.x-x)/180;view['pitch']=max(-1.3,min(1.3,view['pitch']+(event.y-y)/180));view['drag']=None;draw()
        canvas.bind('<ButtonPress-1>',drag_start)
        canvas.bind('<ButtonRelease-1>',drag_end)
        ttk.Label(body,text=self.word('preview_help'),wraplength=440).pack(anchor='w',pady=8)
        row=ttk.Frame(body);row.pack(fill='x',pady=8)
        ttk.Label(row,text=self.word('scale')).pack(side='left')
        factor=tk.StringVar(value='1.0')
        ttk.Entry(row,textvariable=factor,width=12).pack(side='left',padx=8)
        def update_scale():
            nonlocal mesh
            mesh=reader.mesh(float(factor.get()));draw()
        ttk.Button(row,text=self.word('refresh'),command=lambda:self.guard(update_scale)).pack(side='left')
        warning=ttk.Label(body,text='\n'.join(mesh.warnings),wraplength=440)
        warning.pack(anchor='w')
        def save_package():
            identifier=simpledialog.askstring(self.word('identifier'),'author.my_scenery',initialvalue=self.identifier.get(),parent=dialog)
            if not identifier:return
            parent=filedialog.askdirectory(parent=dialog)
            if not parent:return
            result=export(mesh,Path(parent)/identifier,identifier)
            dialog.destroy();self.output.delete('1.0','end');self.output.insert('end',json.dumps(result,indent=2));self.state.set(self.word('done'));self.refresh()
        ttk.Button(body,text=self.word('export'),command=lambda:self.guard(save_package)).pack(anchor='w',pady=12)
        draw()


def main():
    root = tk.Tk()
    helper = Helper(root)
    root.report_callback_exception = lambda cls, value, tb: messagebox.showerror(helper.word('error'), str(value), parent=root)
    root.mainloop()

