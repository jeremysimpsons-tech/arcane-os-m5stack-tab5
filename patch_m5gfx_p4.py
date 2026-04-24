# Tab5: forward lvgl/lvgl.h, patch M5GFX font glue, drop duplicate font objects + RISC-V Helium .S
Import("env")
import glob
import os

if env.get("PIOENV") != "tab5":
    Return()

root = os.path.join(env["PROJECT_DIR"], ".pio", "libdeps", "tab5")

# --- 1) lvgl/lvgl.h forwarder for M5GFX __has_include("lvgl/lvgl.h")
lvgl_dir = os.path.join(root, "lvgl")
nested = os.path.join(lvgl_dir, "lvgl")
f_nested = os.path.join(nested, "lvgl.h")
if os.path.isdir(lvgl_dir):
    os.makedirs(nested, exist_ok=True)
    with open(f_nested, "w", encoding="utf-8", newline="\n") as f:
        f.write("#include \"../lvgl.h\"\n")
    print("patch_tab5: wrote", f_nested)
else:
    print("patch_tab5: skip lvgl forwarder (libdeps not ready)")

# --- 2) LGFX: LVGL9 glyph format enum (aligned A1/A2/A4) — keep if M5GFX present
m5gfx = os.path.join(root, "M5GFX")
path = os.path.join(
    m5gfx,
    "src",
    "lgfx",
    "v1",
    "lgfx_fonts.cpp",
) if os.path.isdir(m5gfx) else None
if path and os.path.isfile(path):
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        t = f.read()
    a = t.replace("case LV_FONT_GLYPH_FORMAT_A1_ALIGNED:", "case (lv_font_glyph_format_t)0x11: /* A1 */")
    a = a.replace("case LV_FONT_GLYPH_FORMAT_A2_ALIGNED:", "case (lv_font_glyph_format_t)0x12: /* A2 */")
    a = a.replace("case LV_FONT_GLYPH_FORMAT_A4_ALIGNED:", "case (lv_font_glyph_format_t)0x14: /* A4 */")
    if a != t:
        with open(path, "w", encoding="utf-8", newline="\n") as f:
            f.write(a)
        print("patch_tab5: lgfx_fonts.cpp patched for LVGL9")
    else:
        print("patch_tab5: lgfx_fonts.cpp already patched or different")
else:
    print("patch_tab5: skip lgfx_fonts (M5GFX missing)")

# --- 3) With real LVGL linked, remove M5GFX duplicate font C and font_fmt_txt
if os.path.isdir(m5gfx):
    for c in glob.glob(os.path.join(m5gfx, "src", "lgfx", "Fonts", "lvgl", "*.c")):
        try:
            os.remove(c)
            print("patch_tab5: removed duplicate M5 font", c)
        except OSError as e:
            print("patch_tab5:", e)
    fmt = os.path.join(m5gfx, "src", "lgfx", "v1", "lv_font", "font_fmt_txt.c")
    if os.path.isfile(fmt):
        try:
            os.remove(fmt)
            print("patch_tab5: removed duplicate", fmt)
        except OSError as e:
            print("patch_tab5:", e)
else:
    print("patch_tab5: skip M5 font cleanup (no M5GFX)")

# --- 4) RISC-V: skip Helium .S (float ABI)
lv_root = os.path.join(root, "lvgl")
hel = os.path.join(lv_root, "src", "draw", "sw", "blend", "helium")
if os.path.isdir(hel):
    for name in list(os.listdir(hel)):
        if name.endswith((".S", ".s")):
            p = os.path.join(hel, name)
            try:
                os.remove(p)
                print("patch_tab5: removed", p)
            except OSError as e:
                print("patch_tab5:", e)
