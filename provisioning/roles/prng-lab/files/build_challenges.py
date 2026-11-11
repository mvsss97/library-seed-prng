#!/usr/bin/env python3
"""
Génère les défis personnalisés (un jeu par sandbox) du SEED Lab
« Pseudo Random Number Generation ».

Les flags (variables APG de la plateforme, variables.yml) arrivent dans un fichier JSON
root 0600 (--flags-file) écrit par Ansible puis supprimé ; jamais sur une ligne de
commande, que sudo journaliserait. Pour les tests locaux : variables d'environnement
PRNG_FLAG1 ... PRNG_FLAG5. Tous les
paramètres propres à la sandbox (horodatages, IV) sont DÉRIVÉS par SHA-256 du flag ET
d'un sel secret tiré au hasard sur la VM (fichier root 0600, --salt-file) :
  - ils changent d'un étudiant à l'autre ;
  - ils ne peuvent pas servir à vérifier hors ligne un flag deviné (les flags APG
    sont générés par la plateforme avec une graine prévisible : sans le sel, l'IV
    visible dans keygen.log permettrait de tester des flags candidats) ;
  - une ré-exécution du provisioning réutilise le sel et produit les mêmes fichiers.

Tâche 1 : task1/keygen.log + task1/secret.enc
          clé = programme keygen.c (srand(time)) à une heure CONNUE
Tâche 2 : task2/alice_tax_return.pdf.enc + task2/bob_notes.txt
          clé = keygen.c à une heure INCONNUE dans les 2 h précédant la création du fichier
Tâches 3-5 : binaire validateur « seedlab » compilé avec les flags 3, 4 et 5.

Usage (provisioning) :
  build_challenges.py --lab-dir /home/seed/lab --owner seed \
      --flags-file flags.json --salt-file /root/.prng-lab-salt \
      --checker-src seedlab.c --checker-out /usr/local/bin/seedlab
Usage (test local, sans gcc ni glibc) :
  build_challenges.py --lab-dir ./out --salt-file ./salt --no-compile
"""

import argparse
import ctypes
import ctypes.util
import datetime
import hashlib
import json
import os
import platform
import shutil
import subprocess
import sys
import tempfile

KEYSIZE = 16
TWO_HOURS = 7200

# Tâche 1 : instant de génération de la clé, quelque part en 2025 (UTC)
TASK1_BASE = 1735689600            # 2025-01-01 00:00:00 UTC
TASK1_SPAN = 365 * 86400

# Tâche 2 : création du PDF chiffré en avril 2018 (comme dans l'énoncé SEED)
TASK2_BASE = 1522540800            # 2018-04-01 00:00:00 UTC
TASK2_SPAN = 29 * 86400

# 16 premiers octets du PDF = texte clair connu de Bob (énoncé SEED, tâche 2)
PDF_KNOWN_PLAINTEXT = bytes.fromhex("255044462d312e350a25d0d4c5d80a34")


# --------------------------------------------------------------------------- dérivation

def derive_int(flag, label, modulo, salt=""):
    digest = hashlib.sha256(f"seed-prng-lab:{label}:{salt}:{flag}".encode()).digest()
    return int.from_bytes(digest[:8], "big") % modulo


def derive_bytes(flag, label, length, salt=""):
    return hashlib.sha256(f"seed-prng-lab:{label}:{salt}:{flag}".encode()).digest()[:length]


def load_salt(path):
    """Sel secret de la sandbox : créé une seule fois (root, 0600), réutilisé ensuite."""
    if not path:
        return ""
    if os.path.exists(path):
        with open(path, encoding="ascii") as f:
            salt = f.read().strip()
        if salt:
            return salt
        os.remove(path)
    salt = os.urandom(32).hex()
    fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    with os.fdopen(fd, "w", encoding="ascii") as f:
        f.write(salt + "\n")
    return salt


def utc(ts):
    return datetime.datetime.fromtimestamp(ts, datetime.timezone.utc).strftime("%Y-%m-%d %H:%M:%S")


