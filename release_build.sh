#!/usr/bin/env bash
# DUELL release build: the FM-1 package and the three SMK-37 packages (Pro, Elite, MKE-P37), from this repository's
# source, into release/. Used by .github/workflows/release.yml (the Firmware Hub's shared fm1-build workflow, which
# builds and signs the packages on GitHub); it runs the same on a computer with JIELI_TOOLCHAIN and AC79_SDK set.
# The version is the VERSION file.
set -euo pipefail
cd "$(dirname "$0")"
ver="$(tr -d '[:space:]' < VERSION)"
python3 -m pip install --quiet fonttools        # the icon and font generators read the font files with it (the Hub's workflow installs only Pillow)
rm -rf release
mkdir -p release
cd duell
python3 tools/build.py
cp "build/duell-$ver.fwsc" ../release/
for model in pro elite mke; do
    BOARD=smk37 MODEL="$model" python3 tools/build.py
    cp "build/smk37/duell-$ver-smk37-$model.fwsc" ../release/
done
cd ..
ls -l release
