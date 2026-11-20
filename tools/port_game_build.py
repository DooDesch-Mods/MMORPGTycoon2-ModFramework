"""Port the build-specific literals of the MT2 adapter from one MT2.exe build to another.

Usage:
    python tools/port_game_build.py OLD_EXE NEW_EXE [--apply] [--report PATH] [--repo DIR] [--accept FILE] [--audit]

Maintainer tool, needs `pip install pefile capstone`. Both executables stay local; nothing from them is
written except addresses, lengths, guard bytes and SHA256 digests that the sources already carry.

What it does:
- Maps functions of the old build to the new one through `.pdata` and linker order. Template instances
  with the same bytes are told apart by their direct call targets. Twins that call nothing can still
  map to the wrong instance; check that the old and new function read the same mapped globals.
- Compares mapped functions instruction by instruction, with RIP-relative operands and branch targets
  masked. A range whose instructions all match is "clean": its RVA and length move, its digest and
  guard bytes are read again from the new image.
- Maps data addresses through pairs of RIP-relative references in matching instructions, IAT slots
  through the import table.
- Rewrites hex addresses, `{rva,size,"sha"}` pins and their call forms, byte guards, the JSON evidence
  files and the exe digest. Anything it cannot prove keeps its old value and goes into the report; old
  digests then fail at runtime and keep the feature disabled.

- With --accept, functions a reviewer listed in `tools/ports/` move whole: their pins get the new start,
  size and digest although their code changed.
- --audit checks the function map instead of rewriting: every function start the adapter and tests name,
  and every accept entry, must map to a new function that reads the mapped .data/.bss globals. Run it on
  the unported sources, before --apply.

Without --apply it only writes the report.
"""
import argparse, bisect, difflib, hashlib, json, pathlib, re, sys
from collections import Counter, defaultdict

import pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_64

ROOT = pathlib.Path(__file__).resolve().parents[1]
CODE_SUFFIXES = ('.h', '.inc', '.cpp', '.hpp')
SMALL = 0x30000  # Below this, a hex literal counts as an address only in a pin or guard slot or at a function start.
MD = Cs(CS_ARCH_X86, CS_MODE_64)
RIP = re.compile(r'\[rip ([+-]) (0x[0-9a-f]+|\d+)\]')
LINE_ARG = re.compile(r'r[89]d, 0x[0-9a-f]+')
SOURCE_PATH = re.compile(rb'[\w./-]+\.(?:cpp|h|hpp|inl)$')
BRANCH = {'call', 'jmp', 'jrcxz', 'jecxz', 'loop', 'loope', 'loopne'} | {'j' + c for c in (
    'a', 'ae', 'b', 'be', 'e', 'ne', 'g', 'ge', 'l', 'le', 'o', 'no', 's', 'ns', 'p', 'np')}


class Image:
    def __init__(self, path):
        data = pathlib.Path(path).read_bytes()
        self.sha = hashlib.sha256(data).hexdigest()
        pe = pefile.PE(data=data, fast_load=True)
        pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY[d] for d in (
            'IMAGE_DIRECTORY_ENTRY_EXCEPTION', 'IMAGE_DIRECTORY_ENTRY_IMPORT')])
        self.img = pe.get_memory_mapped_image()
        self.funcs = sorted({(e.struct.BeginAddress, e.struct.EndAddress) for e in pe.DIRECTORY_ENTRY_EXCEPTION})
        self.begins = [f[0] for f in self.funcs]
        self.begin_set = set(self.begins)
        self.secs = [(s.Name.rstrip(b'\0').decode(errors='replace'), s.VirtualAddress,
                      max(s.Misc_VirtualSize, s.SizeOfRawData)) for s in pe.sections]
        self.iat = {}  # slot rva -> (dll, symbol)
        for entry in getattr(pe, 'DIRECTORY_ENTRY_IMPORT', []):
            for imp in entry.imports:
                name = imp.name.decode() if imp.name else f'#{imp.ordinal}'
                self.iat[imp.address - pe.OPTIONAL_HEADER.ImageBase] = (entry.dll.decode().lower(), name)
        self._insns = {}

    def section(self, v):
        for name, va, size in self.secs:
            if va <= v < va + size:
                return name
        return None

    def owner(self, v):
        """Index of the function whose range or trailing padding holds v."""
        k = bisect.bisect_right(self.begins, v) - 1
        return k if k >= 0 else None

    def insns(self, k):
        if k not in self._insns:
            b, e = self.funcs[k]
            self._insns[k] = [(a, s, m, o) for a, s, m, o in MD.disasm_lite(bytes(self.img[b:e]), b)]
        return self._insns[k]


