"""Capture only the owned helper window using explicit documentation game/profile paths."""
from pathlib import Path
import argparse,ctypes,json,sys,tkinter as tk
from PIL import ImageGrab
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from mt2fw.gui import Helper
parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--game',type=Path,required=True);parser.add_argument('--profile',type=Path,required=True);args=parser.parse_args()
if not (args.game/'MT2.exe').is_file() or not (args.profile/'mods').is_dir():raise RuntimeError('Expected the explicit documentation fixture game and mod profile')
root=tk.Tk();helper=Helper(root);helper.game.set(str(args.game));helper.profile.set(str(args.profile));helper.switch_language();helper.refresh()
out=ROOT/'docs/public/media';out.mkdir(parents=True,exist_ok=True);rows=[]
try:
    for tab,name in [('start','helper-install'),('create','helper-create')]:
        helper.book.select(helper.tabs[tab]);root.update()
        handle=ctypes.windll.user32.GetAncestor(root.winfo_id(),2)
        ImageGrab.grab(window=handle).save(out/(name+'.png'))
        rows.append({'id':name,'path':'public/media/'+name+'.png','locale':'en','origin':'Owned helper HWND capture','framework_version':'0.1.0','rights':'Framework UI MIT; native operating system controls retain their respective rights.'})
finally:root.destroy()
(ROOT/'artifacts').mkdir(exist_ok=True)
(ROOT/'artifacts/helper-doc-captures.json').write_text(json.dumps(rows,indent=2)+'\n',encoding='utf-8')
print('info: English install/create helper views captured without installing or changing any game/profile file')
