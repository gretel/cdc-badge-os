Import("env")

from pathlib import Path


def _patch_unused_ym(path: Path) -> None:
    if not path.is_file():
        return
    text = path.read_text()
    if "(void)ym;" in text:
        return
    lines = text.splitlines()
    out = []
    changed = False
    for line in lines:
        out.append(line)
        if "int16_t ym =" in line:
            indent = line[: len(line) - len(line.lstrip())]
            out.append(f"{indent}(void)ym;")
            changed = True
    if changed:
        new_text = "\n".join(out)
        if text.endswith("\n"):
            new_text += "\n"
        path.write_text(new_text)


project_dir = Path(env.subst("$PROJECT_DIR"))
libdeps = project_dir / ".pio" / "libdeps"

for candidate in libdeps.glob("**/GxEPD2/src/gdey/GxEPD2_579_GDEY0579T93.cpp"):
    _patch_unused_ym(candidate)
for candidate in libdeps.glob("**/GxEPD2/src/gdey3c/GxEPD2_579c_GDEY0579Z93.cpp"):
    _patch_unused_ym(candidate)
