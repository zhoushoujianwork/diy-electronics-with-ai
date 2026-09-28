#!/bin/sh
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$project_dir/hardware/vehicle-dual-power-r1"
python3 tools/build_design.py
python3 tools/calculate.py > /dev/null
python3 -m unittest discover -s tests -v
