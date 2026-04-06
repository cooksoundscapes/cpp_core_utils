#!/bin/bash

# Abort on any failure
set -e

SYSROOT=${SYSROOT:-$HOME/rpi-sysroot}

export SYSROOT
envsubst < cross/armv7h-clang.ini.template > cross/armv7h-clang.ini

meson setup build-armv7 \
  --cross-file cross/armv7h-clang.ini \
  -Dcore:sysroot=$SYSROOT \
  "$@"