def rip_target(insn):
    """Absolute target of the instruction's RIP-relative operand, or None."""
    a, s, m, o = insn
    r = RIP.search(o)
    return a + s + int(r.group(2), 0) * (1 if r.group(1) == '+' else -1) if r else None


def calls(img, k):
    """Direct call targets of function k, in order."""
    return [int(op, 16) for _, _, m, op in img.insns(k) if m == 'call' and op.startswith('0x')]


def data_reads(img, k):
    """Globals in .data/.bss that function k addresses RIP-relative."""
    return {t for t in map(rip_target, img.insns(k)) if t is not None and img.section(t) in ('.data', '.bss')}


def norm(insn):
    a, s, m, o = insn
    if m in BRANCH and o.startswith('0x'):
        return m + ' T'
    return m + ' ' + RIP.sub('[rip+X]', o)


class Port:
    def __init__(self, old, new, accept=None):
        self.o, self.n = Image(old), Image(new)
        self.fmap = self._map_functions()
        self._pairs = {}
        self.data_pairs = None
        self.accepted = set()
        self.data_accept = {}  # old .data/.bss address -> reviewed new address
        if accept:
            self._load_accept(accept)

    def _load_accept(self, path):
        """Reviewed functions: `OLD [=NEW] reason`. Whole-function pins of these move and are hashed again.

        A reviewer lists a function here after reading its instruction diff and finding that the change does
        not touch what the adapter relies on. `=NEW` pairs a function the linker-order map could not match.
        """
        for line in pathlib.Path(path).read_text(encoding='utf-8').splitlines():
            line = line.split('#', 1)[0].strip()
            if not line:
                continue
            spec = line.split()[0]
            old, _, new = spec.partition('=')
            v = int(old, 16)
            if self.o.section(v) not in ('.text', None):
                # A data pair overrides the neighbour-offset guess, e.g. when two globals swapped places.
                if not new or not self.n.section(int(new, 16)):
                    raise SystemExit(f'accept: data entry {spec} needs OLD=NEW inside the new image')
                self.data_accept[v] = int(new, 16)
                continue
            if v not in self.o.begin_set:
                raise SystemExit(f'accept: {old} is not a function start in the old build')
            k = self.o.begins.index(v)
            if new:
                w = int(new, 16)
                if w not in self.n.begin_set:
                    raise SystemExit(f'accept: {new} is not a function start in the new build')
                self.fmap[k] = self.n.begins.index(w)
                self._pairs.pop(k, None)
            elif k not in self.fmap:
                raise SystemExit(f'accept: {old} has no mapped function; give it as OLD=NEW')
            self.accepted.add(k)

    # Functions ---------------------------------------------------------------------------------
    def _map_functions(self):
        o, n = self.o, self.n

        def sim(i, j):
            (ob, oe), (nb, ne) = o.funcs[i], n.funcs[j]
            if oe - ob != ne - nb:
                return 0.0
            a, b = o.img[ob:oe], n.img[nb:ne]
            return 1.0 if a == b else sum(x == y for x, y in zip(a, b)) / len(a)

        fmap, drift, unmatched, twins = {}, 0, [], {}
        for i in range(len(o.funcs)):
            for d in sorted(range(-40, 60), key=lambda d: abs(d - drift)):
                j = i + d
                if 0 <= j < len(n.funcs) and sim(i, j) >= 0.80:
                    fmap[i] = j
                    drift = d
                    others = [i + e for e in range(-40, 60)
                              if i + e != j and 0 <= i + e < len(n.funcs) and sim(i, i + e) >= 0.80]
                    if others:
                        twins[i] = [j] + others
                    break
            else:
                unmatched.append(i)
        # Template instances share a body and differ only in a callee, so bytes alone can pick the wrong twin.
        # Among the candidates, keep the one whose direct call targets agree with the mapped callees.
        for _ in range(2):
            for i, cands in twins.items():
                score = {j: self._call_agreement(i, j, fmap) for j in cands}
                best = max(cands, key=lambda j: score[j])
                if score[best] > score[fmap[i]]:
                    fmap[i] = best
        # An unmatched function between two matched neighbours with a gap of equal size is a changed function.
        for i in unmatched:
            p = next((k for k in range(i - 1, -1, -1) if k in fmap), None)
            q = next((k for k in range(i + 1, len(o.funcs)) if k in fmap), None)
            if p is not None and q is not None and q - p == fmap[q] - fmap[p]:
                fmap[i] = fmap[p] + (i - p)
        return fmap

    def _call_agreement(self, i, j, fmap):
        """Count direct calls of old function i whose mapped target is the call target in new function j.

        RIP-relative function pointers are not counted: they often point at other twins and reinforce a wrong pick.
        """
        hits = 0
        for t, u in zip(calls(self.o, i), calls(self.n, j)):
            k = self.o.owner(t)
            if k is not None and t == self.o.funcs[k][0] and k in fmap and self.n.funcs[fmap[k]][0] == u:
                hits += 1
        return hits

    def pair(self, k):
        """Instruction alignment of old function k: (new index, old->new address map of matching instructions, relocation-only)."""
        if k in self._pairs:
            return self._pairs[k]
        if k not in self.fmap:
            self._pairs[k] = None
            return None
        j = self.fmap[k]
        a, b = self.o.insns(k), self.n.insns(j)
        na, nb = [norm(x) for x in a], [norm(x) for x in b]
        amap, same = {}, True
        for tag, i1, i2, j1, j2 in difflib.SequenceMatcher(None, na, nb, autojunk=False).get_opcodes():
            if tag == 'equal' or (tag == 'replace' and i2 - i1 == j2 - j1 and all(
                    self.source_line_arg(a, i1 + t, b[j1 + t]) for t in range(i2 - i1))):
                for t in range(i2 - i1):
                    amap[a[i1 + t][0]] = b[j1 + t][0]
            else:
                same = False
        # The function end maps to the function end when the tail matches.
        if a and b and na[-1] == nb[-1]:
            amap[self.o.funcs[k][1]] = self.n.funcs[j][1]
        complete = bool(a) and a[-1][0] + a[-1][1] == self.o.funcs[k][1]
        result = (j, amap, same and complete)
        self._pairs[k] = result
        return result

    def source_line_arg(self, ins, i, other):
        """True if ins[i] -> other only changes the source line number of an assert or log call.

        The game passes `__LINE__` as an immediate in r8d/r9d next to a `lea` of its source path
        (`../code/...cpp`). Edits elsewhere in a source file shift these numbers without changing the code.
        """
        x = ins[i]
        if not (x[2] == other[2] == 'mov' and LINE_ARG.fullmatch(x[3]) and LINE_ARG.fullmatch(other[3])
                and x[3][:3] == other[3][:3]):
            return False
        for z in ins[max(0, i - 3):i + 4]:
            t = rip_target(z) if z[2] == 'lea' else None
            if t is not None and self.o.section(t) == '.rdata' and SOURCE_PATH.match(bytes(self.o.img[t:t + 160]).split(b'\0')[0]):
                return True
        return False

    # Code addresses ----------------------------------------------------------------------------
    def map_code(self, v):
        """Return (new, status). status: exact, aligned (function changed elsewhere), begin-changed, unmapped, not-boundary."""
        k = self.o.owner(v)
        if k is None:
            return None, 'unmapped'
        b, e = self.o.funcs[k]
        if v >= e:
            return self.map_thunk(v)
        p = self.pair(k)
        if p is None:
            return None, 'unmapped'
        j, amap, clean = p
        if v == b and k in self.accepted:
            return self.n.funcs[j][0], 'exact'
        # A function entry maps only to the new entry: callers and hooks need the prologue.
        if v in amap and (v != b or amap[v] == self.n.funcs[j][0]):
            return amap[v], 'exact' if clean else 'aligned'
        if v == b:
            return self.n.funcs[j][0], 'begin-changed'
        if any(a == v for a, *_ in self.o.insns(k)):
            return None, 'changed'
        return None, 'not-boundary'

    def map_thunk(self, v):
        """Import thunk `jmp [rip+slot]` outside .pdata: find the thunk of the same import in the new image."""
        code = self.o.img[v:v + 6]
        if code[:2] != b'\xff\x25':
            return None, 'not-boundary'
        slot = v + 6 + int.from_bytes(code[2:6], 'little', signed=True)
        name = self.o.iat.get(slot)
        nslot = next((s for s, nm in self.n.iat.items() if nm == name), None) if name else None
        if nslot is None:
            return None, 'unmapped'
        if not hasattr(self, '_thunks'):
            self._thunks = {}
            img = self.n.img
            i = img.find(b'\xff\x25')
            while i >= 0:
                self._thunks.setdefault(i + 6 + int.from_bytes(img[i + 2:i + 6], 'little', signed=True), i)
                i = img.find(b'\xff\x25', i + 1)
        t = self._thunks.get(nslot)
        return (t, 'exact') if t is not None else (None, 'unmapped')

    def map_range(self, a, size):
        """Map [a, a+size). Returns (new_a, new_size, status); status is clean only if every instruction matches."""
        na, st = self.map_code(a)
        if na is None:
            return None, None, st
        end = a + size
        k = self.o.owner(a)
        if k in self.accepted and a == self.o.funcs[k][0]:
            # A reviewed function moves whole: to its end, or to the next function start when the pin covers padding.
            j = self.fmap[k]
            nxt = self.o.begins[k + 1] if k + 1 < len(self.o.begins) else None
            if end == self.o.funcs[k][1]:
                return na, self.n.funcs[j][1] - na, 'clean'
            if end == nxt and j + 1 < len(self.n.begins):
                return na, self.n.begins[j + 1] - na, 'clean'
        worst = 'clean'
        pos, prev_j = a, None
        while pos < end:
            k = self.o.owner(pos)
            b, e = self.o.funcs[k]
            p = self.pair(k)
            if p is None:
                return na, None, 'unmapped'
            j, amap, _ = p
            if prev_j is not None and j != prev_j + 1:
                return na, None, 'split'
            if any(pos <= x < end and x not in amap for x, *_ in self.o.insns(k)) or \
                    (min(end, e) > pos and self.o.insns(k) and self.o.insns(k)[-1][0] + self.o.insns(k)[-1][1] < min(end, e)):
                worst = 'code-diff'
            prev_j = j
            pos = self.o.begins[k + 1] if k + 1 < len(self.o.begins) else end
        k = self.o.owner(end - 1)
        b, e = self.o.funcs[k]
        j, amap, _ = self.pair(k)
        if end in amap:
            nend = amap[end]
        elif end >= e:
            nend = self.n.funcs[j][1] + (end - e)
        else:
            # A guard may stop inside an instruction; keep the same cut if that instruction kept its length.
            ins = next((x for x in self.o.insns(k) if x[0] < end < x[0] + x[1]), None)
            nins = next((x for x in self.n.insns(j) if ins and x[0] == amap.get(ins[0])), None)
            if not ins or not nins or nins[1] != ins[1]:
                return na, None, 'end-not-boundary'
            nend = nins[0] + (end - ins[0])
        return na, nend - na, worst

    # Data addresses ----------------------------------------------------------------------------
    def build_data_pairs(self):
        pairs = defaultdict(Counter)
        for k in range(len(self.o.funcs)):
            p = self.pair(k)
            # Only relocation-only functions: alignment inside changed functions can pair unrelated operands.
            if p is None or not p[2]:
                continue
            j, amap, _ = p
            new_by_addr = {x[0]: x for x in self.n.insns(j)}
            for ins in self.o.insns(k):
                t1 = rip_target(ins) if ins[0] in amap else None
                other = new_by_addr.get(amap[ins[0]]) if t1 is not None else None
                t2 = rip_target(other) if other else None
                if t2 is None:
                    continue
                if self.o.section(t1) != '.text':
                    pairs[t1][t2] += 1
            # Release disassembly of functions that are done; keeps memory flat.
            self.o._insns.pop(k, None)
            self.n._insns.pop(j, None)
            self._pairs.pop(k, None)
        self.data_pairs = {k: v.most_common(1)[0][0] for k, v in pairs.items()}
        self.data_conflicts = {k for k, v in pairs.items() if v.most_common(1)[0][1] < 0.9 * sum(v.values())}
        self.data_keys = sorted(self.data_pairs)

    def map_data(self, v):
        if v in self.o.iat:
            name = self.o.iat[v]
            ns = next((s for s, nm in self.n.iat.items() if nm == name), None)
            return (ns, 'exact') if ns is not None else (None, 'unmapped')
        if v in self.data_accept:
            return self.data_accept[v], 'exact'
        if self.data_pairs is None:
            self.build_data_pairs()
        if v in self.data_pairs:
            return self.data_pairs[v], 'conflict' if v in self.data_conflicts else 'exact'
        sec = self.o.section(v)
        i = bisect.bisect_right(self.data_keys, v)
        lo = self.data_keys[i - 1] if i > 0 else None
        hi = self.data_keys[i] if i < len(self.data_keys) else None
        deltas = [self.data_pairs[x] - x for x in (lo, hi) if x is not None and self.o.section(x) == sec]
        if len(deltas) == 2 and deltas[0] == deltas[1]:
            return v + deltas[0], 'inferred'
        if sec in ('.bss', None):
            return None, 'ambiguous'
        for d in deltas:
            if self.o.img[v:v + 16] == self.n.img[v + d:v + d + 16]:
                return v + d, 'inferred'
        return None, 'ambiguous'

    def map_any(self, v):
        sec = self.o.section(v)
        if sec == '.text':
            return self.map_code(v)
        if sec:
            return self.map_data(v)
        return None, 'outside-image'


