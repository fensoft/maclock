Import("env")

import hashlib
import re
import shutil
import subprocess
from pathlib import Path


project_dir = Path(env.subst("$PROJECT_DIR"))
web_dir = project_dir / "web" / "control-panel"
header_path = project_dir / "src" / "control_panel_page.h"
fingerprint_pattern = re.compile(
    r"Web source SHA-256: ([0-9a-f]{64})"
)


def source_files():
    for path in sorted(web_dir.rglob("*")):
        if not path.is_file():
            continue
        if "node_modules" in path.parts or "dist" in path.parts:
            continue
        if path.name == ".DS_Store":
            continue
        yield path


def source_fingerprint():
    digest = hashlib.sha256()
    for path in source_files():
        digest.update(path.relative_to(web_dir).as_posix().encode())
        digest.update(b"\0")
        digest.update(path.read_bytes())
        digest.update(b"\0")
    return digest.hexdigest()


def generated_fingerprint():
    if not header_path.exists():
        return None
    match = fingerprint_pattern.search(
        header_path.read_text(errors="replace")
    )
    return match.group(1) if match else None


fingerprint = source_fingerprint()
if generated_fingerprint() != fingerprint:
    npm = shutil.which("npm")
    if not npm:
        raise RuntimeError(
            "npm is required to rebuild the Maclock control panel"
        )

    vite = web_dir / "node_modules" / ".bin" / "vite"
    installed_lock = web_dir / "node_modules" / ".package-lock.json"
    package_lock = web_dir / "package-lock.json"
    dependencies_stale = (
        not vite.exists()
        or not installed_lock.exists()
        or (
            package_lock.exists()
            and package_lock.stat().st_mtime
            > installed_lock.stat().st_mtime
        )
    )
    if dependencies_stale:
        subprocess.run(
            [npm, "ci", "--no-audit", "--no-fund"],
            cwd=web_dir,
            check=True,
        )

    subprocess.run(
        [npm, "run", "build"],
        cwd=web_dir,
        check=True,
    )

    if generated_fingerprint() != fingerprint:
        raise RuntimeError(
            "control-panel header fingerprint does not match its sources"
        )

env.Depends(
    env.subst("$BUILD_DIR/${PROGNAME}.elf"),
    str(header_path),
)
