$ErrorActionPreference = "Stop"

Set-Location $PSScriptRoot

$python = Get-Command python -ErrorAction SilentlyContinue
if (-not $python) {
    $python = Get-Command py -ErrorAction SilentlyContinue
}
if (-not $python) {
    throw "Python 3.10+ is required. Install it from https://www.python.org/downloads/windows/ and rerun this script."
}

if ($python.Source -like "*\py.exe") {
    & py -3 -c "import sys; raise SystemExit(0 if (3, 10) <= sys.version_info[:2] < (3, 13) else 'Python 3.10, 3.11, or 3.12 is required for the pinned PyTorch wheel.')"
} else {
    & python -c "import sys; raise SystemExit(0 if (3, 10) <= sys.version_info[:2] < (3, 13) else 'Python 3.10, 3.11, or 3.12 is required for the pinned PyTorch wheel.')"
}

if ($python.Source -like "*\py.exe") {
    & py -3 -m venv .venv
} else {
    & python -m venv .venv
}

$venvPython = Join-Path $PSScriptRoot ".venv\Scripts\python.exe"
& $venvPython -m pip install --upgrade pip setuptools wheel

$hasNvidia = $false
try {
    $null = & nvidia-smi 2>$null
    if ($LASTEXITCODE -eq 0) {
        $hasNvidia = $true
    }
} catch {
    $hasNvidia = $false
}

if ($hasNvidia) {
    & $venvPython -m pip install torch==2.5.1 torchvision==0.20.1 --index-url https://download.pytorch.org/whl/cu121
} else {
    & $venvPython -m pip install torch==2.5.1 torchvision==0.20.1 --index-url https://download.pytorch.org/whl/cpu
}

& $venvPython -m pip install -r requirements.txt

$downloadScript = @"
from pathlib import Path
from huggingface_hub import hf_hub_download

Path("models").mkdir(exist_ok=True)
hf_hub_download(
    repo_id="Lykon/dreamshaper-xl-lightning",
    filename="DreamShaperXL_Lightning.safetensors",
    local_dir="models",
)
print("Model installed at models/DreamShaperXL_Lightning.safetensors")
"@

$downloadScript | & $venvPython -

Write-Host ""
Write-Host "Done. Example:"
Write-Host ".\.venv\Scripts\Activate.ps1"
Write-Host 'python generate.py "photorealistic top-down sports car, isolated on a pure white background" --transparent-background --output outputs/car.png'
