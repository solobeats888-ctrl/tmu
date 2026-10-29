#!/usr/bin/env python3
"""
Arma el instalador TMU.pkg (formato "flat package" de macOS) sin necesitar una Mac.

Junta:
  - los paquetes del plugin (VST3 y AU) que compila GitHub Actions
  - un paquete con los soundfonts, que se instalan en /Library/Application Support/TMU/SoundFonts

Como los soundfonts pesan varios GB, el trabajo se puede hacer por etapas:
  make_pkg.py init   --out TMU.pkg --sounds lista.tsv --component A.pkg --component B.pkg [--mkbom ruta]
  make_pkg.py step   --out TMU.pkg [--budget 120]      (repetir hasta que diga DONE)
  make_pkg.py finish --out TMU.pkg
o de una sola vez:
  make_pkg.py all    --out TMU.pkg --sounds lista.tsv --component ...

lista.tsv: una linea por archivo: <ruta del .sf2>\t<nombre destino.sf2>
"""
import argparse, bz2, ctypes, ctypes.util, hashlib, json, os, struct, subprocess, sys, time, zlib
from xml.sax.saxutils import escape

VERSION = "1.0.0"
SOUNDS_ID = "com.viszeversa.tmu.sounds"
SOUNDS_LOCATION = "/Library/Application Support/TMU"
UID, GID = 0, 80   # root:admin

# ---------------------------------------------------------------- SHA1 reanudable
class Sha1:
    """SHA1 cuyo estado se puede guardar entre ejecuciones (usa libcrypto via ctypes).
    Si no hay libcrypto usable, cae a hashlib (sin reanudar)."""
    CTX_SIZE = 96

    def __init__(self, state_hex=None, resumable=True):
        self.lib = None
        if resumable and sys.platform.startswith("linux"):
            name = ctypes.util.find_library("crypto")
            if name:
                try:
                    self.lib = ctypes.CDLL(name)
                    self.lib.SHA1_Update.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_size_t]
                except OSError:
                    self.lib = None
        if self.lib is not None:
            self.ctx = ctypes.create_string_buffer(self.CTX_SIZE)
            if state_hex:
                ctypes.memmove(self.ctx, bytes.fromhex(state_hex), self.CTX_SIZE)
            else:
                self.lib.SHA1_Init(self.ctx)
        else:
            if state_hex:
                raise RuntimeError("no se puede reanudar SHA1 sin libcrypto")
            self.h = hashlib.sha1()

    def update(self, data):
        if self.lib is not None:
            self.lib.SHA1_Update(self.ctx, data, len(data))
        else:
            self.h.update(data)

    def state(self):
        if self.lib is None:
            raise RuntimeError("estado no disponible")
        return self.ctx.raw.hex()

    def hexdigest(self):
        if self.lib is not None:
            out = ctypes.create_string_buffer(20)
            tmp = ctypes.create_string_buffer(self.ctx.raw, self.CTX_SIZE)
            self.lib.SHA1_Final(out, tmp)
            return out.raw.hex()
        return self.h.hexdigest()

# ---------------------------------------------------------------- cpio (odc) y gzip sin compresion
def cpio_header(name, mode, size, mtime, ino):
    nb = name.encode("utf-8") + b"\0"
    h = "070707%06o%06o%06o%06o%06o%06o%06o%011o%06o%011o" % (
        0, ino & 0o777777, mode, UID, GID, 1, 0, int(mtime) & 0o77777777777, len(nb), size)
    return h.encode("ascii") + nb

GZIP_HEADER = b"\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\x03"   # deflate, sin nombre, OS=unix

def stored_blocks(data):
    """Bloques deflate 'stored' (sin compresion, los .sf2 casi no comprimen)."""
    out = bytearray()
    for i in range(0, len(data), 65535):
        chunk = data[i:i + 65535]
        out += b"\x00" + struct.pack("<HH", len(chunk), 0xFFFF ^ len(chunk)) + chunk
    return bytes(out)

