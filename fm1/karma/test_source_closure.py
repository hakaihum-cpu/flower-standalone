#!/usr/bin/env python3
"""Regression: the MIYAKO-only target must not compile removed KARMA engines.

This exercises the SAME include-graph closure verifier used after applying
the patch to pinned upstream firmware; it does not compile target firmware.
"""
from pathlib import Path
from tempfile import TemporaryDirectory

from verify_port import verify_compiled_source_closure


def require_failure(src: Path, expected: str) -> None:
    try:
        verify_compiled_source_closure(src)
    except RuntimeError as exc:
        if expected not in str(exc):
            raise AssertionError(f"wrong failure: {exc}") from exc
    else:
        raise AssertionError(f"broken source incorrectly passed: {expected}")


with TemporaryDirectory(prefix="fm1-source-closure-") as temp:
    src = Path(temp)
    (src / "felucca.c").write_text('#include "module0.c"\n')
    for index in range(30):
        next_include = (f'#include "module{index + 1}.c"\n'
                        if index < 29 else "")
        (src / f"module{index}.c").write_text(next_include)
    # A complete include graph must be accepted; comments are not references.
    (src / "module15.c").write_text(
        '/* ENG_PRISM, phys_poly( and EM_BOUNCE must not count. */\n'
        '// #include "eng_elem.c"\n'
        '#include "module16.c"\n'
    )
    verify_compiled_source_closure(src)

    (src / "module15.c").write_text(
        '#include "module16.c"\nuint32_t leaked = (uint32_t)ENG_PRISM;\n'
    )
    require_failure(src, "Disabled engine symbol(s) in module15.c")

    (src / "module15.c").write_text(
        '#include "module16.c"\n#include "eng_elem.c"\n'
    )
    require_failure(src, "Forbidden DSP source is still compiled: eng_elem.c")

    (src / "module15.c").write_text('#include "missing_file.c"\n')
    require_failure(src, "Transitive C include is missing: missing_file.c")

print("PASS: #251 source closure detects omitted-engine references, reintroduced files and missing C dependencies")
