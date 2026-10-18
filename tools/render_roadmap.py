"""Render the public roadmap and feature overview from docs/roadmap.json."""
import argparse
import html
import json
from pathlib import Path
import posixpath
import re
import sys
import textwrap

ROOT = Path(__file__).resolve().parents[1]
START, END = '<!-- roadmap:start -->', '<!-- roadmap:end -->'
COLORS = ('#64e4d5', '#ffca67', '#a8c1d5')
STATES = {'available': '✅ Available', 'partial': '◐ Partial',
          'experimental': '◇ Experimental', 'planned': '○ Planned'}


def text(x, y, value, size=22, fill='#b8cad8', weight=400):
    return (f'<text x="{x}" y="{y}" font-family="Arial" font-size="{size}" fill="{fill}" '
            f'font-weight="{weight}">{html.escape(value)}</text>')


def graphic(data, mobile=False):
    width, height = (600, 1030) if mobile else (1200, 475)
    title = f"{data['product']} roadmap"
    description = '. '.join(f"{stage['label']}: {stage['title']}. " + '; '.join(stage['items'])
                            for stage in data['stages'])
    parts = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {width} {height}" '
             'role="img" aria-labelledby="title description">',
             f'<title id="title">{html.escape(title)}</title>',
             f'<desc id="description">{html.escape(description)}</desc>',
             f'<path fill="#0b1723" d="M0 0h{width}v{height}H0z"/>',
             '<g font-family="Arial">',
             text(28, 46, data['graphic_title'], 30, '#f2f7fb', 700),
             text(28, 80, data['release_label'], 22)]
    for index, stage in enumerate(data['stages']):
        x, y, w, h = (76, 136 + index * 286, 492, 264) if mobile else (28 + index * 392, 150, 360, 276)
        color = COLORS[index]
        cx, cy = (32, y + 16) if mobile else (x + 16, 118)
        if index < 2:
            connection = f'v286' if mobile else 'h392'
            parts.append(f'<path d="M{cx} {cy}{connection}" stroke="{color}" '
                         f'stroke-width="2" stroke-dasharray="{0 if index == 0 else 6}" opacity=".5"/>')
        parts.append(f'<circle cx="{cx}" cy="{cy}" r="14" fill="#0b1723" stroke="{color}" stroke-width="3"/>')
        if index == 0:
            parts.append(f'<path d="m{cx - 6} {cy} 4 4 8-8" fill="none" stroke="{color}" stroke-width="3"/>')
        elif index == 1:
            parts.append(f'<circle cx="{cx}" cy="{cy}" r="5" fill="{color}"/>')
        parts.extend([text(x, y + 22, stage['label'], 23, color, 700),
                      text(x, y + 62, stage['title'], 27, '#f2f7fb', 700)])
        baseline = y + 104
        for item in stage['items']:
            lines = textwrap.wrap(item, width=36 if mobile else 25, break_long_words=False)
            parts.append(f'<circle cx="{x + 5}" cy="{baseline - 8}" r="3" fill="{color}"/>')
            for line in lines:
                parts.append(text(x + 20, baseline, line, 24 if mobile else 22))
                baseline += 30 if mobile else 29
            baseline += 10
        if baseline - 10 > y + h - 10:
            raise ValueError(f"Roadmap text exceeds the {stage['label']} panel")
    parts.extend([text(28, height - 20, 'Priorities can change. No release dates promised.', 19), '</g></svg>'])
    return '\n'.join(parts) + '\n'


def feature_table(data, from_docs=False):
    lines = [f"| Feature | {data['version_label']} | What is covered |", '| --- | --- | --- |']
    for row in data['features']:
        if row['status'] not in STATES:
            raise ValueError('Unknown feature status: ' + row['status'])
        if any('|' in row[key] or '\n' in row[key] for key in ('name', 'scope')):
            raise ValueError('Feature cells must be single lines without table delimiters')
        scope = row['scope']
        if from_docs:
            scope = re.sub(r'\]\((?!https?://|#)([^)]+)\)',
                           lambda match: '](' + (posixpath.relpath(match[1], 'docs')
                               if match[1].startswith('docs/') else
                               data['repository_url'] + '/blob/main/' + match[1]) + ')', scope)
        lines.append(f"| {row['name']} | {STATES[row['status']]} | {scope} |")
    return '\n'.join(lines)


def readme_block(data):
    assets = data['asset_directory']
    alt = 'Roadmap: ' + '. '.join(f"{stage['label']} - {stage['title']}" for stage in data['stages'])
    return '\n'.join([START, f"## {data['overview_heading']}", '', data['overview_intro'], '',
        feature_table(data), '', '## Roadmap', '',
        '<picture>', f'  <source media="(max-width: 600px)" srcset="{assets}/roadmap-mobile.svg">',
        f'  <img src="{assets}/roadmap.svg" alt="{html.escape(alt, quote=True)}" width="1200">',
        '</picture>', '', '[Read the roadmap in text](docs/ROADMAP.md). Priorities can change; no release dates are promised.', END])


def roadmap_text(data):
    lines = [f"# {data['product']} roadmap", '',
             f"> 🛟 **Need help or found a bug?** Get support at [support.doodesch.de/{data['support_slug']}](https://support.doodesch.de/{data['support_slug']}).", '',
             data['overview_intro'], '']
    for stage in data['stages']:
        lines.extend([f"## {stage['label']}: {stage['title']}", ''])
        lines.extend('- ' + item for item in stage['items'])
        lines.append('')
    lines.extend(['Priorities can change. No release dates are promised.', '', '## Feature coverage', '', feature_table(data, from_docs=True), ''])
    return '\n'.join(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true', help='Reject stale generated output without writing')
    args = parser.parse_args()
    data = json.loads((ROOT / 'docs/roadmap.json').read_text(encoding='utf-8'))
    if len(data['stages']) != 3:
        raise ValueError('The public roadmap needs available, next and later stages')
    readme = (ROOT / 'README.md').read_text(encoding='utf-8')
    if readme.count(START) != 1 or readme.count(END) != 1:
        raise ValueError('README must contain exactly one roadmap marker pair')
    start, end = readme.index(START), readme.index(END) + len(END)
    outputs = {'README.md': readme[:start] + readme_block(data) + readme[end:],
               'docs/ROADMAP.md': roadmap_text(data),
               data['asset_directory'] + '/roadmap.svg': graphic(data),
               data['asset_directory'] + '/roadmap-mobile.svg': graphic(data, mobile=True)}
    stale = []
    for name, expected in outputs.items():
        path = ROOT / name
        if path.exists() and path.read_text(encoding='utf-8') == expected:
            continue
        stale.append(name)
        if not args.check:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(expected, encoding='utf-8', newline='\n')
    if args.check and stale:
        raise ValueError('Stale roadmap output: ' + ', '.join(stale) + '; run python tools/render_roadmap.py')
    print('Roadmap: ' + ('current' if args.check else 'generated') + f" ({data['version_label']})")


if __name__ == '__main__':
    try:
        main()
    except (ValueError, KeyError, OSError) as error:
        print('error: ' + str(error), file=sys.stderr)
        sys.exit(1)
