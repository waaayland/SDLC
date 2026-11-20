#!/usr/bin/env python3
"""Render PlantUML (.puml) UML 2.5 models in docs/uml/models/ to PNG via Kroki."""
from pathlib import Path
import requests
import sys

ROOT = Path(__file__).resolve().parent
MODELS_DIR = ROOT / "models"
IMAGES_DIR = ROOT / "images"
SDD_IMAGES_DIR = ROOT.parent.parent / "sdd" / "images"

KROKI_URL = "https://kroki.io/plantuml/png"


def render_file(puml_path: Path) -> Path:
    source = puml_path.read_text(encoding="utf-8")
    resp = requests.post(
        KROKI_URL,
        data=source.encode("utf-8"),
        headers={"Content-Type": "text/plain; charset=utf-8"},
        timeout=30,
    )
    if resp.status_code != 200:
        raise RuntimeError(f"Kroki error {resp.status_code} for {puml_path.name}: {resp.text[:400]}")
    IMAGES_DIR.mkdir(parents=True, exist_ok=True)
    out_path = IMAGES_DIR / f"{puml_path.stem}.png"
    out_path.write_bytes(resp.content)
    return out_path


def main() -> int:
    puml_files = sorted(MODELS_DIR.glob("*.puml"))
    if not puml_files:
        print("No .puml files found.")
        return 1
    for f in puml_files:
        out = render_file(f)
        print(f"Rendered {f.name} -> {out.relative_to(ROOT)} ({out.stat().st_size} bytes)")
        if SDD_IMAGES_DIR.exists():
            sdd_copy = SDD_IMAGES_DIR / f"uml-{f.stem}.png"
            sdd_copy.write_bytes(out.read_bytes())
    return 0


if __name__ == "__main__":
    sys.exit(main())
