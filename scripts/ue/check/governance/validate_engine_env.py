"""Engine-environment governance validator.

Three jobs:
  A. Engine identity: resolved UE_PATH is a real engine root whose
     Build.version Major.Minor matches Alis.uproject EngineAssociation.
  B. Hardcoded-path governance: no tracked text file outside the
     allowlist may contain a versioned engine path (UnrealEngine-5.x /
     UE_5.x). Also asserts the conf grammar (one line per key) via the
     shared strict parser.
  C. Unreal MCP boundary: the official server and toolsets remain enabled
     only for Editor targets.

Wired into validate_all.bat like the other governance checks.
Self-tests: scripts/ue/check/governance/test/test_validate_engine_env.py

Usage:
  python validate_engine_env.py [--repo-root <path>] [--skip-identity]
Exit codes: 0 = pass, 1 = violations, 2 = usage/internal error.
"""
from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys

ENGINE_PATH_RE = re.compile(r"UnrealEngine-\d+\.\d+|UE_\d+\.\d+")

# Reason-based allowlist (path prefixes or exact files, forward slashes):
ALLOWLIST = (
    # The SOT itself + its machine-local sibling and examples
    "scripts/config/ue_path.conf",
    # Transient execution artifacts (explicitly out of governance scope)
    "todo/",
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

TEXT_EXTS = {
    ".md", ".txt", ".ps1", ".psm1", ".bat", ".sh", ".py", ".mk", ".conf",
    ".cs", ".cpp", ".h", ".rs", ".toml", ".json", ".yaml", ".yml", ".ini",
    ".uproject", ".uplugin", ".slnx", ".dsl",
}


def repo_files(repo_root):
    out = subprocess.run(
        ["git", "-C", repo_root, "ls-files"],
        capture_output=True, text=True, check=True,
    ).stdout
    return [l for l in out.splitlines() if l]


def is_allowlisted(path):
    return any(path == a or path.startswith(a) for a in ALLOWLIST)


def check_hardcoded_paths(repo_root):
    violations = []
    for rel in repo_files(repo_root):
        ext = os.path.splitext(rel)[1].lower()
        if ext not in TEXT_EXTS:
            continue
        if is_allowlisted(rel):
            continue
        full = os.path.join(repo_root, rel)
        try:
            with open(full, "r", encoding="utf-8", errors="ignore") as fh:
                for lineno, line in enumerate(fh, 1):
                    if ENGINE_PATH_RE.search(line):
                        violations.append(
                            "%s:%d: hardcoded versioned engine path: %s"
                            % (rel, lineno, line.strip()[:100])
                        )
        except OSError:
            continue
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
        expected_build_id = build_id_from_version(bv)
        if manifest_build_id != expected_build_id:
            problems.append(
                "dev manifest engine mismatch: expected %s from UE_PATH "
                "Build.version but found %s - run "
                "scripts/ue/update/update_engine.ps1"
                % (expected_build_id, manifest_build_id)
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


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", default=".")
    parser.add_argument("--skip-identity", action="store_true",
                        help="skip live engine checks (CI without engines)")
    parser.add_argument("--require-source-identity", action="store_true",
                        help="require a built matching source engine")
    args = parser.parse_args(argv)
    repo_root = os.path.abspath(args.repo_root)

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

    if violations:
        print("[validate_engine_env] FAILED: %d violation(s)" % len(violations))
        for v in violations:
            print("  " + v)
        return 1
    print("[validate_engine_env] OK (identity + path + plugin + MCP invariants)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
