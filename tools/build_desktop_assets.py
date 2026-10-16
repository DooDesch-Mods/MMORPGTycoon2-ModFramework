"""Rasterize the existing polygon logo and author the native Mods window."""
from pathlib import Path
import re
import textwrap
import xml.etree.ElementTree as ET
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
DEST = ROOT / 'adapters/mt2/ui'
DEST.mkdir(parents=True, exist_ok=True)
image = Image.new('RGBA', (384, 384))
draw = ImageDraw.Draw(image)
for element in ET.parse(ROOT / 'assets/brand/icon.svg').getroot().iter('{http://www.w3.org/2000/svg}path'):
    tokens = re.findall(r'[A-Za-z]|[-+]?(?:\d*\.\d+|\d+)', element.attrib['d'])
    points, x, y, command, index = [], 0., 0., '', 0
    while index < len(tokens):
        if tokens[index].isalpha():
            command = tokens[index]
            index += 1
        if command in 'zZ':
            break
        relative = command.islower()
        if command in 'mMlL':
            a, b = float(tokens[index]), float(tokens[index + 1]); index += 2
            x, y = (x + a, y + b) if relative else (a, b)
            if command in 'mM': command = 'l' if relative else 'L'
        elif command in 'vV':
            a = float(tokens[index]); index += 1; y = y + a if relative else a
        elif command in 'hH':
            a = float(tokens[index]); index += 1; x = x + a if relative else a
        else:
            raise ValueError('Logo needs a full SVG renderer for path command ' + command)
        points.append((round(x * 3), round(y * 3)))
    draw.polygon(points, fill=element.attrib['fill'])
image.resize((48, 48), Image.Resampling.LANCZOS).save(DEST / 'mods.png')
image.resize((128, 128), Image.Resampling.LANCZOS).save(ROOT / 'assets/brand/icon.png')
PALETTE = {
    'base': '0.018 0.040 0.058 1',
    'library': '0.025 0.070 0.100 1',
    'details': '0.035 0.095 0.130 1',
    'rule': '0.150 0.330 0.420 1',
    'selected': '0.040 0.350 0.370 1',
}
for name, color in PALETTE.items():
    layer = -10 if name == 'base' else -8 if name in ('library', 'details') else -4
    (DEST / ('mt2fw_ui_' + name + '.mat')).write_text(f'''Material {{
    mode normal
    color {color}
    layer {layer}
    culling none
    zread false
    zwrite false
    glow false
}}\n''', encoding='utf-8')

def text(identifier, x, y, width, height, value, size=14, justify='topleft', color='0.89 0.95 0.98 1', bold=False):
    return f'''mmoTextPane
{{
    id "{identifier}";
    anchorType TopLeft;
    anchorPos {x} {y};
    dims {width} {height};
    text "{value}";
    size {size};
    textColor {color};
    bold {str(bold).lower()};
    justify {justify};
    editable false;
    clickable false;
    border false;
}}'''

def button(identifier, x, y, width, value, command, row=False):
    button = f'''mmoButtonPane
{{
    id "{identifier}";
    anchorType TopLeft;
    anchorPos {x} {y};
    dims {width} {38 if row else 34};
    text "";
    textSize 14;
    clickable true;
    border true;
    edges All;
    corners "TopLeft BottomRight";
    cornerSize 7;
    command "MessageTo Mode {command}";
}}'''
    return button + '\n' + text(identifier + '_label', x + (14 if row else 4), y + 8, width - (28 if row else 8), 24, value, size=16 if row else 14, justify='topleft' if row else 'topcenter')

def surface(identifier, x, y, width, height, color, material='White'):
    if material == 'White':
        material = 'mt2fw_ui_' + next(name for name, value in PALETTE.items() if value == color)
    return f'''mmoPane
{{
    id "{identifier}";
    anchorType TopLeft;
    anchorPos {x} {y};
    dims {width} {height};
    material "{material}";
    color {color};
    clickable false;
    border false;
}}'''

parts = [surface('brand_icon', 24, 18, 40, 40, '1 1 1 1', 'mt2fw_desktop_mods'),
         text('brand', 80, 14, 600, 34, 'MT2 Mod Framework', 24, bold=True),
         text('summary', 80, 48, 600, 24, 'Installed mods', color='0.63 0.79 0.86 1'),
         button('language', 742, 20, 114, 'Deutsch', 'MT2FWLanguage'),
         surface('header_rule', 24, 82, 832, 1, PALETTE['rule']),
         surface('library_background', 18, 100, 274, 368, PALETTE['library']),
         surface('details_background', 306, 100, 550, 318, PALETTE['details']),
         text('library_title', 30, 112, 242, 28, 'Installed mods', 16, bold=True)]
for slot in range(6):
    parts.append(button(f'mod{slot}', 30, 146 + slot * 46, 250, '', f'MT2FWSelect{slot}', row=True))
parts += [button('previous', 30, 426, 82, 'Previous', 'MT2FWPrevious'),
          text('page', 120, 435, 72, 24, '1 / 1', justify='topcenter'),
          button('next', 198, 426, 82, 'Next', 'MT2FWNext'),
          text('name', 324, 116, 510, 38, 'No mods installed', 26, bold=True),
          text('identity', 324, 158, 510, 24, '', 13, color='0.63 0.79 0.86 1'),
          text('state', 324, 190, 510, 26, '', 16, color='0.38 0.89 0.80 1'),
          text('description', 324, 228, 510, 60, 'Install a mod ZIP through Vortex or the MT2FW helper.', 16),
          text('dependencies', 324, 302, 510, 48, '', 14, color='0.63 0.79 0.86 1'),
          text('error', 324, 358, 510, 52, '', 14, color='1 0.79 0.42 1'),
          button('toggle', 306, 432, 146, 'Enable', 'MT2FWToggle'),
          button('reload', 464, 432, 180, 'Reload Lua', 'MT2FWReload'),
          button('settings', 656, 432, 200, 'Settings', 'MT2FWSettings'),
          surface('footer_rule', 24, 486, 832, 1, PALETTE['rule']),
          text('notice', 24, 506, 596, 42, 'Lua changes can be reloaded. Native plugins and content need a restart.', 13, color='0.63 0.79 0.86 1'),
          button('tools', 650, 504, 206, 'Developer tools', 'MT2FWTools')]
window = '''mmoWindow {
    id "mt2fw_mods";
    dims 880 560;
    anchorType Window;
    anchorPos 90 40;
    hasTitlebar true;
    title "Mods";
    material "mt2fw_ui_base";
    corners "BottomRight";
    cornerSize 12;
    hideOnEscape true;
    transitionType rez;
    rezSpeed normal;
    content {
''' + '\n'.join(textwrap.indent(part, '        ') for part in parts) + '\n    }\n}\n'
(DEST / 'mods.win').write_text(window, encoding='utf-8')
print('Updated native Mods window and 48x48 original framework logo.')
