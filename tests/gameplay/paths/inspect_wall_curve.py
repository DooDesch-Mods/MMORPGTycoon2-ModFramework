"""Retain local exact-image Wall solver proof without copying game bytes."""
import argparse
import hashlib
import json
from pathlib import Path
import struct

EXPECTED = "bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6"
BODIES = [(0x5EB530, 0x220, "mmoCursorBehaviourWall::AddPointAt(vsVector3D const&)"),
          (0x5E9AB0, 0x210, "mmoCursorBehaviourWall::CalculateVelocityOfSegment(int)"),
          (0x5EA480, 0x10B0, "mmoCursorBehaviourWall::PlotSpline()")]


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--exe", required=True, type=Path)
    p.add_argument("--output", required=True, type=Path)
    a = p.parse_args()
    data = a.exe.read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    if digest != EXPECTED:
        raise ValueError(f"Executable SHA256 {digest} differs from required {EXPECTED}")
    pe = struct.unpack_from("<I", data, 60)[0]
    count = struct.unpack_from("<H", data, pe + 6)[0]
    optional = struct.unpack_from("<H", data, pe + 20)[0]
    sections = [struct.unpack_from("<8sIIII", data, pe + 24 + optional + i * 40) for i in range(count)]
    table, records = struct.unpack_from("<II", data, pe + 12)
    functions = set()
    index = 0
    while index < records:
        value, section_index, kind, storage, aux = struct.unpack_from("<IhHBB", data, table + index * 18 + 8)
        if kind == 32 and 0 < section_index <= len(sections):
            functions.add(sections[section_index - 1][2] + value)
        index += 1 + aux
    rows = []
    for rva, size, name in BODIES:
        if rva not in functions or min(p for p in functions if p > rva) - rva != size:
            raise ValueError(f"Exact COFF function extent differs RVA={rva:x}")
        section = next(s for s in sections if s[2] <= rva and rva + size <= s[2] + s[3])
        raw = section[4] + rva - section[2]
        rows.append(dict(rva=hex(rva), size=size, sha256=hashlib.sha256(data[raw:raw + size]).hexdigest(), name=name))
    a.output.write_text(json.dumps(dict(exe_sha256=digest, bodies=rows), indent=2) + "\n", encoding="utf-8")
    print(json.dumps(dict(status="confirmed-static-proof", bodies=len(rows), output=str(a.output))))


if __name__ == "__main__":
    main()
