#!/usr/bin/env python3
"""Capture matching configuration screenshots in every supported language."""

from pathlib import Path
import json
import subprocess
import time
from urllib.parse import urlencode
from urllib.request import Request, urlopen

from PIL import Image, ImageChops, ImageStat


ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "img" / "manual"
PORT = 18112
LANGUAGES = {"en": 0, "fr": 1, "es": 2, "de": 3, "it": 4}
PAGES = {
    "configuration-hub": 0, "language": 1, "regional": 2,
    "date-time": 3, "display": 4, "clock-face": 5,
    "face-style": 6, "face-details": 7, "screensaver": 8,
    "night-schedule": 9, "night-behavior": 10, "chime-mode": 11,
    "chime-sound": 12, "chime-volume": 13, "quiet-hours": 14,
    "start-mode": 15, "preferences": 16, "wifi": 17, "tools": 18,
    "software-update-page": 19, "about": 20,
}


def api(path: str, values: dict | None = None) -> dict:
    request = Request(
        f"http://127.0.0.1:{PORT}{path}",
        data=urlencode(values).encode() if values is not None else None,
        method="POST" if values is not None else "GET")
    with urlopen(request) as response:
        return json.load(response)


def set_language(language: int) -> None:
    appearance = api("/api/state")["appearance"]
    values = {
        "language": language, "face": appearance["face"],
        "theme": appearance["theme"], "brightness": 12,
        "hourFormat": appearance["hourFormat"],
        "leadingZero": int(appearance["leadingZero"]),
        "seconds": int(appearance["seconds"]),
        "weekday": int(appearance["weekday"]),
        "accent": appearance["accent"], "fontSize": appearance["fontSize"],
        "weather": int(appearance["weather"]),
        "flipSpeed": appearance["flipSpeed"],
    }
    if not api("/api/appearance", values).get("ok"):
        raise RuntimeError("Could not select language")


def geometry() -> tuple[int, int]:
    for attempt in range(20):
        try:
            output = subprocess.check_output((
                "osascript", "-e",
                'tell application "Maclock Simulator" to activate',
                "-e", "delay 0.2", "-e",
                'tell application "System Events" to tell process '
                '"Maclock Simulator" to get position of window 1'), text=True)
            x, y = (int(value.strip()) for value in output.split(","))
            return x + 9, y + 66
        except subprocess.CalledProcessError:
            if attempt == 19:
                raise
            time.sleep(0.5)
    raise RuntimeError("Maclock Simulator window is unavailable")


def screenshot(destination: Path) -> None:
    desktop_path = Path("/private/tmp/maclock-localized-manual.png")
    left, top = geometry()
    subprocess.check_call(("screencapture", "-x", str(desktop_path)))
    with Image.open(desktop_path) as desktop:
        image = desktop.crop((left, top, left + 608, top + 448))
        image = image.resize((304, 224), Image.Resampling.NEAREST)
        destination.parent.mkdir(parents=True, exist_ok=True)
        image.save(destination, optimize=True)


def similarity(reference: Path, localized: Path) -> float:
    with Image.open(reference) as left, Image.open(localized) as right:
        left = left.convert("L").resize((76, 56), Image.Resampling.BOX)
        right = right.convert("L").resize((76, 56), Image.Resampling.BOX)
        difference = ImageChops.difference(left, right)
        mean = ImageStat.Stat(difference).mean[0]
        return max(0.0, 100.0 * (1.0 - mean / 255.0))


def main() -> None:
    for name, page in PAGES.items():
        result = api("/api/manual/page", {"page": page})
        if not result.get("ok"):
            raise RuntimeError(result)
        time.sleep(0.15)
        reference = OUT / f"{name}.png"
        for code, language in LANGUAGES.items():
            set_language(language)
            time.sleep(0.12)
            destination = (
                reference if code == "en" else OUT / code / f"{name}.png")
            screenshot(destination)
            if code != "en":
                score = similarity(reference, destination)
                minimum_score = 65.0 if name == "language" else 72.0
                if score < minimum_score:
                    raise RuntimeError(
                        f"{code}/{name} differs from English layout: {score:.1f}%")
                print(f"{code}/{name}: {score:.1f}%")
        set_language(0)


if __name__ == "__main__":
    main()
