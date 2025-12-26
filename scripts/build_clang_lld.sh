#!/usr/bin/env bash
set -euo pipefail

if ! command -v clang >/dev/null 2>&1 || ! command -v clang++ >/dev/null 2>&1; then
  echo "clang/clang++ not found. Install: sudo apt install -y clang" >&2
  exit 1
fi
if ! command -v ld.lld >/dev/null 2>&1 && ! command -v lld >/dev/null 2>&1; then
  echo "lld not found. Install: sudo apt install -y lld" >&2
  exit 1
fi

# Always build out-of-tree to avoid cache path issues.
cmake -S . -B build-clang \
  -G "Unix Makefiles" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=clang \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_EXE_LINKER_FLAGS="-fuse-ld=lld" \
  -DCMAKE_SHARED_LINKER_FLAGS="-fuse-ld=lld"

cmake --build build-clang -j"$(nproc)"

echo "Build OK (clang++ + lld). To install: sudo cmake --install build-clang"
