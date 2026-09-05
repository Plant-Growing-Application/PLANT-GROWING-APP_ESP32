#!/usr/bin/env python3
"""Frontend varlik uretimi — TASK-047.

`frontend/` altindaki kaynaklari gzip'leyip `data/` altina yazar.
`data/` LittleFS imajina donusur (`pio run -t uploadfs`).

NEDEN BETIK: elle gzip'leme surdurulebilir degildir. Birinin bir dosyayi
guncelleyip gzip'lemeyi unutmasi an meselesidir ve sonuc SESSIZCE eski
arayuzdur — hata mesaji yok, yalnizca yanlis ekran.

KAYNAK surum kontrolunde, URETILENLER degil (`data/` .gitignore'da).

Kullanim:
    python tools/build_assets.py
"""

import gzip
import pathlib
import shutil
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
SRC = ROOT / "frontend"
OUT = ROOT / "data"

# Gzip'lenecek metin varlıklari. Ikili dosyalar (varsa) oldugu gibi kopyalanir.
GZIP_EXT = {".html", ".css", ".js", ".json", ".svg", ".ico"}

# ── JS MODULLERI TEK DOSYADA BIRLESTIRILIR (TASK-070) ───────────────────────
#
# Kaynak `frontend/js/*.js` altinda modullere bolundu (1663 satirlik tek dosya
# surdurulebilir degildi). Cihaza giden cikti YINE TEK `app.js.gz`'dir:
#
#   · her modul ayri dosya olsaydi tarayici 5 ayri istek atardi ve her biri
#     LittleFS'ten okunurdu — AP modunda gozle gorulur bir gecikme
#   · ayri gzip akislari toplamda daha buyuk cikar (sozluk paylasilmaz)
#   · index.html'de script sirasi elle tutulurdu; biri unutulunca hata
#     ancak tarayicida "undefined is not a function" olarak gorunurdu
#
# SIRA ONEMLI: dosyalar ADA GORE siralanir, bu yuzden onekler sayisaldir
# (10-, 20-, ...). Birlestirilmis dosya TEK KAPSAMDIR; `const`/`let` tanimlari
# kullanildiklari yerden once gelmek zorundadir.
JS_DIR = SRC / "js"
JS_BUNDLE_NAME = "app.js"


# ── ACIKLAMALAR KAYNAKTA KALIR, CIHAZA GITMEZ ───────────────────────────────
#
# Bu projede aciklama satirlari belge degil, GEREKCEDIR: bir kararin neden
# oyle alindigi kodun yaninda durur. Onlari kaynaktan silmek bilgi kaybidir.
#
# Ama ESP32'ye giden LittleFS imajinda o bilgiyi kimse okumaz. Yeniden
# tasarimdan sonra paket 64 KB uyari cizgisine dayandi ve olculdugunde
# buyumenin buyuk bolumu aciklamalardan geliyordu.
#
# Cozum: kaynagi oldugu gibi birak, YAYIN sirasinda temizle. Boylece hem
# gerekce korunur hem de cihazdaki alan.
#
# NEDEN TAM AYRISTIRICI (TOKENIZER), NEDEN DUZ IFADE DEGIL: `//` dizisi bir
# metin sabitinin ("https://…") veya duzenli ifadenin icinde de gecebilir.
# Duz ifadeyle silmek, calisan kodun ortasini kesip SESSIZCE bozuk bir paket
# uretirdi — ve hata ancak tarayicida gorunurdu. Asagidaki yurutucu metin
# sabitlerini, sablon dizgilerini (ic ice `${}` dahil) ve duzenli ifadeleri
# ayirt eder.


