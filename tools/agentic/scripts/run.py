#!/usr/bin/env python
import os
import subprocess

# Load .env from project root
script_dir = os.path.dirname(__file__)
agentic_dir = os.path.dirname(script_dir)
tools_dir = os.path.dirname(agentic_dir)
root_dir = os.path.dirname(tools_dir)
dot_env = os.path.join(root_dir, ".env")
print(f"Looking for .env at: {dot_env}")
with open(dot_env, "r") as f:
    for line in f:
        line = line.strip()
        if "=" in line and not line.startswith("#"):
            key, value = line.split("=", 1)
            os.environ[key] = value

# Now run langgraph dev with env vars set
subprocess.run(["langgraph", "dev"])