# Source rewriting -------------------------------------------------------------------------------
HEX = re.compile(r'(?<![0-9A-Za-z_\\])0x([0-9a-fA-F]{4,8})\b')
SHA = re.compile(r'(?<![0-9a-fA-F])[0-9a-f]{64}(?![0-9a-fA-F])')
NUM = re.compile(r'(?<![0-9A-Za-z_\\.])(0x[0-9a-fA-F]+|\d+)\b')
XSTR = re.compile(r'"((?:\\x[0-9a-fA-F]{2})+)"')
BRACE = re.compile(r'\{\s*((?:0x[0-9a-fA-F]{1,2}|0)(?:\s*,\s*(?:0x[0-9a-fA-F]{1,2}|0)){5,})\s*,?\s*\}')


def fmt_hex(old_text, value):
    digits = f'{value:x}'
    return '0x' + (digits.upper() if old_text[2:].isupper() and any(c.isalpha() for c in old_text[2:]) else digits)


class Rewriter:
    def __init__(self, port, repo):
        self.p, self.repo = port, repo
        self.report = []  # (file, line, kind, old, status, detail)
        self.learned_num, self.learned_sha, self.learned_bytes = {}, {}, {}
        self.counts = Counter()

    def flag(self, rel, text, pos, kind, old, status, detail=''):
        self.report.append((rel, text.count('\n', 0, pos) + 1, kind, old, status, detail))

    def code_file(self, rel, text):
        p, o, n = self.p, self.p.o, self.p.n
        edits = {}

        def put(span, value):
            if span in edits and edits[span] != value:
                self.flag(rel, text, span[0], 'conflict', text[span[0]:span[1]], 'conflict', f'{edits[span]} vs {value}')
                return
            edits[span] = value

        # int(x, 0) rejects decimal digits with a leading zero ("05" in a time format string).
        nums = [(m.span(), int(m.group(1), 16 if m.group(1)[:2] == '0x' else 10)) for m in NUM.finditer(text)]
        num_at = {s: v for s, v in nums}
        starts = [s[0] for s, _ in nums]

        def window(pos, back, ahead):
            i, k = bisect.bisect_left(starts, pos - back), bisect.bisect_right(starts, pos + ahead)
            return nums[i:k]

        claimed = set()  # spans already handled as pin or guard slots

        def array_bytes(m):
            if m.re is XSTR:
                return bytes(int(x, 16) for x in re.findall(r'\\x([0-9a-fA-F]{2})', m.group(1)))
            return bytes(int(x, 0) for x in re.split(r'\s*,\s*', m.group(1).strip()))

        arrays = list(XSTR.finditer(text)) + list(BRACE.finditer(text))
        # A digest may cover sizeof(byte array) instead of a size literal; those sizes have no span to rewrite.
        array_sizes = [(None, k) for k in sorted({len(array_bytes(a)) for a in arrays})]
        # Digests: find the (rva, size) pair in the preceding text whose old bytes hash to the digest.
        for m in SHA.finditer(text):
            sha = m.group(0)
            if sha == o.sha:
                put(m.span(), n.sha)
                self.counts['exe digest'] += 1
                continue
            cands = window(m.start(), 700, 0)
            addrs = [(s, v) for s, v in cands if v >= 0x1000 and o.section(v)]
            sizes = array_sizes + [(s, v) for s, v in cands if 0 < v <= 0x40000]
            hit = None
            for sa, va in reversed(addrs):
                for ss, vs in reversed(sizes):
                    if ss != sa and hashlib.sha256(o.img[va:va + vs]).digest().hex() == sha:
                        hit = (sa, va, ss, vs)
                        break
                if hit:
                    break
            if not hit:
                self.flag(rel, text, m.start(), 'sha', sha[:16], 'unresolved', 'no (rva,size) nearby hashes to it')
                self.counts['sha unresolved'] += 1
                continue
            sa, va, ss, vs = hit
            na, nsize, st = p.map_range(va, vs)
            claimed.add(sa)
            if na is not None:
                put(sa, fmt_hex(text[sa[0]:sa[1]], na) if text[sa[0]:sa[1]].startswith('0x') else str(na))
                self.learned_num[va] = na
            if st == 'clean':
                if nsize != vs:
                    if ss is not None:
                        put(ss, str(nsize) if not text[ss[0]:ss[1]].startswith('0x') else hex(nsize))
                    self.flag(rel, text, m.start(), 'pin', hex(va), 'size-changed', f'{vs} -> {nsize}')
                new_sha = hashlib.sha256(n.img[na:na + nsize]).hexdigest()
                put(m.span(), new_sha)
                self.learned_sha[sha] = new_sha
                self.counts['pin clean'] += 1
            else:
                self.flag(rel, text, m.start(), 'pin', hex(va), st, f'size {vs}, new rva {hex(na) if na else None}; digest kept')
                self.counts['pin ' + st] += 1

        # Byte guards: the literal bytes must sit at a nearby address in the old image.
        for m in arrays:
            old = array_bytes(m)
            if len(old) < 4:
                continue
            hit = None
            # Nearby first; then the whole file, for guard arrays declared apart from their RVA table.
            for cands in (window(m.start(), 400, 400), nums):
                for s, v in sorted(cands, key=lambda x: abs(x[0][0] - m.start())):
                    if v >= 0x1000 and o.img[v:v + len(old)] == old:
                        hit = (s, v)
                        break
                if hit:
                    break
            if not hit:
                self.flag(rel, text, m.start(), 'bytes', old[:8].hex(), 'unresolved', f'{len(old)} bytes not found at a nearby address')
                self.counts['bytes unresolved'] += 1
                continue
            s, v = hit
            na, nsize, st = p.map_range(v, len(old))
            claimed.add(s)
            if na is not None:
                put(s, fmt_hex(text[s[0]:s[1]], na) if text[s[0]:s[1]].startswith('0x') else str(na))
                self.learned_num[v] = na
            if st == 'clean' and nsize == len(old):
                new = bytes(n.img[na:na + nsize])
                if m.re is XSTR:
                    put(m.span(), '"' + ''.join(f'\\x{b:02x}' for b in new) + '"')
                else:
                    body = m.group(0)
                    items = iter(new)
                    put(m.span(), re.sub(r'0x[0-9a-fA-F]{1,2}|\b0\b', lambda t: f'0x{next(items):02x}', body))
                self.learned_bytes[old.hex()] = new.hex()
                self.counts['bytes clean'] += 1
            else:
                self.flag(rel, text, m.start(), 'bytes', hex(v), st if st != 'clean' else 'size-changed', f'{len(old)} -> {nsize}; bytes kept')
                self.counts['bytes ' + st] += 1

        # Plain addresses.
        for m in HEX.finditer(text):
            span, v = m.span(), int(m.group(1), 16)
            if span in claimed or span in edits:
                continue
            sec = o.section(v)
            if not sec or v < 0x1000:
                continue
            if sec == '.text':
                k = o.owner(v)
                begin = v in o.begin_set
                if v < SMALL and (not begin or v & 0xff == 0):
                    continue
                if not begin and not any(a == v for a, *_ in o.insns(k)):
                    continue
            nv, st = p.map_any(v)
            if nv is None:
                self.flag(rel, text, m.start(), 'address', m.group(0), st)
                self.counts['address ' + st] += 1
                continue
            put(span, fmt_hex(m.group(0), nv))
            self.learned_num[v] = nv
            self.counts['address ' + st] += 1
            if st not in ('exact', 'inferred'):
                self.flag(rel, text, m.start(), 'address', m.group(0), st, f'-> {hex(nv)}')
        return edits

    def learned_file(self, rel, text):
        """Tests, loader and tools: replace only values the adapter pass learned, so synthetic test values stay."""
        edits = {}
        for m in SHA.finditer(text):
            if m.group(0) == self.p.o.sha:
                edits[m.span()] = self.p.n.sha
            elif m.group(0) in self.learned_sha:
                edits[m.span()] = self.learned_sha[m.group(0)]
        for m in HEX.finditer(text):
            v = int(m.group(1), 16)
            if v in self.learned_num and v >= SMALL:
                edits[m.span()] = fmt_hex(m.group(0), self.learned_num[v])
        for m in re.finditer(r"""(['"](?:\w*_)?rva['"]\s*:\s*)(\d+)""", text):
            v = int(m.group(2))
            if v in self.learned_num:
                edits[m.span(2)] = str(self.learned_num[v])
        for m in re.finditer(r"""['"]([0-9a-f]{8,})['"]""", text):
            if m.group(1) in self.learned_bytes:
                edits[m.span(1)] = self.learned_bytes[m.group(1)]
        return edits

    def json_file(self, rel, text):
        data = json.loads(text)
        p, o, n = self.p, self.p.o, self.p.n

        def num(x):
            return int(x, 16) if isinstance(x, str) else x

        def put_num(d, key, value):
            d[key] = hex(value) if isinstance(d[key], str) else value

        def walk(node, base, path):
            if isinstance(node, list):
                for i, x in enumerate(node):
                    walk(x, base, f'{path}[{i}]')
                return
            if not isinstance(node, dict):
                return
            for key in list(node):
                if isinstance(node[key], str) and node[key] == o.sha:
                    node[key] = n.sha
            own = num(node['rva']) if 'rva' in node else (base + node['offset'] if base is not None and 'offset' in node else None)
            # Mapped base for children, taken before this node's own values change.
            child_base = own
            size = node.get('length', node.get('size', node.get('bytes') if isinstance(node.get('bytes'), int) else None))
            hexbytes = node.get('bytes') if isinstance(node.get('bytes'), str) else None
            if size is None and hexbytes:
                size = len(hexbytes) // 2
            if own is not None and ('sha256' in node or hexbytes) and size:
                ok_sha = 'sha256' not in node or hashlib.sha256(o.img[own:own + size]).hexdigest() == node['sha256']
                ok_bytes = not hexbytes or o.img[own:own + size].hex() == hexbytes
                if not (ok_sha and ok_bytes):
                    self.flag(rel, '', 0, 'json', f'{path} {hex(own)}', 'old-mismatch', 'evidence does not match the old image')
                else:
                    na, nsize, st = p.map_range(own, size)
                    if st == 'clean':
                        if 'sha256' in node:
                            new = hashlib.sha256(n.img[na:na + nsize]).hexdigest()
                            self.learned_sha[node['sha256']] = new
                            node['sha256'] = new
                        if hexbytes:
                            self.learned_bytes[hexbytes] = n.img[na:na + nsize].hex()
                            node['bytes'] = n.img[na:na + nsize].hex()
                        for k in ('length', 'size'):
                            if k in node:
                                node[k] = nsize
                        if isinstance(node.get('bytes'), int):
                            node['bytes'] = nsize
                        if nsize != size:
                            self.flag(rel, '', 0, 'json', f'{path} {hex(own)}', 'size-changed', f'{size} -> {nsize}')
                        self.counts['json clean'] += 1
                    else:
                        self.flag(rel, '', 0, 'json', f'{path} {hex(own)}', st, 'digest and bytes kept')
                        self.counts['json ' + st] += 1
            for key in list(node):
                if key == 'rva' or key.endswith('_rva'):
                    v = num(node[key])
                    nv, st = p.map_any(v)
                    if nv is None:
                        self.flag(rel, '', 0, 'json', f'{path}.{key} {hex(v)}', st)
                        self.counts['json address ' + st] += 1
                    else:
                        put_num(node, key, nv)
                        self.learned_num[v] = nv
                        if st not in ('exact', 'inferred'):
                            self.flag(rel, '', 0, 'json', f'{path}.{key} {hex(v)}', st, f'-> {hex(nv)}')
            for key, value in node.items():
                if isinstance(value, (dict, list)):
                    walk(value, child_base, f'{path}.{key}')

        walk(data, None, '$')
        return json.dumps(data, indent=2, ensure_ascii=False) + '\n'


