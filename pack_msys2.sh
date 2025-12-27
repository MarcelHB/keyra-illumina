#!/bin/bash

dependencies=("libgcc_s_seh-1.dll" "libwinpthread-1.dll" "libstdc++-6.dll" "SDL3.dll")

dest="build/win_x64"
mkdir -p "$dest"

cp "build/keyll.exe" "$dest"

for dep in ${dependencies[@]}; do
  location=($(whereis "$dep"))
  location=${location[1]}
  cp "$location" "$dest/$(basename $location)"
done

docs=("README.md" "LICENSE")
for doc in ${docs[@]}; do
  cp "$doc" "$dest/$doc"
done

# libwinpthread
curl "https://sourceforge.net/p/mingw-w64/mingw-w64/ci/master/tree/mingw-w64-libraries/winpthreads/COPYING?format=raw" > "$dest/LICENSE_libwinpthreads"

# GCC runtime libraries
curl "https://raw.githubusercontent.com/gcc-mirror/gcc/refs/heads/master/COPYING3" > "$dest/LICENSE_libgcc"
curl "https://raw.githubusercontent.com/gcc-mirror/gcc/refs/heads/master/COPYING.RUNTIME" >> "$dest/LICENSE_libgcc"
