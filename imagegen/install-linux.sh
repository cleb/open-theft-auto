#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

ensure_python() {
  if command -v python3 >/dev/null 2>&1; then
    return
  fi

  if command -v apt-get >/dev/null 2>&1; then
    sudo apt-get update
    sudo apt-get install -y python3 python3-venv python3-pip
  elif command -v dnf >/dev/null 2>&1; then
    sudo dnf install -y python3 python3-pip
  elif command -v pacman >/dev/null 2>&1; then
    sudo pacman -Sy --needed python python-pip
  else
    echo "Python 3 is required. Install Python 3.10+ with your Linux package manager, then rerun this script." >&2
    exit 1
  fi
}

ensure_python

python3 - <<'PY'
import sys

if sys.version_info < (3, 10) or sys.version_info >= (3, 13):
    raise SystemExit("Python 3.10, 3.11, or 3.12 is required for the pinned PyTorch wheel.")
PY

if ! python3 -m venv .venv; then
  if command -v apt-get >/dev/null 2>&1; then
    sudo apt-get update
    sudo apt-get install -y python3-venv python3-pip
    python3 -m venv .venv
  else
    echo "Could not create a virtual environment. Install the Python venv package for your distro, then rerun this script." >&2
    exit 1
  fi
fi
. .venv/bin/activate
python -m pip install --upgrade pip setuptools wheel

if command -v nvidia-smi >/dev/null 2>&1; then
  python -m pip install torch==2.5.1 torchvision==0.20.1 --index-url https://download.pytorch.org/whl/cu121
else
  python -m pip install torch==2.5.1 torchvision==0.20.1 --index-url https://download.pytorch.org/whl/cpu
fi

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
echo 'PYTORCH_CUDA_ALLOC_CONF=expandable_segments:True python generate.py "photorealistic top-down sports car, isolated on a pure white background" --transparent-background --output outputs/car.png'
