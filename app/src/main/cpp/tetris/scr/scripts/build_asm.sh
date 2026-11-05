
#!/usr/bin/env bash
set -euo pipefail

OUT_DIR="out"
OUT_FILE="$OUT_DIR/full_game.s"
TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT

mkdir -p "$OUT_DIR"

echo "Compiling translation units to assembly..."
for src in *.cpp; do
    if [[ -f "$src" ]]; then
        echo "  -> Compiling $src"
        g++ -std=c++23 -O3 -S -fverbose-asm -masm=intel "$src" -o "$TMP_DIR/${src%.cpp}.s"
    fi
done

echo "Merging into $OUT_FILE..."
> "$OUT_FILE"
for asm_file in "$TMP_DIR"/*.s; do
    echo -e "\n/* ======================================================= */" >> "$OUT_FILE"
    echo -e "/* SOURCE: $(basename "$asm_file" .s).cpp */" >> "$OUT_FILE"
    echo -e "/* ======================================================= */\n" >> "$OUT_FILE"
    cat "$asm_file" >> "$OUT_FILE"
done

echo "Done! Full game assembly saved to $OUT_FILE"