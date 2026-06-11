#!/usr/bin/env bash
set -euo pipefail

# Ubuntu/Debian setup for continuing HVM2 CUDA measurements.
# This installs build dependencies, CUDA Toolkit, and the NVIDIA kernel module
# package for the currently running Ubuntu HWE kernel.

kernel_version="$(uname -r)"
nvidia_module_package="linux-modules-nvidia-580-open-${kernel_version}"

if ! sudo -n true 2>/dev/null; then
  cat >&2 <<EOF
ERROR: passwordless sudo is not available in this session.

Run this locally first, then rerun this script:

  sudo apt update
  sudo apt install -y ${nvidia_module_package} nvidia-driver-580-open nvidia-cuda-toolkit
  sudo reboot

After reboot, verify:

  nvidia-smi
  nvcc --version
EOF
  exit 1
fi

sudo apt update
sudo apt install -y \
  build-essential \
  clang \
  git \
  curl \
  pkg-config \
  python3 \
  python3-venv \
  python3-pip \
  "${nvidia_module_package}" \
  nvidia-driver-580-open \
  nvidia-cuda-toolkit

if ! command -v rustup >/dev/null 2>&1; then
  curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y
  # shellcheck source=/dev/null
  source "$HOME/.cargo/env"
fi

rustup default stable

printf '\nChecking toolchain...\n'
rustc --version
cargo --version
python3 --version

if command -v nvidia-smi >/dev/null 2>&1; then
  nvidia-smi
else
  printf 'WARNING: nvidia-smi not found. Install NVIDIA driver before CUDA measurements.\n' >&2
fi

if command -v nvcc >/dev/null 2>&1; then
  nvcc --version
else
  cat >&2 <<'EOF'
WARNING: nvcc not found after apt install.
On Ubuntu this can happen if CUDA Toolkit is installed from NVIDIA's repository
instead of the distro package, or if /usr/local/cuda/bin is not in PATH.

Typical PATH fix, if CUDA is installed under /usr/local/cuda:
  export PATH=/usr/local/cuda/bin:$PATH
  export LD_LIBRARY_PATH=/usr/local/cuda/lib64:${LD_LIBRARY_PATH:-}
EOF
fi

cat <<'EOF'

Next steps:
  git clone https://github.com/HigherOrderCO/HVM2.git
  cd HVM2
  cargo build --release

Then from reducerCombinators:
  python3 tools/hvm2_cuda_sum_tree_sweep.py \
    --hvm /path/to/HVM2/target/release/hvm \
    --mode cuda \
    --runs 30 \
    --sizes 16 32 64 128 \
    --out docs/results/raw/hvm2_cuda_sum_tree_linux.csv
EOF
