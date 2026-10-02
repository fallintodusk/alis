"""Engine-environment governance validator.

Four jobs:
  A. Engine identity: resolved UE_PATH is a real engine root whose
     Build.version Major.Minor matches Alis.uproject EngineAssociation.
  B. Hardcoded-path governance: no tracked text file, whatever its type,
     may contain a versioned engine path (UnrealEngine-5.x / UE_5.x) or a
     machine-local path literal (repo roots, tool installs, user homes - as
     defined by machine_local_paths.py, which the public mirror applies too),
     each outside its reason list. Machine-local values belong to the user
     settings SOT. Also asserts the conf grammar (one line per key) via the
     shared strict parser.
  C. Unreal MCP boundary: the official server and toolsets remain enabled
     only for Editor targets.
  D. Tracked Codex config: the repo's own .codex/config.toml may not pin
     an engine version - it forwards UE_EDITOR_CMD from the environment.
     Deliberately does NOT police the user-global ~/.codex/config.toml:
     other projects legitimately target other engines there, and making
     ALIS validation fail over them is the same cross-project coupling
     that was removed from the mutation path.

Wired into validate_all.bat like the other governance checks.
Self-tests: scripts/ue/check/governance/test/test_validate_engine_env.py

Usage:
  python validate_engine_env.py [--repo-root <path>] [--skip-identity]
  python validate_engine_env.py --paths-only   # job B only (public CI)
Exit codes: 0 = pass, 1 = violations, 2 = usage/internal error.
"""
from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys

import machine_local_paths

ENGINE_PATH_RE = re.compile(r"UnrealEngine-\d+\.\d+|UE_\d+\.\d+")

# Reason-based allowlists, forward slashes. An entry ending in "/" covers its
# directory; any other entry covers exactly one file, so a template or backup
# beside an exempt file is still checked.
ALLOWLIST = (
    # The SOT itself
    "scripts/config/ue_path.conf",
    # Test fixtures encode versioned paths on purpose
    "scripts/config/test/fixtures/",
    # This validator + its tests name the patterns they hunt
    "scripts/ue/check/governance/validate_engine_env.py",
    "scripts/ue/check/governance/test/",
    # Conformance/test suites exercise versioned-path handling on purpose
    "scripts/config/test/",
    "scripts/setup/test/",
    "scripts/ue/update/test/",
    # Engine-root REGEX classes (not literal paths) live in the writer
    "scripts/setup/UEEnvSync.psm1",
    # Rust conformance tests + fixture-driven engine_config crate
    "tools/BuildService/crates/engine_config/",
)

# Reason-based exemptions from the machine-local literal check:
MACHINE_PATH_ALLOWLIST = (
    # Third-party trees keep their upstream text
    "Plugins/ThirdParty/",
    "Plugins/InstanceArrayTool/",
    # The machine-local settings SOT itself (never published)
    "scripts/config/ue_path.conf",
    # Agent adapter symlinks: their link target stays machine-bound by
    # operator decision
    "CLAUDE.md",
    "CODEX.md",
)

# Skipped without reading; anything else is sniffed for NUL bytes, so no
# text type can slip past the scan by its extension.
BINARY_EXTS = {
    ".uasset", ".umap", ".ubulk", ".uexp", ".png", ".jpg", ".jpeg", ".tga",
    ".bmp", ".exr", ".hdr", ".ico", ".psd", ".wav", ".mp3", ".ogg", ".fbx",
    ".ttf", ".otf", ".woff", ".woff2", ".dll", ".exe", ".pdb", ".lib", ".zip",
    ".mp4", ".mov", ".bin",
}


def repo_files(repo_root):
    out = subprocess.run(
        ["git", "-C", repo_root, "ls-files"],
        capture_output=True, text=True, check=True,
    ).stdout
    return [l for l in out.splitlines() if l]


def matches_entry(path, entries):
    return any(
        path.startswith(entry) if entry.endswith("/") else path == entry
        for entry in entries
    )


def is_allowlisted(path):
    return matches_entry(path, ALLOWLIST)


def is_machine_path_exempt(path):
    return matches_entry(path, MACHINE_PATH_ALLOWLIST) or "/ThirdParty/" in path


def read_text_file(full):
    """File text, or None for binary content and unreadable files."""
    try:
        with open(full, "rb") as fh:
            raw = fh.read()
    except OSError:
        return None
    if b"\0" in raw[:8192]:
        return None
    return raw.decode("utf-8", errors="ignore")


def check_hardcoded_paths(repo_root):
    violations = []
    for rel in repo_files(repo_root):
        if os.path.splitext(rel)[1].lower() in BINARY_EXTS:
            continue
        text = read_text_file(os.path.join(repo_root, rel))
        if text is None:
            continue
        lines = text.splitlines()
        if not is_allowlisted(rel):
            for lineno, line in enumerate(lines, 1):
                if ENGINE_PATH_RE.search(line):
                    violations.append(
                        "%s:%d: hardcoded versioned engine path: %s"
                        % (rel, lineno, line.strip()[:100])
                    )
        if not is_machine_path_exempt(rel):
            for lineno in machine_local_paths.machine_path_lines(text):
                violations.append(
                    "%s:%d: machine-local path (resolve it at runtime; see "
                    "docs/architecture/principles.md): %s"
                    % (rel, lineno, lines[lineno - 1].strip()[:100])
                )
    return violations