def spelling(flag):
    """Lecture caractère par caractère (évite les confusions l/I/1, O/0)."""
    out = []
    for c in flag:
        kind = "chiffre" if c.isdigit() else ("MAJ" if c.isupper() else "min")
        out.append(f"{c}({kind})")
    return " ".join(out)


# --------------------------------------------------------------------------- rand() glibc

def glibc_rand_emulated(seed, count):
    """Ré-implémentation exacte de srand()/rand() de la glibc (générateur TYPE_3)."""
    word = seed & 0xFFFFFFFF          # srand(unsigned int seed)
    if word == 0:
        word = 1
    r = [word]
    for _ in range(1, 31):            # state[i] = (16807 * state[i-1]) % 2147483647
        hi, lo = divmod(word, 127773)
        word = 16807 * lo - 2836 * hi
        if word < 0:
            word += 2147483647
        r.append(word)
    for i in range(31, 34):
        r.append(r[i - 31])
    for i in range(34, 344):
        r.append((r[i - 31] + r[i - 3]) & 0xFFFFFFFF)
    out = []
    i = 344
    while len(out) < count:
        r.append((r[i - 31] + r[i - 3]) & 0xFFFFFFFF)
        out.append(r[i] >> 1)
        i += 1
    return out


def glibc_rand_native(seed, count):
    """Appelle la vraie glibc de la VM (celle qu'utiliseront les étudiants)."""
    if platform.system() != "Linux":
        return None
    name = ctypes.util.find_library("c")
    if not name:
        return None
    libc = ctypes.CDLL(name)
    libc.srand.argtypes = [ctypes.c_uint]
    libc.rand.restype = ctypes.c_int
    libc.srand(seed)
    return [libc.rand() for _ in range(count)]


def keygen_key(seed):
    """Clé produite par keygen.c (énoncé SEED, listing 1) quand time() vaut « seed »."""
    values = glibc_rand_emulated(seed, KEYSIZE)
    native = glibc_rand_native(seed, KEYSIZE)
    if native is not None and native != values:
        sys.exit("ERROR: emulated rand() differs from the VM glibc rand()")
    return bytes(v % 256 for v in values)


# --------------------------------------------------------------------------- chiffrement

def openssl_aes128cbc(key, iv, data, decrypt=False):
    cmd = ["openssl", "enc", "-aes-128-cbc", "-K", key.hex(), "-iv", iv.hex()]
    if decrypt:
        cmd.insert(2, "-d")
    return subprocess.run(cmd, input=data, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                          check=True).stdout


# --------------------------------------------------------------------------- PDF

def pdf_escape(text):
    return text.replace("\\", "\\\\").replace("(", "\\(").replace(")", "\\)")


def build_pdf(flag):
    """Petit PDF valide qui commence EXACTEMENT par les 16 octets connus de Bob."""
    lines = [
        (20, 720, "Alice - Declaration de revenus 2017"),
        (11, 696, "Document confidentiel - ne pas diffuser"),
        (12, 650, "Contribuable : Alice, CEO"),
        (12, 630, "Revenu imposable : 1 284 500 USD"),
        (12, 610, "Impot du : 402 117 USD"),
        (14, 560, "Flag de la tache 2 : " + flag),
        (9, 540, "Lecture : " + spelling(flag)),
        (10, 500, "SEED Labs - Pseudo Random Number Generation Lab"),
    ]
    stream = "".join(f"BT /F1 {size} Tf 72 {y} Td ({pdf_escape(text)}) Tj ET\n"
                     for size, y, text in lines).encode("latin-1")
    objects = {
        4: b"<< /Type /Catalog /Pages 5 0 R >>",
        5: b"<< /Type /Pages /Kids [6 0 R] /Count 1 >>",
        6: (b"<< /Type /Page /Parent 5 0 R /MediaBox [0 0 612 792] "
            b"/Resources << /Font << /F1 8 0 R >> >> /Contents 7 0 R >>"),
        7: b"<< /Length " + str(len(stream)).encode() + b" >>\nstream\n" + stream + b"endstream",
        8: b"<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>",
    }
    out = bytearray(b"%PDF-1.5\n%\xd0\xd4\xc5\xd8\n")
    offsets = {}
    for num in sorted(objects):
        offsets[num] = len(out)
        out += f"{num} 0 obj\n".encode() + objects[num] + b"\nendobj\n"
    xref = len(out)
    out += b"xref\n0 1\n0000000000 65535 f \n4 5\n"
    for num in sorted(objects):
        out += f"{offsets[num]:010d} 00000 n \n".encode()
    out += b"trailer\n<< /Size 9 /Root 4 0 R >>\nstartxref\n" + str(xref).encode() + b"\n%%EOF\n"
    pdf = bytes(out)
    assert pdf[:16] == PDF_KNOWN_PLAINTEXT, "en-tête PDF inattendu"
    return pdf


