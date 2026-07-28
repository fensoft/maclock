Import("env")

import os


version = os.environ.get("MACLOCK_BUILD_VERSION", "").lstrip("v")
if version:
    parts = version.split(".")
    if len(parts) != 3 or not all(part.isdigit() for part in parts):
        raise ValueError(
            "MACLOCK_BUILD_VERSION must use major.minor.patch"
        )
    env.Append(CPPDEFINES=[("MACLOCK_VERSION", f'\\"{version}\\"')])
