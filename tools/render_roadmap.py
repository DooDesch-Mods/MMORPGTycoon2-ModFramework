"""Render the public roadmap and feature overview from docs/roadmap.json."""
import argparse
from collections import Counter
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
COVERAGE_COLORS = {'available': ('#64e4d5', '#15383d'), 'partial': ('#ffca67', '#3e321f'),
                   'experimental': ('#8cb9ff', '#1b304a'), 'planned': ('#a8c1d5', '#21303e')}


def text(x, y, value, size=22, fill='#b8cad8', weight=400):
    return (f'<text x="{x}" y="{y}" font-family="Arial" font-size="{size}" fill="{fill}" '
            f'font-weight="{weight}">{html.escape(value)}</text>')


def graphic(data, mobile=False):
    width, height = (600, 1030) if mobile else (1200, 475)
    title = f"{data['product']} roadmap"
    description = '. '.join(f"{stage['label']}: {stage['title']}. " + '; '.join(stage['items'])
                            for stage in data['stages'])
    parts = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}" '
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


def tiles(rows, x, y, width, height):
    """Split equal-weight feature areas without weighting broad systems as complete."""
    if len(rows) == 1:
        return [(rows[0], x, y, width, height)]
    count = len(rows) // 2
    ratio = count / len(rows)
    if width >= height:
        split = width * ratio
        return tiles(rows[:count], x, y, split, height) + tiles(rows[count:], x + split, y, width - split, height)
    split = height * ratio
    return tiles(rows[:count], x, y, width, split) + tiles(rows[count:], x, y + split, width, height - split)


def coverage_counts(data):
    if not data['features']:
        raise ValueError('Coverage requires a nonempty feature inventory')
    names = [row['name'] for row in data['features']]
    if len(set(names)) != len(names):
        raise ValueError('Duplicate feature areas would inflate coverage counts')
    counts = Counter(row['status'] for row in data['features'])
    if counts.keys() - STATES.keys():
        raise ValueError('Coverage contains an unknown feature status')
    return counts