def check_conf_grammar(repo_root):
    config_dir = os.path.join(repo_root, "scripts", "config")
    sys.path.insert(0, config_dir)
    try:
        import ue_conf
        values, _files = ue_conf.resolve_conf(config_dir)
        return [], values
    except Exception as exc:
        return ["conf grammar error: %s" % exc], {}
    finally:
        sys.path.pop(0)


def build_id_from_version(build_version):
    branch = str(build_version.get("BranchName", ""))
    changelist = int(build_version.get("Changelist", 0))
    if branch:
        return "%s-CL-%d" % (branch, changelist)
    return "UE%s.%s-CL-%d" % (
        build_version["MajorVersion"], build_version["MinorVersion"],
        changelist,
    )


def engine_line_from_build_id(build_id):
    match = re.search(r"(?:Release-|^)(\d+)\.(\d+)(?:[.-]|$)", str(build_id))
    return "%s.%s" % match.groups() if match else None


def check_engine_identity(repo_root, values):
    problems = []
    ue_path = values.get("UE_PATH")
    if not ue_path:
        problems.append("conf declares no UE_PATH")
        return problems
    bv_path = os.path.join(ue_path, "Engine", "Build", "Build.version")
    if not os.path.isfile(bv_path):
        problems.append(
            "UE_PATH is not an engine root (no Engine/Build/Build.version): %s"
            % ue_path
        )
        return problems
    with open(bv_path, encoding="utf-8") as fh:
        bv = json.load(fh)
    line = "%s.%s" % (bv["MajorVersion"], bv["MinorVersion"])
    uproject = os.path.join(repo_root, "Alis.uproject")
    with open(uproject, encoding="utf-8") as fh:
        m = re.search(r'"EngineAssociation"\s*:\s*"([^"]*)"', fh.read())
    assoc = m.group(1) if m else None
    if assoc != line:
        problems.append(
            "engine identity mismatch: UE_PATH Build.version line %s but "
            "Alis.uproject EngineAssociation is %s - run "
            "scripts/ue/update/update_engine.ps1 instead of editing by hand"
            % (line, assoc)
        )
    manifest_path = os.path.join(
        repo_root, "Plugins", "Boot", "Orchestrator", "Data",
        "dev_manifest.json",
    )
    try:
        with open(manifest_path, encoding="utf-8") as fh:
            manifest_build_id = json.load(fh).get("engine_build_id")
        expected_line = "%s.%s" % (bv["MajorVersion"], bv["MinorVersion"])
        manifest_line = engine_line_from_build_id(manifest_build_id)
        if manifest_line != expected_line:
            problems.append(
                "dev manifest engine line mismatch: expected %s from UE_PATH "
                "Build.version but found %s - run "
                "scripts/ue/update/update_engine.ps1"
                % (expected_line, manifest_build_id)
            )
    except (OSError, ValueError, TypeError, json.JSONDecodeError) as exc:
        problems.append("invalid dev manifest engine pin: %s" % exc)
    editor = os.path.join(ue_path, "Engine", "Binaries", "Win64",
                          "UnrealEditor-Cmd.exe")
    if not os.path.isfile(editor):
        problems.append("UE_PATH has no UnrealEditor-Cmd.exe: %s" % ue_path)
    return problems


def check_source_engine_identity(repo_root, values):
    problems = []
    source_path = values.get("UE_SOURCE_PATH")
    if not source_path:
        return ["source release FROZEN: conf declares no UE_SOURCE_PATH"]
    bv_path = os.path.join(source_path, "Engine", "Build", "Build.version")
    if not os.path.isfile(bv_path):
        return ["source release FROZEN: invalid UE_SOURCE_PATH: %s" % source_path]
    with open(bv_path, encoding="utf-8") as fh:
        bv = json.load(fh)
    line = "%s.%s" % (bv["MajorVersion"], bv["MinorVersion"])
    with open(os.path.join(repo_root, "Alis.uproject"), encoding="utf-8") as fh:
        match = re.search(r'"EngineAssociation"\s*:\s*"([^"]*)"', fh.read())
    association = match.group(1) if match else None
    if association != line:
        problems.append(
            "source release FROZEN: UE_SOURCE_PATH line %s does not match "
            "Alis.uproject EngineAssociation %s" % (line, association)
        )
    if os.path.isfile(os.path.join(source_path, "Engine", "Build",
                                   "InstalledBuild.txt")):
        problems.append("source release FROZEN: UE_SOURCE_PATH is an installed engine")
    editor = os.path.join(source_path, "Engine", "Binaries", "Win64",
                          "UnrealEditor-Cmd.exe")
    if not os.path.isfile(editor):
        problems.append("source release FROZEN: source editor is not built: %s" % editor)
    return problems


