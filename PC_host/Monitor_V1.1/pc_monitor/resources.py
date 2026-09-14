from __future__ import annotations

import sys
from pathlib import Path


def resource_path(*parts: str) -> Path:
    """Resolve bundled resources in source and PyInstaller one-file builds."""
    bundle_root = getattr(sys, "_MEIPASS", None)
    if bundle_root:
        base = Path(bundle_root)
    else:
        base = Path(__file__).resolve().parents[1]
    return base.joinpath(*parts)
