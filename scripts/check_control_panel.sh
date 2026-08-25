#!/bin/sh
set -eu

base_url="${1:-http://127.0.0.1:8088}"
state="$(curl -fsS "$base_url/api/state")"
screens="$(curl -fsS "$base_url/api/loading/list")"
project="$(curl -fsS "$base_url/api/loading/project?name=macintosh")"

case "$state" in
  *'"loadingScreen"'*) ;;
  *) echo "Missing appearance.loadingScreen" >&2; exit 1 ;;
esac

for required in '"id": "module"' '"template": "plugin_{i2c}.png"' '"sound_volume": 60'; do
  case "$project" in
    *"$required"*) ;;
    *) echo "Macintosh boot project is missing required visual layers" >&2; exit 1 ;;
  esac
done

case "$screens" in
  *'"screens"'*) ;;
  *) echo "Missing loading-screen list" >&2; exit 1 ;;
esac

archive_entries="$(curl -fsS "$base_url/api/configuration/export" | strings)"
case "$archive_entries" in
  *'loading/macintosh/loading.json'*) ;;
  *) echo "Configuration archive omitted loading project" >&2; exit 1 ;;
esac

curl -fsS -X POST "$base_url/api/loading/preview" \
  -d 'screen=macintosh' >/dev/null