def coverage_graphic(data, mobile=False):
    counts = coverage_counts(data)
    total = sum(counts.values())
    statuses = [status for status in STATES if counts[status]]
    width, height = (600, 1040 + 38 * (len(statuses) - 3)) if mobile else (1200, 560)
    description = '; '.join(f'{status}: {counts[status]} of {total}' for status in statuses)
    out = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}" '
           'role="img" aria-labelledby="title description">',
           f'<title id="title">{html.escape(data["coverage_title"])}</title>',
           f'<desc id="description">{html.escape(description)}. Each tile is one tracked feature area. '
           'Percentages describe the listed areas, not overall project completion or stability.</desc>',
           f'<path fill="#0b1723" d="M0 0h{width}v{height}H0z"/>',
           text(28, 44, data['coverage_title'], 30, '#f2f7fb', 700),
           text(28, 78, data['release_label'], 22)]
    for index, status in enumerate(statuses):
        color = COVERAGE_COLORS[status][0]
        percent = 100 * counts[status] / total
        label = status.capitalize()
        if mobile:
            out.append(text(28, 124 + index * 38, f'{percent:.1f}% {label} ({counts[status]}/{total})', 26, color, 700))
        else:
            x = 28 + index * (1144 / len(statuses))
            out.extend([text(x, 126, f'{percent:.1f}%', 37, color, 700),
                        text(x, 155, label, 23, color, 700),
                        text(x, 184, f'{counts[status]} of {total} tracked areas', 20)])
    gx, gy, gw, gh = (28, 240 + 38 * (len(statuses) - 3), 544, 680) if mobile else (28, 210, 1144, 264)
    offset = 0
    for status in statuses:
        rows = [row for row in data['features'] if row['status'] == status]
        size = (gh if mobile else gw) * len(rows) / total
        area = (gx, gy + offset, gw, size) if mobile else (gx + offset, gy, size, gh)
        color, fill = COVERAGE_COLORS[status]
        for row, x, y, w, h in tiles(rows, *area):
            x, y, w, h = x + 3, y + 3, w - 6, h - 6
            out.append(f'<rect x="{x:.2f}" y="{y:.2f}" width="{w:.2f}" height="{h:.2f}" fill="{fill}" stroke="{color}" stroke-opacity=".65"/>')
            out.append(f'<g><title>{html.escape(row["name"] + ": " + status)}</title>')
            size_text = 25 if mobile else 20
            label = row.get('short_name', row['name'])
            lines = textwrap.wrap(label, width=max(8, int((w - 24) / (size_text * .56))), break_long_words=False)
            if len(lines) * (size_text + 5) > h - 20:
                raise ValueError('Coverage tile label does not fit: ' + label)
            baseline = y + (h - len(lines) * (size_text + 5)) / 2 + size_text
            for line in lines:
                out.append(text(round(x + 12, 2), round(baseline, 2), line, size_text, '#f2f7fb', 600))
                baseline += size_text + 5
            out.append('</g>')
        offset += size
    out.extend([text(28, height - 44, 'Each tile is one tracked feature area.', 20),
                text(28, height - 16, 'See feature details for scope and limits.', 20), '</svg>'])
    return '\n'.join(out) + '\n'


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
    raw = data['repository_url'].replace('https://github.com/', 'https://raw.githubusercontent.com/') + '/main/'
    alt = 'Roadmap: ' + '. '.join(f"{stage['label']} - {stage['title']}" for stage in data['stages'])
    counts = coverage_counts(data)
    total = sum(counts.values())
    summary = ', '.join(f'{counts[status]} {status}' for status in STATES if counts[status])
    return '\n'.join([START, f"## {data['overview_heading']}", '', data['overview_intro'], '',
        '<picture>', f'  <source media="(max-width: 600px)" srcset="{raw}{assets}/coverage-mobile.svg">',
        f'  <img src="{assets}/coverage.svg" alt="Feature coverage: {summary}, across {total} tracked areas" width="1200">',
        '</picture>', '',
        'Percentages count the listed areas equally. They describe feature coverage, not overall project completion or stability.', '',
        '<details>', '<summary>Feature details - what is included</summary>', '',
        feature_table(data), '', '</details>', '', '## Roadmap', '',
        '<picture>', f'  <source media="(max-width: 600px)" srcset="{raw}{assets}/roadmap-mobile.svg">',
        f'  <img src="{assets}/roadmap.svg" alt="{html.escape(alt, quote=True)}" width="1200">',
        '</picture>', '', '[Read the roadmap in text](docs/ROADMAP.md).', END])


def roadmap_text(data):
    lines = [f"# {data['product']} roadmap", '',
             f"> 🛟 **Need help or found a bug?** Get support at [support.doodesch.de/{data['support_slug']}](https://support.doodesch.de/{data['support_slug']}).", '',
             data['overview_intro'], '']
    for stage in data['stages']:
        lines.extend([f"## {stage['label']}: {stage['title']}", ''])
        lines.extend('- ' + item for item in stage['items'])
        lines.append('')
    lines.extend(['Priorities can change. No release dates are promised.', '', '## Feature coverage', '', feature_table(data, from_docs=True), ''])
    counts = coverage_counts(data)
    total = sum(counts.values())
    lines.extend(['## How coverage is counted', '',
                  'Each listed feature area counts once. The categories have equal weight. Partial and experimental areas do not count as fully available.', ''])
    lines.extend(f'- {status.capitalize()}: {count}/{total} ({100 * count / total:.1f}%)'
                 for status, count in counts.items())
    lines.extend(['', 'This measures the listed feature areas, not overall project completion or stability. '
                  'The denominator changes when the inventory changes.', ''])
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
               data['asset_directory'] + '/roadmap-mobile.svg': graphic(data, mobile=True),
               data['asset_directory'] + '/coverage.svg': coverage_graphic(data),
               data['asset_directory'] + '/coverage-mobile.svg': coverage_graphic(data, mobile=True)}
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
