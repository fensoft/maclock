#!/bin/bash
set -e
set -x
ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"

if [ ! -e "$ROOT_DIR/src/minivmac" ]; then
  if [ ! -e "$ROOT_DIR/minivmac-36.04.src.tgz" ]; then
    wget --no-check-certificate https://www.gryphel.com/d/minivmac/minivmac-36.04/minivmac-36.04.src.tgz
  fi
  (cd "$ROOT_DIR/src" && tar xf "$ROOT_DIR/minivmac-36.04.src.tgz")
  PATCH_FILE="$ROOT_DIR/patches/asc_interrupt_pulsenotify.patch"
  if [ -f "$PATCH_FILE" ]; then
    patch -p0 -N -r - -d "$ROOT_DIR" < "$PATCH_FILE"
  fi
fi

if [ ! -e data/vMac.ROM ]; then
  wget http://psychonaut.bplaced.net/MinivMac/vMac.ROM -O data/vMac.ROM
fi
if [ ! -e data/disk1.dsk ]; then
  wget http://psychonaut.bplaced.net/MinivMac/MinivMac_disks/System7.zip -O data/System7.zip
  unzip data/System7.zip
  mv System7.DSK data/disk1.dsk
  rm data/System7.zip
fi