#!/usr/bin/env python3
"""Genera un soundfont minimo (una onda seno) solo para probar el instalador y el plugin en GitHub."""
import math, struct, sys

def chunk(cid, data):
    return cid + struct.pack("<I", len(data)) + data + (b"\0" if len(data) & 1 else b"")

def lst(kind, body):
    return chunk(b"LIST", kind + body)

def name20(s):
    return s.encode("ascii")[:19].ljust(20, b"\0")

sr, n = 44100, 44100
smpl = b"".join(struct.pack("<h", int(12000 * math.sin(2 * math.pi * 440 * i / sr))) for i in range(n)) + b"\0" * 92

info = chunk(b"ifil", struct.pack("<HH", 2, 1)) + chunk(b"isng", b"EMU8000\0") + chunk(b"INAM", b"TMU Test\0")
phdr = name20("Test Sine") + struct.pack("<HHHIII", 0, 0, 0, 0, 0, 0) + name20("EOP") + struct.pack("<HHHIII", 0, 0, 1, 0, 0, 0)
pbag = struct.pack("<HH", 0, 0) + struct.pack("<HH", 1, 0)
pmod = b"\0" * 10
pgen = struct.pack("<HH", 41, 0) + b"\0" * 4
inst = name20("Sine") + struct.pack("<H", 0) + name20("EOI") + struct.pack("<H", 1)
ibag = struct.pack("<HH", 0, 0) + struct.pack("<HH", 2, 0)
imod = b"\0" * 10
igen = struct.pack("<HH", 54, 1) + struct.pack("<HH", 53, 0) + b"\0" * 4
shdr = name20("Sine") + struct.pack("<IIIIIBbHH", 0, n, 0, n, sr, 69, 0, 0, 1) + name20("EOS") + b"\0" * 26

pdta = b"".join(chunk(k, v) for k, v in [(b"phdr", phdr), (b"pbag", pbag), (b"pmod", pmod), (b"pgen", pgen),
                                            (b"inst", inst), (b"ibag", ibag), (b"imod", imod), (b"igen", igen), (b"shdr", shdr)])
body = b"sfbk" + lst(b"INFO", info) + lst(b"sdta", chunk(b"smpl", smpl)) + lst(b"pdta", pdta)
open(sys.argv[1], "wb").write(chunk(b"RIFF", body))
