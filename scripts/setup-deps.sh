#!/usr/bin/env bash
set -euo pipefail

if [[ "${EUID}" -ne 0 ]]; then
    echo "This script installs system packages and must run as root." >&2
    echo "Run: sudo $0" >&2
    exit 1
fi

apt-get update
apt-get install -y \
    build-essential \
    nasm \
    pkg-config \
    libglfw3-dev \
    libgl1-mesa-dev \
    unzip
