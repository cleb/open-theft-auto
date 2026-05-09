# Copilot Instructions

## Local image generation

When creating or replacing game art assets, use the local image generator in `imagegen/` when appropriate.

Setup:

```bash
cd imagegen
./install-linux.sh
```

Usage:

```bash
cd imagegen
. .venv/bin/activate
PYTORCH_CUDA_ALLOC_CONF=expandable_segments:True python generate.py \
  "<prompt>" \
  --width <width> --height <height> \
  --transparent-background \
  --output ../assets/textures/<asset-name>.png
```

Prompt transparent sprites and HUD icons as isolated subjects on a pure white background, then use `--transparent-background`. Do not commit `imagegen/.venv/`, `imagegen/models/`, or `imagegen/outputs/`; commit only the final asset in `assets/` and any source/docs changes.
