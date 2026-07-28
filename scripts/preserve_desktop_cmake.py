Import("env")

import atexit
import os
from pathlib import Path
import shutil


root = Path(env.subst("$PROJECT_DIR"))
cmake_path = root / "CMakeLists.txt"
backup_path = root / ".pio" / "CMakeLists.desktop"

if not cmake_path.exists() and backup_path.exists():
    shutil.copy2(backup_path, cmake_path)

desktop_cmake = (
    cmake_path.exists()
    and "project(maclock-local LANGUAGES C CXX)"
    in cmake_path.read_text(encoding="utf-8")
)

if desktop_cmake:
    backup_path.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(cmake_path, backup_path)

    version = os.environ.get("MACLOCK_BUILD_VERSION", "1.0.0")
    version = version.lstrip("v")
    cmake_path.write_text(
        "cmake_minimum_required(VERSION 3.16)\n"
        f'set(PROJECT_VER "{version}")\n'
        "include($ENV{IDF_PATH}/tools/cmake/project.cmake)\n"
        "project(maclock)\n",
        encoding="utf-8",
    )

    def restore_desktop_cmake():
        if backup_path.exists():
            shutil.copy2(backup_path, cmake_path)

    atexit.register(restore_desktop_cmake)