def apply(text, edits):
    out, last = [], 0
    for (a, b), value in sorted(edits.items()):
        out.append(text[last:a])
        out.append(value)
        last = b
    out.append(text[last:])
    return ''.join(out)


def audit(port, repo, code_dirs):
    """Report function starts whose new partner does not read the mapped globals, with nearby candidates.

    Byte similarity can pick the wrong template twin: instances share a body and differ only in the global
    or callee they use. A candidate has the old size, lies within 0x40000 of the mapped start, reads every
    mapped global and makes the same direct calls in the same order. Returns the number of mismatches.
    """
    o, n = port.o, port.n
    starts = defaultdict(set)  # old function start -> source lines or the accept list
    for path in sorted(x for d in {*code_dirs, 'tests'} for x in (repo / d).rglob('*') if x.suffix in CODE_SUFFIXES):
        rel = path.relative_to(repo).as_posix()
        for i, line in enumerate(path.read_text(encoding='utf-8', errors='replace').splitlines(), 1):
            for m in HEX.finditer(line):
                v = int(m.group(1), 16)
                if v in o.begin_set and o.section(v) == '.text':
                    starts[v].add(f'{rel}:{i}')
    for k in port.accepted:
        starts[o.begins[k]].add('accept list')
    if port.data_pairs is None:
        port.build_data_pairs()

    def fmt(xs):
        return ' '.join(hex(x) if x is not None else 'unmapped' for x in sorted(xs, key=lambda x: x or 0))

    checked = bad = 0
    for v in sorted(starts):
        k = o.owner(v)
        nv, st = port.map_code(v)
        old = data_reads(o, k)
        if nv is None or not old:
            continue
        checked += 1
        want = {port.map_data(g)[0] for g in old}
        got = data_reads(n, n.owner(nv))
        if want <= got:
            continue
        bad += 1
        size = o.funcs[k][1] - o.funcs[k][0]
        want_calls = [port.map_code(t)[0] for t in calls(o, k)]
        lo, hi = bisect.bisect_left(n.begins, nv - 0x40000), bisect.bisect_right(n.begins, nv + 0x40000)
        cands = [n.begins[j] for j in range(lo, hi) if n.funcs[j][1] - n.funcs[j][0] == size
                 and want <= data_reads(n, j) and calls(n, j) == want_calls]
        print(f'mismatch {v:#x} -> {nv:#x} ({st}): expects {fmt(want)}, reads {fmt(got)}; '
              f'candidates: {fmt(cands) or "none"}; from {", ".join(sorted(starts[v])[:3])}', flush=True)
    print(f'audit: {len(starts)} function starts, {checked} read globals, {bad} mismatch', flush=True)
    return bad


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('old_exe')
    ap.add_argument('new_exe')
    ap.add_argument('--apply', action='store_true')
    ap.add_argument('--repo', default=str(ROOT))
    ap.add_argument('--report', default='port-report.tsv')
    ap.add_argument('--accept', help='reviewed functions, one `OLD[=NEW] reason` per line')
    ap.add_argument('--audit', action='store_true', help='check the function map against the globals each function reads; run on unported sources')
    ap.add_argument('--code', action='append', metavar='DIR',
                    help='folder under --repo whose literals are rewritten in full (repeatable; default adapters)')
    args = ap.parse_args()
    repo = pathlib.Path(args.repo)
    code_dirs = args.code or ['adapters']
    port = Port(args.old_exe, args.new_exe, args.accept)
    print(f'old {port.o.sha}\nnew {port.n.sha}\nfunctions mapped {len(port.fmap)} of {len(port.o.funcs)}', flush=True)
    if args.audit:
        audit(port, repo, code_dirs)
        return
    rw = Rewriter(port, repo)
    changed = {}
    adapters = sorted(p for d in code_dirs for p in (repo / d).rglob('*') if p.suffix in CODE_SUFFIXES + ('.json',))
    for path in adapters:
        rel = path.relative_to(repo).as_posix()
        text = path.read_bytes().decode('utf-8')
        if path.suffix == '.json':
            if not re.search(r'"(?:\w*_)?rva"|' + port.o.sha, text):
                continue
            new = rw.json_file(rel, text.replace('\r\n', '\n'))
            if '\r\n' in text:
                new = new.replace('\n', '\r\n')
        else:
            new = apply(text, rw.code_file(rel, text))
        if new != text:
            changed[path] = new
    # Digests whose (rva, size) live in another file, for example the step pins.
    for path in adapters:
        text = changed.get(path) or path.read_bytes().decode('utf-8')
        new = SHA.sub(lambda m: rw.learned_sha.get(m.group(0), m.group(0)), text)
        if new != text:
            changed[path] = new
    others = [p for d in ('src', 'tests', 'tools') if d not in code_dirs for p in (repo / d).rglob('*')
              if p.suffix in CODE_SUFFIXES + ('.py', '.json') and p.name != 'port_game_build.py' and 'third_party' not in p.parts]
    for path in sorted(others):
        rel = path.relative_to(repo).as_posix()
        text = path.read_bytes().decode('utf-8')
        new = apply(text, rw.learned_file(rel, text))
        if new != text:
            changed[path] = new
    for key, value in sorted(rw.counts.items()):
        print(f'{value:6}  {key}')
    print(f'files changed {len(changed)}, report items {len(rw.report)}')
    with open(args.report, 'w', encoding='utf-8') as fh:
        fh.write('file\tline\tkind\told\tstatus\tdetail\n')
        for row in sorted(rw.report):
            fh.write('\t'.join(str(x) for x in row) + '\n')
    if args.apply:
        for path, text in changed.items():
            path.write_bytes(text.encode('utf-8'))
        print('applied')
    else:
        for path in sorted(changed):
            print('would change', path.relative_to(repo).as_posix())


if __name__ == '__main__':
    sys.exit(main())
