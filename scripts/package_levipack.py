import argparse
import json
import zipfile
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--library", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()

    library = args.library.resolve()
    output = args.output.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)

    manifest = {
        "type": "preload-native",
        "name": "Lyrics 3D",
        "author": "Custom",
        "description": "3D world-space floating nametag lyrics synced to local MP3 + LRC files.",
        "version": "1.0.0",
        "entry": "liblyrics3d.so",
    }

    with zipfile.ZipFile(output, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as zf:
        zf.writestr("manifest.json", json.dumps(manifest, indent=2) + "\n")
        zf.write(library, "liblyrics3d.so")

    print(f"Packaged: {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
  
