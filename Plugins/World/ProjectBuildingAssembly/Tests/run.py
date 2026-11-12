"""Compile the production layout core as a small C++20 host test."""
import json
import os
from pathlib import Path
import subprocess
from time import perf_counter

import jsonschema

plugin = Path(__file__).resolve().parents[1]
repo = plugin.parents[2]
scratch = repo / 'tmp/world/building_assembly/host'
scratch.mkdir(parents=True, exist_ok=True)
kit = plugin / 'Data/Kits/original_test_v1.json'
jsonschema.Draft202012Validator(json.loads((kit.parent / '../Schemas/modular-kit.schema.json').read_text())).validate(json.loads(kit.read_text()))
exe = scratch / ('plan_test.exe' if os.name == 'nt' else 'plan_test')
module = plugin / 'Source/ProjectBuildingAssemblyEditor'
started = perf_counter()
subprocess.run([os.environ.get('CXX', 'g++'), '-std=c++20', '-O2', '-Wall', '-Wextra',
                '-DPROJECTBUILDINGASSEMBLYEDITOR_API=', '-I', str(module / 'Public'),
                str(module / 'Private/ProjectBuildingAssemblyPlan.cpp'), str(plugin / 'Tests/plan_test.cpp'),
                '-o', str(exe)], check=True)
subprocess.run([str(exe)], check=True)
print(f'Compile and test: {perf_counter()-started:.3f} seconds')
