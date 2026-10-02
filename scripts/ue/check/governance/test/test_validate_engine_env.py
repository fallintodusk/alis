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

# Machine-local samples are assembled at runtime so this file itself carries
# no literal for the job under test to find.
REPO_LITERAL = "E" + ":/Repos_Alis/Alis"
USER_LITERAL = "C" + ":" + "\\" + "Users" + "\\" + "someone" + "\\" + "tools"

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
                {
                    "Name": "MetaHumanCharacter",
                    "Enabled": True,
                    "TargetAllowList": ["Editor"],
                },
                {
                    "Name": "MetaHumanCoreTech",
                    "Enabled": True,
                    "SupportedTargetPlatforms": ["Win64", "Linux"],
                },
                {
                    "Name": "MetaHumanLiveLink",
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
            fh.write("build with X:/Engines/UnrealEngine-5.7/Engine/Build/Build.bat\n")
        subprocess.run(["git", "-C", repo, "add", "-A"], check=True)
        check("hardcoded versioned path detected", run_main(repo) == 1)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def test_generated_slnx_path_detected():
    tmp = tempfile.mkdtemp()
    try:
        repo, _ = make_repo(tmp)
        with open(os.path.join(repo, "Alis.slnx"), "w") as fh:
            fh.write('<Project Path="X:/Engines/UE_5.8/Engine/X.csproj" />\n')
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
        repo, eng = make_repo(tmp)
        manifest = os.path.join(
            repo, "Plugins", "Boot", "Orchestrator", "Data",
            "dev_manifest.json")
        with open(os.path.join(eng, "Engine", "Build", "Build.version"), "w") as fh:
            json.dump({
                "MajorVersion": 5, "MinorVersion": 8, "PatchVersion": 3,
                "Changelist": 58210709, "BranchName": "++UE5+Release-5.8",
            }, fh)
        check("same engine line with a different changelist passes", run_main(repo) == 0)
        with open(manifest, "w") as fh:
            json.dump({"engine_build_id": "++UE5+Release-5.7-CL-1"}, fh)
        check("dev manifest engine line mismatch detected", run_main(repo) == 1)
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


def test_metahuman_authoring_boundary_detected():
    tmp = tempfile.mkdtemp()
    try:
        repo, _ = make_repo(tmp)
        path = os.path.join(repo, "Alis.uproject")
        with open(path, encoding="utf-8") as fh:
            project = json.load(fh)
        plugins = {entry["Name"]: entry for entry in project["Plugins"]}
        plugins["MetaHumanCharacter"]["TargetAllowList"] = ["Editor", "Game"]
        plugins["MetaHumanLiveLink"].pop("TargetAllowList")
        with open(path, "w") as fh:
            json.dump(project, fh)
        problems = vee.check_metahuman_authoring_boundary(repo)
        check("both MetaHuman Shipping boundaries detected", len(problems) == 2)
        check("MetaHuman boundary is wired into validator", run_main(repo) == 1)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def test_metahuman_authoring_boundary_allows_absent_plugins():
    tmp = tempfile.mkdtemp()
    try:
        repo, _ = make_repo(tmp)
        path = os.path.join(repo, "Alis.uproject")
        with open(path, encoding="utf-8") as fh:
            project = json.load(fh)
        project["Plugins"] = [
            entry for entry in project["Plugins"]
            if entry["Name"] not in ("MetaHumanCharacter", "MetaHumanLiveLink")
        ]
        with open(path, "w") as fh:
            json.dump(project, fh)
        problems = vee.check_metahuman_authoring_boundary(repo)
        check("absent MetaHuman authoring plugins are accepted", not problems)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def test_placeholders_are_clean():
    tmp = tempfile.mkdtemp()
    try:
        repo, _ = make_repo(tmp)
        with open(os.path.join(repo, "good_doc.md"), "w") as fh:
            fh.write("build with %UE_PATH%/Engine and package with "
                     "%UE_SOURCE_PATH% (resolve via scripts/config/ue_path.conf); "
                     "run <repo>/scripts/x.py, cache in %TEMP%\\x and $HOME/.cargo\n")
        subprocess.run(["git", "-C", repo, "add", "-A"], check=True)
        check("placeholder docs pass", run_main(repo) == 0)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def test_machine_local_path_detected():
    tmp = tempfile.mkdtemp()
    try:
        repo, _ = make_repo(tmp)
        with open(os.path.join(repo, "run.bat"), "w") as fh:
            fh.write("set PROJ=%s/Alis.uproject\n" % REPO_LITERAL)
        subprocess.run(["git", "-C", repo, "add", "-A"], check=True)
        check("machine-local path in a script detected", run_main(repo) == 1)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def test_machine_local_path_in_any_text_type_detected():
    tmp = tempfile.mkdtemp()
    try:
        repo, _ = make_repo(tmp)
        os.makedirs(os.path.join(repo, "hooks"))
        with open(os.path.join(repo, "hooks", "post-merge"), "w") as fh:
            fh.write("cd %s\n" % USER_LITERAL)
        subprocess.run(["git", "-C", repo, "add", "-A"], check=True)
        check("machine-local path in an extensionless file detected",
              run_main(repo, ["--paths-only"]) == 1)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def test_machine_local_path_reasons_are_exempt():
    tmp = tempfile.mkdtemp()
    try:
        repo, _ = make_repo(tmp)
        upstream = os.path.join(repo, "Plugins", "ThirdParty", "Lib")
        os.makedirs(upstream)
        with open(os.path.join(upstream, "README.md"), "w") as fh:
            fh.write("built in %s\n" % REPO_LITERAL)
        subprocess.run(["git", "-C", repo, "add", "-A"], check=True)
        check("third-party upstream text is exempt",
              run_main(repo, ["--paths-only"]) == 0)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def test_exemptions_name_exact_files():
    # A file entry exempts that file only: the SOT conf stays exempt, while
    # the template beside it is checked by both path families.
    for kind, line in (
        ("machine-local", "# UE_PATH=%s/Engine\n" % REPO_LITERAL),
        ("versioned engine", "# UE_PATH=X:/Engines/UnrealEngine-5.7\n"),
    ):
        tmp = tempfile.mkdtemp()
        try:
            repo, _ = make_repo(tmp)
            cfg = os.path.join(repo, "scripts", "config")
            with open(os.path.join(cfg, "ue_path.conf"), "a") as fh:
                fh.write(line)
            subprocess.run(["git", "-C", repo, "add", "-A"], check=True)
            check("%s path in the SOT conf stays exempt" % kind,
                  run_main(repo, ["--paths-only"]) == 0)
            with open(os.path.join(cfg, "ue_path.conf.example"), "w") as fh:
                fh.write(line)
            subprocess.run(["git", "-C", repo, "add", "-A"], check=True)
            check("%s path in ue_path.conf.example detected" % kind,
                  run_main(repo, ["--paths-only"]) == 1)
        finally:
            shutil.rmtree(tmp, ignore_errors=True)


def test_task_files_are_checked():
    # Fresh agents execute task files, so no todo folder is exempt.
    lines = (
        ("machine-local", "run %s/scripts/x.bat\n" % REPO_LITERAL),
        ("versioned engine", "use X:/Engines/UnrealEngine-5.7/Engine\n"),
    )
    for folder in ("00_current", "01_done", "02_backlog", "03_parked", "04_cancelled"):
        for kind, line in lines:
            tmp = tempfile.mkdtemp()
            try:
                repo, _ = make_repo(tmp)
                os.makedirs(os.path.join(repo, "todo", folder))
                with open(os.path.join(repo, "todo", folder, "task.md"), "w") as fh:
                    fh.write(line)
                subprocess.run(["git", "-C", repo, "add", "-A"], check=True)
                check("%s path in todo/%s detected" % (kind, folder),
                      run_main(repo, ["--paths-only"]) == 1)
            finally:
                shutil.rmtree(tmp, ignore_errors=True)


def test_paths_only_needs_no_private_state():
    tmp = tempfile.mkdtemp()
    try:
        repo, _ = make_repo(tmp)
        os.remove(os.path.join(repo, "Alis.uproject"))
        os.remove(os.path.join(repo, "scripts", "config", "ue_path.conf"))
        subprocess.run(["git", "-C", repo, "add", "-A"], check=True)
        check("paths-only passes without project or engine state",
              run_main(repo, ["--paths-only"]) == 0)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def test_repo_codex_config_rejects_versioned_path():
    """Job D: the TRACKED project Codex config may not pin an engine version.

    It is committed, so a version there is wrong on every other machine and on
    every engine upgrade; UE_EDITOR_CMD arrives from the environment via
    env_vars instead.

    Deliberately NOT tested: policing the user-global ~/.codex/config.toml.
    Other projects legitimately target other engines there, and failing ALIS
    validation over them is cross-project coupling.
    """
    tmp = tempfile.mkdtemp()
    os.makedirs(os.path.join(tmp, ".codex"))
    cfg = os.path.join(tmp, ".codex", "config.toml")
    try:
        with open(cfg, "w", encoding="utf-8") as fh:
            fh.write('UE_EDITOR_CMD = "X:/Engines/UE_5.8/x.exe"')
        check("tracked codex versioned path rejected",
              len(vee.check_repo_codex_config(tmp)) == 1)

        with open(cfg, "w", encoding="utf-8") as fh:
            fh.write('env_vars = ["UE_EDITOR_CMD"]')
        check("env forwarding passes",
              vee.check_repo_codex_config(tmp) == [])

        os.remove(cfg)
        check("absent tracked codex config skips",
              vee.check_repo_codex_config(tmp) == [])
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
    test_metahuman_authoring_boundary_detected()
    test_metahuman_authoring_boundary_allows_absent_plugins()
    test_placeholders_are_clean()
    test_machine_local_path_detected()
    test_machine_local_path_in_any_text_type_detected()
    test_machine_local_path_reasons_are_exempt()
    test_exemptions_name_exact_files()
    test_task_files_are_checked()
    test_paths_only_needs_no_private_state()
    test_repo_codex_config_rejects_versioned_path()
    if failures:
        print("FAILED: %d" % len(failures))
        sys.exit(1)
    print("ALL PASS")
