#!/usr/bin/env python3
"""Builds the shader-record tool when its sources changed and runs it: `python3 tools/shader_record.py check --lint
port/shader-lists`. Commands: check, explain, close, canonical, merge, fields (docs/data-packs.md, "Shader records")."""
import os
import sys

import data_pack

if __name__ == "__main__":
    tool = str(data_pack.shader_record_tool())
    os.execv(tool, ["shader-record", *sys.argv[1:]])
