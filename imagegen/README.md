# Image Generator

This directory contains a local image-generation helper for creating game assets with DreamShaper XL Lightning.

## Install

Run the installer for your platform from this directory:

```bash
cd imagegen
./install-linux.sh
```

On macOS, use `./install-macos.sh`. On Windows, run `.\install-windows.ps1` in PowerShell.

The installer creates `imagegen/.venv/`, installs Python dependencies, and downloads the model checkpoint to `imagegen/models/`. Those generated files are intentionally ignored by git.

## Generate an asset

Activate the virtual environment, then run `generate.py`:

```bash
cd imagegen
. .venv/bin/activate
PYTORCH_CUDA_ALLOC_CONF=expandable_segments:True python generate.py \
  "stylized game HUD icon of a stern police officer's head wearing a dark blue peaked cap with a silver badge, front-facing portrait, clean readable silhouette, isolated on a perfectly pure white background" \
  --width 256 --height 256 \
  --transparent-background \
  --output ../assets/textures/police-chase-indicator.png
```

Use `--transparent-background` for sprite/HUD assets and prompt for an isolated subject on a pure white background for best alpha extraction.

Generated images under `imagegen/outputs/` are ignored. Write final game assets directly to the appropriate `assets/` path when they should be committed.
