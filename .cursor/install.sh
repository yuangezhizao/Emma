#!/usr/bin/env bash
# Idempotent Cloud Agent bootstrap for the Emma ESP-IDF (ESP32-S3) project.
# Installs ESP-IDF + the xtensa QEMU emulator, fetches component dependencies,
# then configures the esp32s3 target and builds the firmware.
set -euo pipefail

IDF_BRANCH="release/v5.5"
IDF_DIR="${HOME}/esp/esp-idf"
IDF_TARGET="esp32s3"

# Run from the repository root regardless of how the script is invoked.
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${REPO_ROOT}"

# 1. System prerequisites for building ESP-IDF (libslirp0 is needed by the
#    bundled QEMU's open_eth user networking).
export DEBIAN_FRONTEND=noninteractive
sudo apt-get update -y
sudo apt-get install -y --no-install-recommends \
  git wget flex bison gperf python3 python3-pip python3-venv \
  cmake ninja-build ccache libffi-dev libssl-dev dfu-util \
  libusb-1.0-0 libslirp0

# 2. ESP-IDF sources (shallow clone once; re-runs are a no-op).
if [ ! -d "${IDF_DIR}/.git" ]; then
  mkdir -p "$(dirname "${IDF_DIR}")"
  git clone -b "${IDF_BRANCH}" --depth 1 --recursive --shallow-submodules \
    https://github.com/espressif/esp-idf.git "${IDF_DIR}"
fi

# 3. Toolchain + QEMU (the installers skip anything already present).
"${IDF_DIR}/install.sh" "${IDF_TARGET}"
# shellcheck disable=SC1091
. "${IDF_DIR}/export.sh" >/dev/null 2>&1
python "${IDF_DIR}/tools/idf_tools.py" install qemu-xtensa

# 4. Make idf.py available in interactive shells (matches the repo devcontainer).
BASHRC="${HOME}/.bashrc"
IDF_SOURCE_LINE=". \"${IDF_DIR}/export.sh\" >/dev/null 2>&1"
if ! grep -qF "${IDF_SOURCE_LINE}" "${BASHRC}" 2>/dev/null; then
  printf '\n# ESP-IDF environment\n%s\n' "${IDF_SOURCE_LINE}" >>"${BASHRC}"
fi

# 5. Project component dependencies (repos.json driven; empty by default).
mkdir -p dependencies
python3 fetch_repos.py

# 6. Configure target (only when needed) and build the firmware.
if ! grep -q "CONFIG_IDF_TARGET=\"${IDF_TARGET}\"" sdkconfig 2>/dev/null; then
  idf.py set-target "${IDF_TARGET}"
fi
idf.py build