def _strip_js_comments(src: str) -> str:
    """JS kaynagindan aciklamalari cikarir. Kod anlamini DEGISTIRMEZ.

    ── BAGLAM YIGINI ───────────────────────────────────────────────────────
    Sablon dizgileri IC ICE gecebilir ve bu projede geciyor:

        `<div>${cond ? `<span>${x}</span>` : ''}</div>`

    Ilk yazimda tek bir "sablon derinligi" sayaci vardi ve `${` icindeki
    ikinci backtick ayristiriciyi kod kipinde birakiyordu: o noktadan
    sonrasi metin sanildi, aciklamalar SILINMEDI ve dosyanin yarisi oldugu
    gibi paketlendi. Hata sessizdi — cikti gecerli JS'ti, yalnizca buyuktu.

    Bu yuzden derinlik degil YIGIN: her `` ` `` bir 'tmpl', her `{` bir
    'brace' iter; `}` uste duran 'brace'i cikarir ve yigin tekrar 'tmpl'i
    gosteriyorsa metin kipine DONULUR.
    """
    out = []
    i, n = 0, len(src)
    stack = []                # 'tmpl' | 'brace'
    prev_significant = ""     # duzenli ifade / bolme ayrimi icin

    def in_template() -> bool:
        return bool(stack) and stack[-1] == "tmpl"

    def regex_allowed() -> bool:
        """`/` burada duzenli ifade baslatabilir mi?

        Klasik belirsizlik: `a / b` bolmedir, `replace(/x/, …)` degildir.
        Onceki ANLAMLI simge bir deger sonu ise (tanimlayici, sayi, `)`,
        `]`) bolmedir; aksi hâlde duzenli ifadedir.
        """
        if not prev_significant:
            return True
        c = prev_significant[-1]
        if c.isalnum() or c in "_$":
            # `return /x/` gibi anahtar sozcukten sonra duzenli ifade olur.
            return prev_significant in (
                "return", "typeof", "instanceof", "in", "of", "new",
                "delete", "void", "case", "do", "else", "yield", "await",
            )
        return c not in ")]"

    while i < n:
        ch = src[i]
        nxt = src[i + 1] if i + 1 < n else ""

        # --- sablon dizgisi govdesi ---
        if in_template():
            if ch == "\\":
                out.append(src[i:i + 2]); i += 2; continue
            if ch == "`":
                stack.pop(); out.append(ch); i += 1
                prev_significant = "`"
                continue
            if ch == "$" and nxt == "{":
                stack.append("brace")
                out.append("${"); i += 2
                prev_significant = "{"
                continue
            out.append(ch); i += 1; continue

        # --- aciklamalar ---
        if ch == "/" and nxt == "/":
            while i < n and src[i] != "\n":
                i += 1
            continue
        if ch == "/" and nxt == "*":
            i += 2
            while i < n and not (src[i] == "*" and i + 1 < n and src[i + 1] == "/"):
                i += 1
            i += 2
            # Blok aciklama bir bosluk birakir: `a/**/b` -> `a b`, birlesmesin.
            out.append(" ")
            continue

        # --- metin sabitleri ---
        if ch in "'\"":
            q = ch
            out.append(ch); i += 1
            while i < n:
                if src[i] == "\\":
                    out.append(src[i:i + 2]); i += 2; continue
                out.append(src[i])
                if src[i] == q:
                    i += 1; break
                i += 1
            prev_significant = q
            continue

        if ch == "`":
            stack.append("tmpl")
            out.append(ch); i += 1
            continue

        # --- duzenli ifade ---
        if ch == "/" and regex_allowed():
            out.append(ch); i += 1
            in_class = False
            while i < n:
                c = src[i]
                if c == "\\":
                    out.append(src[i:i + 2]); i += 2; continue
                if c == "[":
                    in_class = True
                elif c == "]":
                    in_class = False
                elif c == "/" and not in_class:
                    out.append(c); i += 1
                    while i < n and src[i].isalpha():   # bayraklar
                        out.append(src[i]); i += 1
                    break
                elif c == "\n":
                    break                               # kapanmamis: birak
                out.append(c); i += 1
            prev_significant = "/"
            continue

        # --- suslu parantez dengesi ---
        if ch == "{":
            stack.append("brace")
        elif ch == "}" and stack and stack[-1] == "brace":
            stack.pop()

        out.append(ch)
        if not ch.isspace():
            # Anahtar sozcuk tanimak icin tanimlayicilari biriktir.
            if ch.isalnum() or ch in "_$":
                if prev_significant and (prev_significant[-1].isalnum()
                                         or prev_significant[-1] in "_$"):
                    prev_significant += ch
                else:
                    prev_significant = ch
            else:
                prev_significant = ch
        i += 1

    if stack:
        # Dengesiz yigin = ayristirici saptı. Sessizce buyuk bir paket
        # uretmektense YAPIYI DURDUR: hata simdi gorunsun, sahada degil.
        raise SystemExit(
            "HATA: JS ayristirici dengesiz baglamla bitti "
            f"({stack[-3:]}); aciklama temizleme guvenli degil")

    # Aciklamalardan geriye kalan bos satirlari topla.
    lines = [ln.rstrip() for ln in "".join(out).split("\n")]
    return "\n".join(ln for ln in lines if ln.strip())


def _strip_css_comments(src: str) -> str:
    """CSS aciklamalarini cikarir. CSS'te ic ice aciklama yoktur."""
    out, i, n = [], 0, len(src)
    while i < n:
        if src[i] == '"' or src[i] == "'":
            q = src[i]
            out.append(src[i]); i += 1
            while i < n:
                if src[i] == "\\":
                    out.append(src[i:i + 2]); i += 2; continue
                out.append(src[i])
                if src[i] == q:
                    i += 1; break
                i += 1
            continue
        if src[i] == "/" and i + 1 < n and src[i + 1] == "*":
            i += 2
            while i < n and not (src[i] == "*" and i + 1 < n and src[i + 1] == "/"):
                i += 1
            i += 2
            continue
        out.append(src[i]); i += 1

    lines = [ln.rstrip() for ln in "".join(out).split("\n")]
    return "\n".join(ln for ln in lines if ln.strip())


