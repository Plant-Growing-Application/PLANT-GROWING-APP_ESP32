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


def main() -> int:
    if not SRC.is_dir():
        print(f"HATA: kaynak dizin yok: {SRC}", file=sys.stderr)
        return 1

    if OUT.exists():
        shutil.rmtree(OUT)
    OUT.mkdir(parents=True)

    total_raw = 0
    total_gz = 0

    for path in sorted(SRC.rglob("*")):
        if not path.is_file():
            continue

        rel = path.relative_to(SRC)
        raw = path.read_bytes()
        total_raw += len(raw)

        if path.suffix.lower() in GZIP_EXT:
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
