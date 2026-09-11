"""Strict parser for scripts/config/ue_path.conf (+ ue_path.local.conf).

Single source of truth for the conf GRAMMAR on the Python side.
Consumed by .githooks/pre-commit, the governance validator, and the
conformance test suite. Mirrors Resolve-UEConfig.ps1 semantics.

Grammar (see ue_path.conf header): KEY=VALUE, no spaces around '=',
full-line '#' comments only, duplicate/unknown/empty = error.
Merge: local.conf overrides tracked conf PER KEY.
"""
from __future__ import annotations

import os
import re

KNOWN_KEYS = (
    "UE_PATH",
    "UE_SOURCE_PATH",
    "BUILD_TARGET",
    "BUILD_CONFIG",
    "BUILD_PLATFORM",
)

_LINE_RE = re.compile(r"^([A-Z_][A-Z0-9_]*)=(.*)$")


class UEConfError(ValueError):
    """Raised for any grammar violation; message names file and line."""


def parse_conf_file(path):
    """Parse one conf file strictly. Returns dict of key -> value."""
    result = {}
    with open(path, "r", encoding="ascii") as fh:
        for lineno, raw in enumerate(fh, 1):
            line = raw.rstrip("\r\n")
            stripped = line.strip()
            if not stripped or stripped.startswith("#"):
                continue
            m = _LINE_RE.match(line)
            if not m:
                raise UEConfError(
                    "%s:%d: malformed line (grammar: KEY=VALUE, no spaces "
                    "around '='): %r" % (path, lineno, line)
                )
            key, value = m.group(1), m.group(2)
            if key not in KNOWN_KEYS:
                raise UEConfError("%s:%d: unknown key %r" % (path, lineno, key))
            if key in result:
                raise UEConfError("%s:%d: duplicate key %r" % (path, lineno, key))
            if not value.strip():
                raise UEConfError("%s:%d: empty value for %r" % (path, lineno, key))
            if "#" in value or "$" in value or '"' in value or "'" in value:
                raise UEConfError(
                    "%s:%d: forbidden character in value for %r "
                    "(no inline comments, '$', or quotes)" % (path, lineno, key)
                )
            result[key] = value.strip()
    return result


def resolve_conf(config_dir):
    """Key-level merge: ue_path.local.conf overrides ue_path.conf.

    Returns (values, files_used). Missing files are skipped; a present
    file with grammar errors raises UEConfError.
    """
    tracked = os.path.join(config_dir, "ue_path.conf")
    local = os.path.join(config_dir, "ue_path.local.conf")
    values = {}
    used = []
    if os.path.isfile(tracked):
        values.update(parse_conf_file(tracked))
        used.append(tracked)
    if os.path.isfile(local):
        values.update(parse_conf_file(local))
        used.append(local)
    return values, used


def normalize_engine_path(path):
    """Canonical comparable form: forward slashes, drive-letter case,
    MSYS /c/... -> C:/..., no trailing slash, lowercase."""
    if not path:
        return ""
    p = path.strip().replace("\\", "/")
    m = re.match(r"^/([A-Za-z])(/.*)?$", p)
    if m:
        p = m.group(1).upper() + ":" + (m.group(2) or "")
    p = p.rstrip("/")
    return p.lower()


def check_stale_env(resolved_ue_path, env=os.environ):
    """Return an error string if env UE_PATH exists and mismatches the
    resolved conf value; None when consistent or env unset."""
    env_val = env.get("UE_PATH")
    if not env_val or not resolved_ue_path:
        return None
    if normalize_engine_path(env_val) != normalize_engine_path(resolved_ue_path):
        return (
            "stale UE_PATH env (%s) does not match resolved conf value (%s) "
            "- rerun scripts/setup/setup_ue_env.ps1" % (env_val, resolved_ue_path)
        )
    return None