def check_plugin_pins(repo_root):
    """Conditional preservation invariant: project-owned source plugins
    embedded only in ALIS must not carry an EngineVersion pin unless an
    explicit documented reason exists (narrow allowlist below)."""
    pin_allowlist = ()
    problems = []
    plugins_dir = os.path.join(repo_root, "Plugins")
    for root, _dirs, files in os.walk(plugins_dir):
        for name in files:
            if not name.endswith(".uplugin"):
                continue
            full = os.path.join(root, name)
            rel = os.path.relpath(full, repo_root).replace("\\", "/")
            with open(full, encoding="utf-8", errors="ignore") as fh:
                if re.search(r'"EngineVersion"\s*:', fh.read()):
                    if rel not in pin_allowlist:
                        problems.append(
                            "%s: carries an EngineVersion pin without a "
                            "documented allowlist reason (see "
                            "validate_engine_env.py)" % rel
                        )
    return problems


def check_mcp_editor_boundary(repo_root):
    problems = []
    uproject = os.path.join(repo_root, "Alis.uproject")
    try:
        with open(uproject, encoding="utf-8") as fh:
            project = json.load(fh)
    except (OSError, json.JSONDecodeError) as exc:
        return ["cannot validate Unreal MCP project boundary: %s" % exc]

    plugins = {
        entry.get("Name"): entry for entry in project.get("Plugins", [])
    }
    for name in ("ModelContextProtocol", "AllToolsets"):
        entry = plugins.get(name)
        if not entry or entry.get("Enabled") is not True:
            problems.append("Alis.uproject: %s must be enabled" % name)
            continue
        if entry.get("TargetAllowList") != ["Editor"]:
            problems.append(
                "Alis.uproject: %s TargetAllowList must equal [Editor]" % name
            )
    return problems


def check_metahuman_authoring_boundary(repo_root):
    """Keep MetaHuman authoring and capture plugins out of game targets."""
    problems = []
    uproject = os.path.join(repo_root, "Alis.uproject")
    try:
        with open(uproject, encoding="utf-8") as fh:
            project = json.load(fh)
    except (OSError, json.JSONDecodeError) as exc:
        return ["cannot validate MetaHuman target boundary: %s" % exc]

    plugins = {
        entry.get("Name"): entry for entry in project.get("Plugins", [])
    }
    for name in ("MetaHumanCharacter", "MetaHumanLiveLink"):
        entry = plugins.get(name)
        if not entry or entry.get("Enabled") is not True:
            continue
        if entry.get("TargetAllowList") != ["Editor"]:
            problems.append(
                "Alis.uproject: %s TargetAllowList must equal [Editor]" % name
            )
    return problems


def check_repo_codex_config(repo_root):
    """The tracked project Codex config must carry NO engine version.

    It is committed, so a version there would be wrong on every other machine
    and on every engine upgrade. The environment supplies UE_EDITOR_CMD.
    """
    path = os.path.join(repo_root, ".codex", "config.toml")
    if not os.path.isfile(path):
        return []
    try:
        with open(path, encoding="utf-8", errors="ignore") as fh:
            text = fh.read()
    except OSError as exc:
        return ["cannot read .codex/config.toml: %s" % exc]

    found = sorted(set(ENGINE_PATH_RE.findall(text)))
    return [
        ".codex/config.toml contains versioned engine path '%s' - the tracked "
        "project config must forward UE_EDITOR_CMD from the environment "
        "instead of pinning a version" % f
        for f in found
    ]


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", default=".")
    parser.add_argument("--skip-identity", action="store_true",
                        help="skip live engine checks (CI without engines)")
    parser.add_argument("--require-source-identity", action="store_true",
                        help="require a built matching source engine")
    parser.add_argument("--paths-only", action="store_true",
                        help="run only the hardcoded-path job (public CI has "
                             "no engine and no private project state)")
    args = parser.parse_args(argv)
    repo_root = os.path.abspath(args.repo_root)

    if args.paths_only:
        violations = check_hardcoded_paths(repo_root)
        if violations:
            print("[validate_engine_env] FAILED: %d violation(s)" % len(violations))
            for v in violations:
                print("  " + v)
            return 1
        print("[validate_engine_env] OK (hardcoded-path invariants)")
        return 0

    violations = []
    grammar_problems, values = check_conf_grammar(repo_root)
    violations += grammar_problems
    if not args.skip_identity and not grammar_problems:
        violations += check_engine_identity(repo_root, values)
        if args.require_source_identity:
            violations += check_source_engine_identity(repo_root, values)
    violations += check_hardcoded_paths(repo_root)
    violations += check_plugin_pins(repo_root)
    violations += check_mcp_editor_boundary(repo_root)
    violations += check_metahuman_authoring_boundary(repo_root)
    violations += check_repo_codex_config(repo_root)

    if violations:
        print("[validate_engine_env] FAILED: %d violation(s)" % len(violations))
        for v in violations:
            print("  " + v)
        return 1
    print("[validate_engine_env] OK (identity + path + plugin + MCP invariants)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
