"""Self-tests for validate_engine_env.py (positive + negative fixtures).

Run: python scripts/ue/check/governance/test/test_validate_engine_env.py
Exit code: 0 = all pass, 1 = failures.
"""
from __future__ import annotations

import json
import os
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
import validate_engine_env as vee  # noqa: E402

REPO_ROOT = os.path.abspath(os.path.join(HERE, "..", "..", "..", "..", ".."))

failures = []


def check(name, cond, detail=""):
    if cond:
        print("[OK] %s" % name)
    else:
        print("[FAIL] %s %s" % (name, detail))
        failures.append(name)


def make_repo(tmp):
    """Minimal git repo with conf + uproject + fake engine."""
    cfg = os.path.join(tmp, "scripts", "config")
    os.makedirs(cfg)
    shutil.copy(os.path.join(REPO_ROOT, "scripts", "config", "ue_conf.py"), cfg)
    eng = os.path.join(tmp, "eng")
    os.makedirs(os.path.join(eng, "Engine", "Build"))
    os.makedirs(os.path.join(eng, "Engine", "Binaries", "Win64"))
    with open(os.path.join(eng, "Engine", "Build", "Build.version"), "w") as fh:
        json.dump({
            "MajorVersion": 5,
            "MinorVersion": 8,
            "PatchVersion": 1,
            "Changelist": 56057345,
            "BranchName": "++UE5+Release-5.8",
        }, fh)
    open(os.path.join(eng, "Engine", "Binaries", "Win64",
                      "UnrealEditor-Cmd.exe"), "w").write("stub")
    with open(os.path.join(cfg, "ue_path.conf"), "w") as fh:
        root = eng.replace("\\", "/")
        fh.write("UE_PATH=%s\nUE_SOURCE_PATH=%s\n" % (root, root))
    with open(os.path.join(tmp, "Alis.uproject"), "w") as fh:
        json.dump({
            "EngineAssociation": "5.8",
            "Plugins": [
                {
                    "Name": "ModelContextProtocol",
                    "Enabled": True,
                    "TargetAllowList": ["Editor"],
                },
                {
                    "Name": "AllToolsets",
                    "Enabled": True,
                    "TargetAllowList": ["Editor"],
                },
            ],
        }, fh)
    manifest_dir = os.path.join(
        tmp, "Plugins", "Boot", "Orchestrator", "Data")
    os.makedirs(manifest_dir)
    with open(os.path.join(manifest_dir, "dev_manifest.json"), "w") as fh:
        json.dump({
            "engine_build_id": "++UE5+Release-5.8-CL-56057345",
        }, fh)
    os.makedirs(os.path.join(tmp, "Plugins", "Features", "TestPlugin"))
    with open(os.path.join(tmp, "Plugins", "Features", "TestPlugin",
                           "TestPlugin.uplugin"), "w") as fh:
        fh.write('{ "FriendlyName": "TestPlugin" }')
    subprocess.run(["git", "-C", tmp, "init", "-q"], check=True)
    subprocess.run(["git", "-C", tmp, "add", "-A"], check=True)
    subprocess.run(["git", "-C", tmp, "-c", "user.email=t@t",
                    "-c", "user.name=t", "commit", "-qm", "init"], check=True)
    return tmp, eng


def run_main(repo, extra=()):
    return vee.main(["--repo-root", repo] + list(extra))


