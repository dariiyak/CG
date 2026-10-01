#!/bin/bash
set -euo pipefail
export PATH="/opt/homebrew/bin:/usr/local/bin:$PATH"
LAB_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
LAB_SDK="${VULKAN_SDK:-$HOME/VulkanSDK/1.4.321.0/macOS}"
if [[ ! -f "$LAB_SDK/../setup-env.sh" ]]; then
  echo "Vulkan SDK not found. Install 1.4.321.0 or set VULKAN_SDK to its macOS directory." >&2
  exit 1
fi
# LunarG's environment script does not support nounset in all versions.
set +u
source "$LAB_SDK/../setup-env.sh"
set -u
cd "$LAB_ROOT"
if [[ ! -x build-debug/cg-lab1.app/Contents/MacOS/cg-lab1 ]]; then
  cmake --preset debug
  cmake --build build-debug --parallel
fi
exec "$LAB_ROOT/build-debug/cg-lab1.app/Contents/MacOS/cg-lab1" "$@"
