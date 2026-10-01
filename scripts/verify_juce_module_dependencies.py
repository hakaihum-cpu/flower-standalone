#!/usr/bin/env python3
from pathlib import Path
import re, sys, xml.etree.ElementTree as ET

if len(sys.argv) != 3:
    raise SystemExit("usage: verify_juce_module_dependencies.py <JUCE root> <project.jucer>")

juce = Path(sys.argv[1]).resolve()
jucer = Path(sys.argv[2]).resolve()
root = ET.parse(jucer).getroot()
android = root.find("./EXPORTFORMATS/ANDROIDSTUDIO")
mods_node = root.find("./MODULES")
paths_node = android.find("./MODULEPATHS") if android is not None else None
if mods_node is None or paths_node is None:
    raise SystemExit("[FAIL] JUCER module nodes missing")

mods = {x.attrib["id"] for x in mods_node.findall("MODULE")}
paths = {x.attrib["id"] for x in paths_node.findall("MODULEPATH")}
if mods != paths:
    raise SystemExit(f"[FAIL] MODULE/MODULEPATH mismatch: modules={sorted(mods)} paths={sorted(paths)}")

deps = {}
versions = {}
for m in sorted(mods):
    header = juce / "modules" / m / f"{m}.h"
    if not header.is_file():
        raise SystemExit(f"[FAIL] JUCE module header missing: {header}")
    text = header.read_text(errors="ignore")
    vm = re.search(r"(?m)^[ \\t]*version:[ \\t]*([^\\s]+)", text)
    dm = re.search(r"(?m)^[ \\t]*dependencies:[ \\t]*([^\\r\\n]*)", text)
    versions[m] = vm.group(1).strip() if vm else None
    raw = dm.group(1).strip() if dm else ""
    deps[m] = {x for x in re.split(r"[\s,]+", raw) if x}

for m in sorted(mods):
    if versions[m] != "9.0.2":
        raise SystemExit(f"[FAIL] unexpected JUCE module version for {m}: {versions[m]}")
    missing = deps[m] - mods
    if missing:
        raise SystemExit(f"[FAIL] missing actual JUCE dependency for {m}: {sorted(missing)}")

print("[PASS] actual JUCE 9.0.2 module declarations match JUCER dependency closure")
