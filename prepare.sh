#!/bin/bash
set -e
set -x
ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"

if [ ! -e "$ROOT_DIR/src/minivmac" ]; then
  if [ ! -e "$ROOT_DIR/minivmac-36.04.src.tgz" ]; then
    wget --no-check-certificate https://www.gryphel.com/d/minivmac/minivmac-36.04/minivmac-36.04.src.tgz
  fi
  (cd "$ROOT_DIR/src" && tar xf "$ROOT_DIR/minivmac-36.04.src.tgz")
  for PATCH_FILE in "$ROOT_DIR"/patches/*.patch; do
    [ -e "$PATCH_FILE" ] || continue
    patch -p0 -N -r - -d "$ROOT_DIR" < "$PATCH_FILE"
  done
fi