# ---------------------------------------------------------------- lector de xar (para los .pkg del plugin)
def read_xar(path):
    """Devuelve dict: ruta_interna -> bytes (solo archivos)."""
    import xml.etree.ElementTree as ET
    with open(path, "rb") as f:
        hdr = f.read(28)
        magic, hsize, ver, clen, ulen, alg = struct.unpack(">4sHHQQI", hdr)
        if magic != b"xar!":
            raise ValueError("%s no es un .pkg (xar) valido" % path)
        f.seek(hsize)
        toc = zlib.decompress(f.read(clen))
        heap = hsize + clen
        root = ET.fromstring(toc).find("toc")
        files = {}

        def walk(node, prefix):
            for fe in node.findall("file"):
                name = fe.findtext("name")
                p = prefix + name
                if fe.findtext("type") == "directory":
                    walk(fe, p + "/")
                    continue
                d = fe.find("data")
                if d is None:
                    files[p] = b""
                    continue
                f.seek(heap + int(d.findtext("offset")))
                raw = f.read(int(d.findtext("length")))
                enc = d.find("encoding").get("style") if d.find("encoding") is not None else "application/octet-stream"
                if enc == "application/x-gzip":
                    raw = zlib.decompress(raw)
                elif enc == "application/x-bzip2":
                    raw = bz2.decompress(raw)
                files[p] = raw
        walk(root, "")
        return files

# ---------------------------------------------------------------- escritor de xar
def toc_xml(entries, created, toc_cksum_size=20):
    """entries: arbol [(name, children|None, data_info|None)]. data_info=(offset,length,sha1)"""
    ids = [0]
    def fnode(name, children, info, ind):
        ids[0] += 1
        sp = "  " * ind
        s = '%s<file id="%d">\n%s <name>%s</name>\n' % (sp, ids[0], sp, escape(name))
        if children is not None:
            s += "%s <type>directory</type>\n%s <mode>0755</mode>\n" % (sp, sp)
            for c in children:
                s += fnode(*c, ind + 1)
        else:
            off, length, sha = info
            s += ("%s <type>file</type>\n%s <mode>0644</mode>\n"
                  "%s <data>\n%s  <length>%d</length>\n%s  <offset>%d</offset>\n%s  <size>%d</size>\n"
                  '%s  <encoding style="application/octet-stream"/>\n'
                  '%s  <extracted-checksum style="sha1">%s</extracted-checksum>\n'
                  '%s  <archived-checksum style="sha1">%s</archived-checksum>\n%s </data>\n') % (
                sp, sp, sp, sp, length, sp, off, sp, length, sp, sp, sha, sp, sha, sp)
        s += "%s</file>\n" % sp
        return s
    body = "".join(fnode(*e, 2) for e in entries)
    return ('<?xml version="1.0" encoding="UTF-8"?>\n<xar>\n <toc>\n'
            '  <checksum style="sha1">\n   <offset>0</offset>\n   <size>%d</size>\n  </checksum>\n'
            '  <creation-time>%s</creation-time>\n%s </toc>\n</xar>\n') % (
        toc_cksum_size, created, body)

# ---------------------------------------------------------------- textos del instalador
WELCOME = """<html><body style="font-family:-apple-system,Helvetica;font-size:13px">
<h2 style="color:#ff3d6e">TMU &mdash; SoundFont Library</h2>
<p>Este instalador agrega a tu Mac:</p>
<ul>
<li><b>TMU</b> en formato VST3 y Audio Unit (para FL Studio y otros DAW).</li>
<li>La biblioteca de <b>{n} soundfonts</b> ({gb:.1f} GB), en <i>/Library/Application Support/TMU/SoundFonts</i>.</li>
</ul>
<p>Necesita {gb:.1f} GB libres. La copia puede tardar varios minutos.</p>
</body></html>"""

CONCLUSION = """<html><body style="font-family:-apple-system,Helvetica;font-size:13px">
<h2 style="color:#ff3d6e">Listo</h2>
<p>Abri FL Studio, entra a <b>Options &gt; Manage plugins</b> y toca <b>Find installed plugins</b>. TMU aparece como generador.</p>
<p>Para sumar tus propios soundfonts, ponelos en la carpeta <b>Musica / TMU SoundFonts</b> y toca <b>RESCAN</b> en el plugin.</p>
<p><b>Si FL no ve el plugin o Mac lo bloquea</b>, abri la app Terminal, pega esto y apreta Enter (te pide la contrasena de la Mac):</p>
<pre style="font-size:11px">sudo xattr -cr "/Library/Audio/Plug-Ins/VST3/TMU.vst3"
sudo xattr -cr "/Library/Audio/Plug-Ins/Components/TMU.component"
sudo codesign --force --deep --sign - "/Library/Audio/Plug-Ins/VST3/TMU.vst3"
sudo codesign --force --deep --sign - "/Library/Audio/Plug-Ins/Components/TMU.component"
killall -9 AudioComponentRegistrar</pre>
<p>Despues reinicia FL Studio. Estas instrucciones tambien estan en LEEME.txt.</p>
</body></html>"""

# ---------------------------------------------------------------- etapas
def load_state(out):
    with open(out + ".state.json") as f:
        return json.load(f)

