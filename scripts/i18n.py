#!/usr/bin/env python3
"""i18n.py: Übersetzungskataloge der Oberfläche pflegen (gettext-Format, ohne Zusatzwerkzeuge).

Warum es das gibt: Die Texte der Oberfläche stehen deutsch im Quelltext (tr("...")). Für Englisch, Französisch und
Spanisch liegt je eine .po-Datei in apps/whitehole_studio/i18n/. Dieses Skript hält die Dateien aktuell, ohne dass
lupdate/lrelease nötig sind (die gibt es nicht überall, zum Beispiel nicht in jeder Qt-Installation der CI).

  python3 scripts/i18n.py extract   zeigt die Zahl der Texte und Auffälligkeiten
  python3 scripts/i18n.py update    schreibt messages.pot und ergänzt die .po-Dateien (neue Texte mit leerem msgstr)
  python3 scripts/i18n.py check     Exit 1, wenn messages.pot veraltet ist, Übersetzungen fehlen oder Platzhalter abweichen

Gelesen wird jedes tr("…") und QT_TR_NOOP("…") in apps/whitehole_studio/*.cpp und *.hpp. Wird tr() mit einem Namen
aufgerufen (tr(kHinweis)), sucht das Skript die Konstante `const char* … kHinweis = "…";` in derselben Datei.
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "apps" / "whitehole_studio"
I18N = SRC / "i18n"
LANGS = ["en", "fr", "es"]

STRING = r'"(?:[^"\\\n]|\\.)*"'
CALL = re.compile(r'(?<![A-Za-z0-9_])(?:QT_TR_NOOP|tr)\s*\(\s*')
CONST = re.compile(r'(?:constexpr\s+)?const\s+char\s*\*\s*(?:const\s+)?(\w+)\s*=\s*((?:' + STRING + r'\s*)+);')
ESC = {"n": "\n", "t": "\t", "r": "\r", '"': '"', "\\": "\\", "'": "'", "0": "\0"}


def unescape(body: str) -> str:
    out, i = [], 0
    while i < len(body):
        c = body[i]
        if c != "\\":
            out.append(c)
            i += 1
            continue
        n = body[i + 1]
        if n == "u":
            out.append(chr(int(body[i + 2:i + 6], 16)))
            i += 6
        elif n == "x":
            j = i + 2
            while j < len(body) and body[j] in "0123456789abcdefABCDEF":
                j += 1
            out.append(chr(int(body[i + 2:j], 16)))
            i = j
        else:
            out.append(ESC.get(n, n))
            i += 2
    return "".join(out)


def literals(text: str) -> str:
    """Aneinandergereihte Zeichenketten "a" "b" zu einem Text."""
    return "".join(unescape(m[1:-1]) for m in re.findall(STRING, text))


FUNC = re.compile(r'^[A-Za-z_][^\n;]*?(\w+)\([^\n;]*\)[^\n;]*\{\s*$', re.M)


def in_exercise(text: str, pos: int) -> bool:
    """Steht die Position in einer Funktion exercise…(): Rauchtest-Meldungen für Entwickler, nicht für Nutzer."""
    last = None
    for m in FUNC.finditer(text, 0, pos):
        last = m
    return last is not None and last.group(1).startswith("exercise")


def extract():
    found = {}  # msgid -> [Fundstellen]
    problems = []
    for path in sorted(list(SRC.glob("*.cpp")) + list(SRC.glob("*.hpp"))):
        text = path.read_text(encoding="utf-8")
        consts = {m.group(1): literals(m.group(2)) for m in CONST.finditer(text)}
        for m in CALL.finditer(text):
            rest = text[m.end():]
            if in_exercise(text, m.start()):
                continue
            line = text.count("\n", 0, m.start()) + 1
            where = f"{path.name}:{line}"
            lit = re.match(r'(?:' + STRING + r'\s*)+', rest)
            if lit:
                found.setdefault(literals(lit.group(0)), []).append(where)
                continue
            ident = re.match(r'(\w+)\s*[,)]', rest)
            if re.match(r'[\w:]+\s*(?:\.|->|\()', rest):
                continue  # tr(eintrag.text): der Text steht anderswo als QT_TR_NOOP("…") und wird dort gefunden
            if ident and ident.group(1) in consts:
                found.setdefault(consts[ident.group(1)], []).append(where)
            elif ident:
                problems.append(f"{where}: tr({ident.group(1)}) mit unbekannter Konstante (Text wird nicht übersetzt)")
            else:
                problems.append(f"{where}: tr(...) mit nicht lesbarem Argument: {rest[:40].strip()!r}")
    return found, problems


def esc(text: str) -> str:
    return (text.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n").replace("\t", "\\t").replace("\r", "\\r"))


def entry(msgid, msgstr="", refs=None):
    lines = []
    if refs:
        lines.append("#: " + " ".join(refs[:3]))
    lines.append(f'msgid "{esc(msgid)}"')
    lines.append(f'msgstr "{esc(msgstr)}"')
    return "\n".join(lines)


HEADER = '''msgid ""
msgstr ""
"Project-Id-Version: WhiteHoleFX\\n"
"Language: {lang}\\n"
"MIME-Version: 1.0\\n"
"Content-Type: text/plain; charset=UTF-8\\n"
"Content-Transfer-Encoding: 8bit\\n"
'''


def read_po(path: Path):
    """Sehr einfacher Leser: msgid -> msgstr (nur das, was dieses Skript selbst schreibt)."""
    result, mid, mstr, cur = {}, None, None, None
    if not path.exists():
        return result
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if line.startswith("msgid "):
            if mid is not None and mstr is not None:
                result[mid] = mstr
            mid, mstr, cur = literals(line[6:]), None, "id"
        elif line.startswith("msgstr "):
            mstr, cur = literals(line[7:]), "str"
        elif line.startswith('"'):
            if cur == "id":
                mid += literals(line)
            elif cur == "str":
                mstr += literals(line)
    if mid is not None and mstr is not None:
        result[mid] = mstr
    result.pop("", None)
    return result


def write_all(found):
    I18N.mkdir(exist_ok=True)
    ids = sorted(found)
    pot = HEADER.format(lang="") + "\n" + "\n\n".join(entry(i, "", found[i]) for i in ids) + "\n"
    (I18N / "messages.pot").write_text(pot, encoding="utf-8")
    for lang in LANGS:
        old = read_po(I18N / f"{lang}.po")
        body = "\n\n".join(entry(i, old.get(i, ""), found[i]) for i in ids)
        (I18N / f"{lang}.po").write_text(HEADER.format(lang=lang) + "\n" + body + "\n", encoding="utf-8")


PH = re.compile(r"%(?:n|\d{1,2})")


def check(found):
    errors = []
    pot = read_po(I18N / "messages.pot")
    if set(pot) != set(found):
        errors.append(f"messages.pot ist veraltet ({len(set(found) - set(pot))} neu, {len(set(pot) - set(found))} entfallen): "
                      "python3 scripts/i18n.py update")
    for lang in LANGS:
        po = read_po(I18N / f"{lang}.po")
        missing = [i for i in found if not po.get(i)]
        if missing:
            errors.append(f"{lang}.po: {len(missing)} Texte ohne Übersetzung, z. B. {missing[0]!r}")
        for i in found:
            tr = po.get(i)
            if tr and sorted(PH.findall(i)) != sorted(PH.findall(tr)):
                errors.append(f"{lang}.po: Platzhalter weichen ab: {i!r} -> {tr!r}")
    return errors


def main():
    cmd = sys.argv[1] if len(sys.argv) > 1 else "check"
    found, problems = extract()
    for p in problems:
        print("Hinweis:", p)
    if cmd == "extract":
        print(f"{len(found)} Texte")
        return 0
    if cmd == "update":
        write_all(found)
        print(f"{len(found)} Texte in messages.pot und {', '.join(l + '.po' for l in LANGS)} geschrieben")
        return 0
    if cmd == "check":
        errors = check(found)
        for e in errors:
            print("FEHLER:", e)
        print("Übersetzungen vollständig" if not errors else f"{len(errors)} Fehler")
        return 1 if errors else 0
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main())
