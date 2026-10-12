"""Compatibility entry point for the current unified APK packager."""
from pathlib import Path
import runpy
runpy.run_path(str(Path(__file__).resolve().parents[1]/'tools/package_android.py'),run_name='__main__')