def test_clean_repo_passes():
    tmp = tempfile.mkdtemp()
    try:
        repo, _ = make_repo(tmp)
        check("clean synthetic repo passes", run_main(repo) == 0)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def test_hardcoded_path_detected():
    tmp = tempfile.mkdtemp()
    try:
        repo, _ = make_repo(tmp)
        with open(os.path.join(repo, "bad_doc.md"), "w") as fh:
            fh.write("build with <ue-path>/Engine/Build/Build.bat\n")
        subprocess.run(["git", "-C", repo, "add", "-A"], check=True)
        check("hardcoded versioned path detected", run_main(repo) == 1)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def test_generated_slnx_path_detected():
    tmp = tempfile.mkdtemp()
    try:
        repo, _ = make_repo(tmp)
        with open(os.path.join(repo, "Alis.slnx"), "w") as fh:
            fh.write('<Project Path="<ue-path>/Engine/X.csproj" />\n')
        subprocess.run(["git", "-C", repo, "add", "-f", "Alis.slnx"],
                       check=True)
        check("tracked generated slnx path detected", run_main(repo) == 1)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def test_identity_mismatch_detected():
    tmp = tempfile.mkdtemp()
    try:
        repo, _ = make_repo(tmp)
        path = os.path.join(repo, "Alis.uproject")
        with open(path, encoding="utf-8") as fh:
            project = json.load(fh)
        project["EngineAssociation"] = "5.7"
        with open(path, "w") as fh:
            json.dump(project, fh)
        check("identity mismatch detected", run_main(repo) == 1)
        check("skip-identity skips the live check",
              run_main(repo, ["--skip-identity"]) == 0)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def test_source_identity_gate():
    tmp = tempfile.mkdtemp()
    try:
        repo, eng = make_repo(tmp)
        check("matching built source passes",
              run_main(repo, ["--require-source-identity"]) == 0)
        with open(os.path.join(eng, "Engine", "Build", "Build.version"), "w") as fh:
            json.dump({"MajorVersion": 5, "MinorVersion": 7, "PatchVersion": 4}, fh)
        check("stale source line fails",
              run_main(repo, ["--require-source-identity"]) == 1)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def test_dev_manifest_mismatch_detected():
    tmp = tempfile.mkdtemp()
    try:
        repo, _ = make_repo(tmp)
        manifest = os.path.join(
            repo, "Plugins", "Boot", "Orchestrator", "Data",
            "dev_manifest.json")
        with open(manifest, "w") as fh:
            json.dump({"engine_build_id": "++UE5+Release-5.7-CL-1"}, fh)
        check("dev manifest engine mismatch detected", run_main(repo) == 1)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def test_pin_invariant_detected():
    tmp = tempfile.mkdtemp()
    try:
        repo, _ = make_repo(tmp)
        pin = os.path.join(repo, "Plugins", "Features", "TestPlugin",
                           "TestPlugin.uplugin")
        with open(pin, "w") as fh:
            fh.write('{ "FriendlyName": "TestPlugin", "EngineVersion": "5.8.0" }')
        check("unallowlisted EngineVersion pin detected", run_main(repo) == 1)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def test_mcp_editor_boundary_detected():
    tmp = tempfile.mkdtemp()
    try:
        repo, _ = make_repo(tmp)
        path = os.path.join(repo, "Alis.uproject")
        with open(path, encoding="utf-8") as fh:
            project = json.load(fh)
        project["Plugins"][0]["TargetAllowList"] = ["Editor", "Game"]
        project["Plugins"][1].pop("TargetAllowList")
        with open(path, "w") as fh:
            json.dump(project, fh)
        problems = vee.check_mcp_editor_boundary(repo)
        check("both MCP Editor-only boundaries detected", len(problems) == 2)
        check("MCP boundary is wired into validator", run_main(repo) == 1)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def test_placeholders_are_clean():
    tmp = tempfile.mkdtemp()
    try:
        repo, _ = make_repo(tmp)
        with open(os.path.join(repo, "good_doc.md"), "w") as fh:
            fh.write("build with %UE_PATH%/Engine and package with "
                     "%UE_SOURCE_PATH% (resolve via scripts/config/ue_path.conf)\n")
        subprocess.run(["git", "-C", repo, "add", "-A"], check=True)
        check("placeholder docs pass", run_main(repo) == 0)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


if __name__ == "__main__":
    test_clean_repo_passes()
    test_hardcoded_path_detected()
    test_generated_slnx_path_detected()
    test_identity_mismatch_detected()
    test_source_identity_gate()
    test_dev_manifest_mismatch_detected()
    test_pin_invariant_detected()
    test_mcp_editor_boundary_detected()
    test_placeholders_are_clean()
    if failures:
        print("FAILED: %d" % len(failures))
        sys.exit(1)
    print("ALL PASS")
