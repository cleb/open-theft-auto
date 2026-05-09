#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

if ! command -v python3 >/dev/null 2>&1; then
  if command -v brew >/dev/null 2>&1; then
    brew install python
  else
    echo "Python 3 is required. Install Python 3.10+ from python.org or Homebrew, then rerun this script." >&2
    exit 1
  fi
fi

python3 - <<'PY'
import sys

if sys.version_info < (3, 10) or sys.version_info >= (3, 13):
    raise SystemExit("Python 3.10, 3.11, or 3.12 is required for the pinned PyTorch wheel.")
PY

python3 -m venv .venv
. .venv/bin/activate
python -m pip install --upgrade pip setuptools wheel

python -m pip install torch==2.5.1 torchvision==0.20.1
python -m pip install -r requirements.txt

python - <<'PY'
from pathlib import Path
from huggingface_hub import hf_hub_download

Path("models").mkdir(exist_ok=True)
hf_hub_download(
    repo_id="Lykon/dreamshaper-xl-lightning",
    filename="DreamShaperXL_Lightning.safetensors",
    local_dir="models",
)
print("Model installed at models/DreamShaperXL_Lightning.safetensors")
PY

echo
echo "Done. Example:"
echo '. .venv/bin/activate'
echo 'python generate.py "photorealistic top-down sports car, isolated on a pure white background" --transparent-background --output outputs/car.png'
