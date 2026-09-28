#!/usr/bin/env python3
"""Compatibility import path for older host tooling.

New code should import :mod:`reliefos_layout`. The legacy names below map to
the same constants and functions, preserving existing developer scripts.
"""
from __future__ import annotations

import reliefos_layout as _canonical
from reliefos_layout import *  # noqa: F401,F403

for _name, _value in vars(_canonical).items():
    if "RELIEFOS" in _name:
        globals()[_name.replace("RELIEFOS", "LEONOS")] = _value