def _strip_html_comments(src: str) -> str:
    """HTML aciklamalarini cikarir. Kosullu yorumlar bu projede yok."""
    out, i, n = [], 0, len(src)
    while i < n:
        if src.startswith("<!--", i):
            end = src.find("-->", i + 4)
            i = n if end < 0 else end + 3
            continue
        out.append(src[i]); i += 1

    lines = [ln.rstrip() for ln in "".join(out).split("\n")]
    return "\n".join(ln for ln in lines if ln.strip())


STRIPPERS = {".js": _strip_js_comments, ".css": _strip_css_comments,
             ".html": _strip_html_comments}


def build_js_bundle() -> bytes:
    """`frontend/js/*.js` dosyalarini ada gore siralayip birlestirir."""
    parts = sorted(JS_DIR.glob("*.js"))
    if not parts:
        raise SystemExit(f"HATA: {JS_DIR} altinda hic JS modulu yok")

    chunks = []
    for p in parts:
        text = _strip_js_comments(p.read_text(encoding="utf-8"))
        chunks.append(text)
        print(f"  [bundle] {p.name}  {len(text.encode('utf-8')):>6} bayt (aciklamasiz)")

    # 'use strict' TEK KEZ, en ustte. Her modulun basinda tekrarlansaydi
    # birlesik dosyanin ortasinda gecersiz bir yonerge olarak kalirdi.
    return ("'use strict';\n" + "\n".join(chunks)).encode("utf-8")


def main() -> int:
    if not SRC.is_dir():
        print(f"HATA: kaynak dizin yok: {SRC}", file=sys.stderr)
        return 1

    if OUT.exists():
        shutil.rmtree(OUT)
    OUT.mkdir(parents=True)

    total_raw = 0
    total_gz = 0

    # Once JS paketi: `js/` altindaki moduller tek `app.js` olur.
    bundle = build_js_bundle()
    total_raw += len(bundle)
    packed_bundle = gzip.compress(bundle, compresslevel=9, mtime=0)
    (OUT / (JS_BUNDLE_NAME + ".gz")).write_bytes(packed_bundle)
    total_gz += len(packed_bundle)
    print(f"  {JS_BUNDLE_NAME}  {len(bundle):>6} -> {len(packed_bundle):>6} bayt"
          f"  (-%{100 - (len(packed_bundle) * 100 // max(len(bundle), 1))})")

    for path in sorted(SRC.rglob("*")):
        if not path.is_file():
            continue

        # `js/` modulleri yukarida paketlendi; ayrica kopyalanmazlar.
        if JS_DIR in path.parents:
            continue

        rel = path.relative_to(SRC)
        raw = path.read_bytes()
        total_raw += len(raw)

        if path.suffix.lower() in GZIP_EXT:
            # Aciklamalar cihaza gitmez (bkz. `_strip_js_comments` gerekcesi).
            strip = STRIPPERS.get(path.suffix.lower())
            if strip is not None:
                raw = strip(raw.decode("utf-8")).encode("utf-8")

            # mtime=0: ayni girdi ayni cikti uretsin (tekrarlanabilir yapi).
            dst = OUT / (str(rel) + ".gz")
            dst.parent.mkdir(parents=True, exist_ok=True)
            packed = gzip.compress(raw, compresslevel=9, mtime=0)
            dst.write_bytes(packed)
            total_gz += len(packed)
            pct = 100 - (len(packed) * 100 // max(len(raw), 1))
            print(f"  {rel}  {len(raw):>6} -> {len(packed):>6} bayt  (-%{pct})")
        else:
            dst = OUT / rel
            dst.parent.mkdir(parents=True, exist_ok=True)
            dst.write_bytes(raw)
            total_gz += len(raw)
            print(f"  {rel}  {len(raw):>6} bayt (sikistirilmadi)")

    print(f"\nToplam: {total_raw} -> {total_gz} bayt")
    print(f"LittleFS bolumu: 896 KB — kullanim %{total_gz * 100 // (896 * 1024)}")

    # Eski projede tam Bootstrap 298 KB idi ve %95'i kullanilmiyordu.
    if total_gz > 64 * 1024:
        print("\nUYARI: varliklar 64 KB'i asti. Cerchevesiz tasarim hedefi"
              " 12 KB gzip idi; bir bagimlilik eklendi mi?", file=sys.stderr)

    return 0


if __name__ == "__main__":
    sys.exit(main())