def save_state(out, st):
    tmp = out + ".state.json.tmp"
    with open(tmp, "w") as f:
        json.dump(st, f)
    os.replace(tmp, out + ".state.json")

def cmd_init(a):
    sounds = []
    with open(a.sounds, encoding="utf-8") as f:
        for line in f:
            line = line.rstrip("\n")
            if not line.strip():
                continue
            src, dst = line.split("\t")
            st = os.stat(src)
            sounds.append({"src": src, "dst": dst, "size": st.st_size, "mtime": int(st.st_mtime)})

    components = stash_components(a.out, a.component)
    # espacio reservado antes del Payload para el TOC y los paquetes del plugin (se pueden agregar al final)
    payload_pos = 64 * 1024 * 1024
    small = sum(sum(c["files"].values()) for c in components)
    if small + 8 * 1024 * 1024 > payload_pos:
        payload_pos = ((small + 8 * 1024 * 1024) // (1 << 20) + 1) << 20

    st = {"out": a.out, "sounds": sounds, "components": components, "mkbom": a.mkbom,
          "payload_pos": payload_pos, "payload_len": 0, "next": 0, "crc": 0, "isize": 0,
          "sha1": None, "bom": [], "phase": "header", "resumable": not a.oneshot}

    with open(a.out, "wb") as f:
        f.truncate(payload_pos)
    save_state(a.out, st)
    print("init ok: %d soundfonts, %.2f GB, payload en %d" % (
        len(sounds), sum(s["size"] for s in sounds) / 1e9, payload_pos))
    return st

def stash_components(out, paths):
    components = []
    for cpath in paths:
        files = read_xar(cpath)
        name = os.path.basename(cpath)
        stash = out + ".parts/" + name
        os.makedirs(stash, exist_ok=True)
        keep = {}
        for p, data in files.items():
            if p in ("Bom", "PackageInfo", "Payload", "Scripts"):
                with open(os.path.join(stash, p), "wb") as g:
                    g.write(data)
                keep[p] = len(data)
        if "PackageInfo" not in keep or "Payload" not in keep:
            raise SystemExit("El componente %s no tiene PackageInfo/Payload" % cpath)
        components.append({"name": name, "dir": stash, "files": keep})
    return components

def write_payload(f, st, sha, data):
    """Agrega bytes ya deflate-encoded al Payload."""
    f.seek(st["payload_pos"] + st["payload_len"])
    f.write(data)
    sha.update(data)
    st["payload_len"] += len(data)

def feed(f, st, sha, raw):
    """Agrega bytes del cpio (sin comprimir) al gzip."""
    st["crc"] = zlib.crc32(raw, st["crc"])
    st["isize"] += len(raw)
    write_payload(f, st, sha, stored_blocks(raw))

def cmd_step(a, st=None, sha=None):
    st = st or load_state(a.out)
    if sha is None:
        sha = Sha1(st["sha1"], resumable=st["resumable"])
    t0 = time.time()
    done_now = 0
    with open(st["out"], "r+b") as f:
        if st["phase"] == "header":
            write_payload(f, st, sha, GZIP_HEADER)
            ino = 1
            for name in (".", "./SoundFonts"):
                feed(f, st, sha, cpio_header(name, 0o40755, 0, time.time(), ino))
                st["bom"].append([name, "40755", 0, 0])
                ino += 1
            st["phase"] = "files"
            if st["resumable"]:
                st["sha1"] = sha.state()
                save_state(a.out, st)

        while st["phase"] == "files" and st["next"] < len(st["sounds"]):
            if st["resumable"] and done_now > 0 and time.time() - t0 > a.budget:
                break
            s = st["sounds"][st["next"]]
            name = "./SoundFonts/" + s["dst"]
            feed(f, st, sha, cpio_header(name, 0o100644, s["size"], s["mtime"], 10 + st["next"]))
            # cksum (POSIX) del archivo para el Bom, calculado en el mismo pase de lectura
            ck = subprocess.Popen(["cksum"], stdin=subprocess.PIPE, stdout=subprocess.PIPE)
            got = 0
            with open(s["src"], "rb") as src:
                while True:
                    buf = src.read(4 * 1024 * 1024)
                    if not buf:
                        break
                    got += len(buf)
                    ck.stdin.write(buf)
                    feed(f, st, sha, buf)
            out, _ = ck.communicate()
            if got != s["size"]:
                raise SystemExit("El archivo cambio mientras se copiaba: " + s["src"])
            st["bom"].append([name, "100644", s["size"], int(out.split()[0]), s["mtime"]])
            st["next"] += 1
            done_now += 1
            if st["resumable"]:   # guardar progreso despues de cada archivo
                st["sha1"] = sha.state()
                save_state(a.out, st)
            print("  + %s (%.0f MB)" % (s["dst"], s["size"] / 1e6), flush=True)

        if st["phase"] == "files" and st["next"] >= len(st["sounds"]):
            trailer = cpio_header("TRAILER!!!", 0, 0, 0, 0)
            pad = (-(st["isize"] + len(trailer))) % 512
            feed(f, st, sha, trailer + b"\0" * pad)
            write_payload(f, st, sha, b"\x01\x00\x00\xff\xff" + struct.pack("<II", st["crc"] & 0xFFFFFFFF, st["isize"] & 0xFFFFFFFF))
            st["phase"] = "payload_done"
            st["payload_sha1"] = sha.hexdigest()

    if st["resumable"] and st["phase"] != "payload_done":
        st["sha1"] = sha.state()
    save_state(a.out, st)
    done = st["phase"] == "payload_done"
    print("DONE" if done else "PROGRESS %d/%d archivos, %.2f GB escritos" % (
        st["next"], len(st["sounds"]), st["payload_len"] / 1e9))
    return st, sha, done

def make_bom(st, workdir):
    lst = os.path.join(workdir, "bom.txt")
    with open(lst, "w", encoding="utf-8") as g:
        for e in st["bom"]:
            if e[1] == "40755":
                g.write("%s\t40755\t%d/%d\t%d\n" % (e[0], UID, GID, int(time.time())))
            else:
                g.write("%s\t100644\t%d/%d\t%d\t%d\t%d\n" % (e[0], UID, GID, e[4], e[2], e[3]))
    bom = os.path.join(workdir, "Bom")
    subprocess.check_call([st["mkbom"], "-i", lst, bom])
    with open(bom, "rb") as f:
        return f.read()

def cmd_finish(a):
    st = load_state(a.out)
    if st["phase"] != "payload_done":
        raise SystemExit("Falta terminar las etapas (step)")
    if a.component:
        st["components"] = stash_components(a.out, a.component)
    if a.mkbom != "mkbom":
        st["mkbom"] = a.mkbom
    if not st["components"]:
        raise SystemExit("Faltan los paquetes del plugin (--component TMU-VST3.pkg --component TMU-AU.pkg)")
    work = a.out + ".parts"
    os.makedirs(work, exist_ok=True)
    total_size = sum(s["size"] for s in st["sounds"])
    sounds_kb = total_size // 1024 + 1

    # ---- piezas chicas
    pieces = []   # (ruta xar, bytes)
    refs = []
    for c in st["components"]:
        import re
        pi = open(os.path.join(c["dir"], "PackageInfo"), "rb").read().decode("utf-8")
        ident = re.search(r'identifier="([^"]+)"', pi).group(1)
        ver = re.search(r'version="([^"]+)"', pi).group(1)
        kb = re.search(r'installKBytes="(\d+)"', pi)
        refs.append((ident, ver, int(kb.group(1)) if kb else 0, c["name"]))
        for p in ("Bom", "PackageInfo", "Scripts", "Payload"):
            if p in c["files"]:
                pieces.append((c["name"] + "/" + p, open(os.path.join(c["dir"], p), "rb").read()))

    sounds_pkg = "TMU-SoundFonts.pkg"
    bom = make_bom(st, work)
    pkginfo = ('<?xml version="1.0" encoding="utf-8"?>\n'
               '<pkg-info format-version="2" identifier="%s" version="%s" install-location="%s" '
               'auth="root" overwrite-permissions="false" relocatable="false" postinstall-action="none">\n'
               '    <payload numberOfFiles="%d" installKBytes="%d"/>\n'
               '    <bundle-version/>\n    <upgrade-bundle/>\n    <update-bundle/>\n    <atomic-update-bundle/>\n'
               '    <strict-identifier/>\n    <relocate/>\n</pkg-info>\n') % (
        SOUNDS_ID, VERSION, SOUNDS_LOCATION, len(st["bom"]), sounds_kb)
    pieces.append((sounds_pkg + "/Bom", bom))
    pieces.append((sounds_pkg + "/PackageInfo", pkginfo.encode("utf-8")))
    refs.append((SOUNDS_ID, VERSION, sounds_kb, sounds_pkg))

    lines = "".join('            <line choice="%s"/>\n' % r[0] for r in refs)
    choices = "".join('    <choice id="%s" visible="false">\n        <pkg-ref id="%s"/>\n    </choice>\n' % (r[0], r[0]) for r in refs)
    pkgrefs = "".join('    <pkg-ref id="%s" version="%s" onConclusion="none" installKBytes="%d">#%s</pkg-ref>\n' % r for r in refs)
    distribution = ('<?xml version="1.0" encoding="utf-8"?>\n'
                    '<installer-gui-script minSpecVersion="2">\n'
                    '    <title>TMU</title>\n'
                    '    <welcome file="welcome.html" mime-type="text/html"/>\n'
                    '    <conclusion file="conclusion.html" mime-type="text/html"/>\n'
                    '    <options customize="never" require-scripts="false" rootVolumeOnly="true" hostArchitectures="x86_64,arm64"/>\n'
                    '    <domains enable_anywhere="false" enable_currentUserHome="false" enable_localSystem="true"/>\n'
                    '    <choices-outline>\n        <line choice="default">\n%s        </line>\n    </choices-outline>\n'
                    '    <choice id="default"/>\n%s%s</installer-gui-script>\n') % (lines, choices, pkgrefs)
    pieces.insert(0, ("Distribution", distribution.encode("utf-8")))
    gb = total_size / 1e9
    pieces.insert(1, ("Resources/welcome.html", WELCOME.format(n=len(st["sounds"]), gb=gb).encode("utf-8")))
    pieces.insert(2, ("Resources/conclusion.html", CONCLUSION.encode("utf-8")))

    # ---- arbol para el TOC
    def build_tree(heap_start):
        layout = {}
        off = 20
        for path, data in pieces:
            layout[path] = (off, len(data), hashlib.sha1(data).hexdigest())
            off += len(data)
        small_end = off
        payload_off = st["payload_pos"] - heap_start
        if payload_off < small_end:
            raise SystemExit("No alcanza el espacio reservado")
        layout[sounds_pkg + "/Payload"] = (payload_off, st["payload_len"], st["payload_sha1"])
        tree = {}
        order = [p for p, _ in pieces] + [sounds_pkg + "/Payload"]
        for p in order:
            parts = p.split("/")
            node = tree
            for d in parts[:-1]:
                node = node.setdefault(d, {})
            node[parts[-1]] = layout[p]
        def conv(d):
            out = []
            for k, v in d.items():
                if isinstance(v, dict):
                    out.append((k, conv(v), None))
                else:
                    out.append((k, None, v))
            return out
        return conv(tree)

    # El TOC va antes del heap y su largo cambia los offsets. Se usa zlib sin compresion (nivel 0)
    # y se rellena con espacios para que el largo final sea exactamente el reservado.
    created = time.strftime("%Y-%m-%dT%H:%M:%S", time.gmtime())
    target = len(zlib.compress(toc_xml(build_tree(28 + 100000), created).encode("utf-8"), 0)) + 256
    for _ in range(10):
        xml = toc_xml(build_tree(28 + target), created).encode("utf-8")
        need = target - len(zlib.compress(xml, 0))
        if need < 0:
            target += -need + 256
            continue
        xml = xml[:-len(b"</xar>\n")] + b" " * need + b"</xar>\n"
        comp = zlib.compress(xml, 0)
        if len(comp) == target:
            break
        target += 64
    else:
        raise SystemExit("No se pudo armar el TOC")

    with open(st["out"], "r+b") as f:
        f.seek(0)
        f.write(struct.pack(">4sHHQQI", b"xar!", 28, 1, len(comp), len(xml), 1))
        f.write(comp)
        f.write(hashlib.sha1(comp).digest())
        for _, data in pieces:
            f.write(data)
        if f.tell() > st["payload_pos"]:
            raise SystemExit("Se piso el payload")
    print("PKG LISTO: %s (%.2f GB)" % (st["out"], os.path.getsize(st["out"]) / 1e9))

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("cmd", choices=["init", "step", "finish", "all"])
    ap.add_argument("--out", required=True)
    ap.add_argument("--sounds")
    ap.add_argument("--component", action="append", default=[])
    ap.add_argument("--mkbom", default="mkbom")
    ap.add_argument("--budget", type=float, default=110)
    a = ap.parse_args()
    a.oneshot = a.cmd == "all"
    if a.cmd == "init":
        cmd_init(a)
    elif a.cmd == "step":
        cmd_step(a)
    elif a.cmd == "finish":
        cmd_finish(a)
    else:
        cmd_init(a)
        st, sha, done = None, None, False
        while not done:
            st, sha, done = cmd_step(a, st, sha)
        a.component = []
        cmd_finish(a)

if __name__ == "__main__":
    main()
