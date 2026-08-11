#!/bin/sh
set -eu

source_file=${1:-source/3ds/gui_hard.c}

if [ ! -f "$source_file" ]; then
    printf '%s\n' "menu source contract: missing source file: $source_file" >&2
    exit 1
fi

# This gate is intentionally first: the legacy checks must not pass against a
# baseline that has not adopted the per-frame palette snapshot yet.
if ! grep -Eq '(^|[^[:alnum:]_])MenuPaletteSnapshot([^[:alnum:]_]|$)' "$source_file"; then
    printf '%s\n' \
        "menu source contract: MenuPaletteSnapshot is not integrated in $source_file" >&2
    exit 1
fi

extract_menu_ranges() {
    sed -n \
        -e '/static void draw_main_menu_shell/,/static void first_menu/p' \
        -e '/static bool rom_loader_impl/,/static void multiplayer_main/p' \
        -e '/static void options(/,/static void video_settings/p' \
        "$source_file"
}

target=$(extract_menu_ranges)
if [ -z "$target" ]; then
    printf '%s\n' "menu source contract: target menu ranges are empty" >&2
    exit 1
fi

if ! printf '%s\n' "$target" | grep -Eq 'MenuPaletteSnapshot'; then
    printf '%s\n' \
        "menu source contract: target menu ranges do not consume MenuPaletteSnapshot" >&2
    exit 1
fi

legacy=$(printf '%s\n' "$target" | grep -En \
    'tVBOpt\.TINT|TINT_[[:alnum:]_]+|COLOR_BRIGHTNESS|menu_theme[[:space:]]*\(|menu_background_color[[:space:]]*\(' \
    || true)
if [ -n "$legacy" ]; then
    printf '%s\n' "menu source contract: legacy menu colour source found:" >&2
    printf '%s\n' "$legacy" >&2
    exit 1
fi

# The interactive options page owns its complete panel rendering.  The
# generic button renderer must not draw a second copy of input-only labels.
if ! grep -Eq \
    'buttons\[i\]\.str && !buttons\[i\]\.input_only' "$source_file"; then
    printf '%s\n' \
        "menu source contract: input-only options still use the legacy label renderer" >&2
    exit 1
fi

background_calls=$(grep -Ec 'draw_menu_background\(' "$source_file" || true)
if [ "$background_calls" -ne 3 ]; then
    printf '%s\n' \
        "menu source contract: expected one full-screen background helper and two menu calls" >&2
    exit 1
fi

printf '%s\n' "menu source contract: passed ($source_file)"
