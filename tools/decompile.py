"""Run the shared core's 68000 decompiler for DoDonPachi (readable C in src/)."""
import runpy
import sys
from pathlib import Path

TOOLS = Path(__file__).resolve().parent
sys.path[:0] = [str(TOOLS / "core"), str(TOOLS)]
runpy.run_module("m68000.decompile", run_name="__main__")