# --------------------------------------------------------------------------- fichiers

class Writer:
    def __init__(self, owner):
        self.owner = owner

    def write(self, path, data, mode=0o644, mtime=None):
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "wb") as f:
            f.write(data if isinstance(data, bytes) else data.encode("utf-8"))
        os.chmod(path, mode)
        if self.owner:
            shutil.chown(path, self.owner, self.owner)
        if mtime is not None:
            os.utime(path, (mtime, mtime))


def build_task1(flag, lab, writer, salt=""):
    t_key = TASK1_BASE + derive_int(flag, "t1-time", TASK1_SPAN, salt)
    iv = derive_bytes(flag, "t1-iv", 16, salt)
    key = keygen_key(t_key)
    message = (
        "=== secret.txt (serveur backup-01) ===\n\n"
        "Bravo : vous avez reconstruit la clé AES-128 à partir de l'heure de génération.\n"
        "Leçon : srand(time(NULL)) rend la clé aussi prévisible que l'horloge.\n\n"
        f"Flag de la tâche 1 : {flag}\n"
        f"Lecture            : {spelling(flag)}\n"
    ).encode("utf-8")
    secret = openssl_aes128cbc(key, iv, message)
    assert openssl_aes128cbc(key, iv, secret, decrypt=True) == message
    log = (
        "# Extrait de l'historique root du serveur backup-01 (heures en UTC)\n"
        f"{utc(t_key)} UTC  root  ./keygen > /root/.key        # nouvelle clé AES-128 (programme keygen.c)\n"
        f"{utc(t_key + 9)} UTC  root  openssl enc -aes-128-cbc -K $(tail -1 /root/.key) "
        f"-iv {iv.hex()} -in secret.txt -out secret.enc\n"
        f"{utc(t_key + 11)} UTC  root  shred -u secret.txt /root/.key\n"
    )
    writer.write(os.path.join(lab, "task1", "secret.enc"), secret)
    writer.write(os.path.join(lab, "task1", "keygen.log"), log)
    return {"time": t_key, "key": key.hex(), "iv": iv.hex()}


