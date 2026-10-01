#!/bin/bash
set -euo pipefail
export PATH="/opt/homebrew/bin:/usr/local/bin:$PATH"
LAB_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$LAB_ROOT"
mkdir -p screenshots build-debug/verification
ctest --test-dir build-debug --output-on-failure
./scripts/run-macos.sh --frames 60 --capture screenshots/perspective.ppm > build-debug/verification/perspective.log 2>&1
./scripts/run-macos.sh --frames 80 --preset 1 --resize-test --capture screenshots/orthographic.ppm > build-debug/verification/orthographic.log 2>&1
./scripts/run-macos.sh --frames 60 --preset 2 --capture screenshots/animation.ppm > build-debug/verification/animation.log 2>&1
./scripts/run-macos.sh --frames 150 --animate --resize-test --capture build-debug/verification/motion.ppm > build-debug/verification/motion.log 2>&1
for scenario in perspective orthographic animation motion; do
  tail -2 "build-debug/verification/$scenario.log"
done