def build_task2(flag, lab, writer, salt=""):
    t_file = TASK2_BASE + derive_int(flag, "t2-file", TASK2_SPAN, salt)
    t_key = t_file - 60 - derive_int(flag, "t2-offset", TWO_HOURS - 120, salt)
    iv = derive_bytes(flag, "t2-iv", 16, salt)
    key = keygen_key(t_key)
    pdf = build_pdf(flag)
    encrypted = openssl_aes128cbc(key, iv, pdf)
    assert openssl_aes128cbc(key, iv, encrypted, decrypt=True) == pdf
    notes = (
        "Notes de Bob - copie du disque d'Alice\n"
        "======================================\n\n"
        "Fichier volé          : alice_tax_return.pdf.enc\n"
        f"Date de création      : {utc(t_file)} (UTC)\n"
        "Hypothèse             : clé générée par keygen (même programme que la tâche 1)\n"
        "                        dans les DEUX HEURES qui précèdent la création du fichier\n"
        "Algorithme            : aes-128-cbc\n\n"
        "Bloc connu (16 octets) :\n"
        f"  Plaintext  : {PDF_KNOWN_PLAINTEXT.hex()}   (\"%PDF-1.5\" + suite de l'en-tête)\n"
        f"  Ciphertext : {encrypted[:16].hex()}\n"
        f"  IV         : {iv.hex()}\n"
    )
    writer.write(os.path.join(lab, "task2", "alice_tax_return.pdf.enc"), encrypted, mtime=t_file)
    writer.write(os.path.join(lab, "task2", "bob_notes.txt"), notes)
    return {"file_time": t_file, "time": t_key, "key": key.hex(), "iv": iv.hex()}


def c_array(name, data):
    return f"static const unsigned char {name}[] = {{{', '.join(f'0x{b:02x}' for b in data)}}};\n"


def build_checker(flags, src, out):
    xk = os.urandom(32)
    header = "/* Généré par build_challenges.py - supprimé après compilation */\n"
    header += c_array("SEEDLAB_XK", xk)
    for n in (3, 4, 5):
        raw = flags[n].encode()
        obf = bytes(b ^ xk[i % len(xk)] for i, b in enumerate(raw))
        header += c_array(f"SEEDLAB_F{n}", obf)
        header += f"#define SEEDLAB_F{n}_LEN {len(raw)}\n"
    build = tempfile.mkdtemp(prefix="seedlab-")
    try:
        os.chmod(build, 0o700)
        shutil.copy(src, os.path.join(build, "seedlab.c"))
        with open(os.path.join(build, "flags.h"), "w", encoding="utf-8") as f:
            f.write(header)
        subprocess.run(["gcc", "-O2", "-s", "-Wall", "-o", out, os.path.join(build, "seedlab.c"),
                        "-lcrypto", "-lm"], check=True)
    finally:
        shutil.rmtree(build, ignore_errors=True)
    os.chown(out, 0, 0)
    os.chmod(out, 0o711)   # exécution seule : illisible pour l'étudiant


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--lab-dir", required=True)
    parser.add_argument("--owner", help="propriétaire des fichiers du TP (ex. seed)")
    parser.add_argument("--flags-file", help='JSON {"1": flag1, ..., "5": flag5} (sinon PRNG_FLAG1..5)')
    parser.add_argument("--salt-file", help="sel secret de la sandbox (créé s'il n'existe pas)")
    parser.add_argument("--checker-src")
    parser.add_argument("--checker-out")
    parser.add_argument("--no-compile", action="store_true", help="ne pas compiler le validateur (test local)")
    parser.add_argument("--print-params", action="store_true", help="affiche les paramètres (test local uniquement)")
    args = parser.parse_args()

    source = {}
    if args.flags_file:
        with open(args.flags_file, encoding="utf-8") as f:
            source = json.load(f)
    flags = {}
    for n in range(1, 6):
        value = str(source.get(str(n), "")) if args.flags_file else os.environ.get(f"PRNG_FLAG{n}", "")
        if not value or len(value) > 50:
            sys.exit(f"ERROR: flag {n} is missing or longer than 50 characters")
        flags[n] = value

    salt = load_salt(args.salt_file)
    writer = Writer(args.owner)
    p1 = build_task1(flags[1], args.lab_dir, writer, salt)
    p2 = build_task2(flags[2], args.lab_dir, writer, salt)
    if not args.no_compile:
        if not (args.checker_src and args.checker_out):
            sys.exit("ERROR: --checker-src and --checker-out are required")
        build_checker(flags, args.checker_src, args.checker_out)
    if args.print_params:
        print({"task1": p1, "task2": p2})
    print("Challenges generated.")   # ASCII : la locale du module Ansible peut être C/POSIX


if __name__ == "__main__":
    main()
