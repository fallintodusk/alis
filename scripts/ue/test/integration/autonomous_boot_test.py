#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Autonomous Boot Flow Test with Crash Detection and Auto-Fix

This script:
  1. Runs standalone boot tests
  2. Captures full output
  3. Detects crashes (CrashReportClient.exe)
  4. Analyzes test results
  5. Reports success/failure with actionable details
"""

import os
import sys
import subprocess
import time
import re
from datetime import datetime
from pathlib import Path

# Fix encoding for Windows console
if sys.platform == 'win32':
    import codecs
    sys.stdout = codecs.getwriter('utf-8')(sys.stdout.buffer, 'ignore')
    sys.stderr = codecs.getwriter('utf-8')(sys.stderr.buffer, 'ignore')

# Configuration
SCRIPT_DIR = Path(__file__).parent.resolve()
PROJECT_ROOT = SCRIPT_DIR.parent.parent
LOG_DIR = PROJECT_ROOT / "Saved" / "AutomatedTests"
TIMESTAMP = datetime.now().strftime("%Y%m%d_%H%M%S")
LOG_FILE = LOG_DIR / f"boot_test_{TIMESTAMP}.log"

# Find UE path (SOT: local.conf > conf)
UE_PATH = Path("<ue-path>")
config_dir = PROJECT_ROOT / "scripts" / "config"
local_conf = config_dir / "ue_path.local.conf"
default_conf = config_dir / "ue_path.conf"
config_file = local_conf if local_conf.exists() else default_conf
if config_file.exists():
    with open(config_file) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            if line.startswith("UE_PATH") and ("=" in line or ":=" in line):
                if ":=" in line:
                    path_str = line.split(":=", 1)[1].strip().strip('"').strip("'")
                else:
                    path_str = line.split("=", 1)[1].strip().strip('"').strip("'")
                if not path_str.startswith("#"):
                    UE_PATH = Path(path_str)
                    break

PROJECT_FILE = PROJECT_ROOT / "Alis.uproject"
UE_EDITOR_CMD = UE_PATH / "Engine" / "Binaries" / "Win64" / "UnrealEditor-Cmd.exe"


def print_header(title):
    """Print a formatted header"""
    print("\n" + "=" * 60)
    print(f"  {title}")
    print("=" * 60)


def kill_processes():
    """Kill any existing UE processes"""
    for proc_name in ["UnrealEditor-Cmd.exe", "CrashReportClient.exe"]:
        try:
            subprocess.run(["taskkill", "/F", "/IM", proc_name],
                         capture_output=True, check=False)
        except Exception:
            pass
    time.sleep(1)


def clear_log_file():
    """Clear or backup the Alis.log file to get fresh test output"""
    ue_log_file = PROJECT_ROOT / "Saved" / "Logs" / "Alis.log"
    if ue_log_file.exists():
        backup_file = ue_log_file.with_suffix(f".log.backup_{TIMESTAMP}")
        try:
            import shutil
            shutil.move(str(ue_log_file), str(backup_file))
            print(f"  Backed up existing log to: {backup_file.name}")
        except Exception as e:
            print(f"  Warning: Could not backup log: {e}")
            try:
                ue_log_file.unlink()
                print("  Deleted existing log file")
            except Exception:
                pass


def check_crash():
    """Check if CrashReportClient is running"""
    try:
        result = subprocess.run(["tasklist", "/FI", "IMAGENAME eq CrashReportClient.exe"],
                              capture_output=True, text=True, check=False)
        return "CrashReportClient.exe" in result.stdout
    except Exception:
        return False


def run_tests():
    """Run the automation tests and capture output"""
    LOG_DIR.mkdir(parents=True, exist_ok=True)

    print_header("Running Boot Flow Tests")
    print(f"\nStart Time:  {datetime.now()}")
    print(f"UE Editor:   {UE_EDITOR_CMD}")
    print(f"Project:     {PROJECT_FILE}")
    print(f"Log File:    {LOG_FILE}")

    print("\n[STEP 1] Killing existing UE processes...")
    kill_processes()
    clear_log_file()

    print("[STEP 2] Running tests...")
    print()

    cmd = [
        str(UE_EDITOR_CMD),
        str(PROJECT_FILE),
        "-ExecCmds=Automation RunTests ProjectIntegrationTests.Standalone",
        "-unattended",
        "-nopause",
        "-NullRHI",
        "-log"
    ]

    try:
        with open(LOG_FILE, "w") as log:
            result = subprocess.run(cmd, stdout=log, stderr=subprocess.STDOUT,
                                  timeout=300, check=False)
        exit_code = result.returncode
    except subprocess.TimeoutExpired:
        print("[WARNING] Test timed out after 300 seconds")
        exit_code = -1
    except Exception as e:
        print(f"[ERROR] Error running test: {e}")
        return False

    print(f"\n[STEP 3] Test execution completed (Exit Code: {exit_code})")
    return True


def analyze_results():
    """Analyze test results and provide detailed feedback"""
    print("\n[STEP 4] Analyzing results...")
    print()

    # UE writes to its own log, not our captured log
    ue_log_file = PROJECT_ROOT / "Saved" / "Logs" / "Alis.log"

    if not ue_log_file.exists():
        print("[ERROR] Unreal log file not found!")
        print(f"Expected at: {ue_log_file}")
        return False

    # Read the last part of the log (tests are at the end)
    with open(ue_log_file, "r", encoding="utf-8", errors="ignore") as f:
        all_lines = f.readlines()
        # Get last 500 lines (should contain our test output)
        log_content = "".join(all_lines[-500:])

    # Check for crash
    crash_indicators = [
        "EXCEPTION_ACCESS_VIOLATION",
        "Fatal error!",
        "Unhandled Exception:",
        "Assertion failed:"
    ]

    has_crash = any(indicator in log_content for indicator in crash_indicators) or check_crash()

    if has_crash:
        print_header("CRASH DETECTED!")
        print("\nTest crashed during execution!")

        # Extract crash info from log
        for line in log_content.splitlines():
            if any(ind in line for ind in crash_indicators):
                print(f"  {line}")

        if check_crash():
            print("\nCrashReportClient.exe is running.")
            print("Killing crash reporter...")
            kill_processes()

        print("\nPlease check crash dumps in:")
        print(f"  {PROJECT_ROOT / 'Saved' / 'Crashes'}")
        print(f"\nLog file:")
        print(f"  {LOG_FILE}")
        return False

    # Check for test not found
    if "No automation tests matched" in log_content:
        print_header("TEST NOT FOUND")
        print("\nThe test 'ProjectIntegrationTests.Standalone' was not found.")
        print("\nPossible causes:")
        print("  1. Tests use wrong context (ClientContext requires standalone game)")
        print("  2. Plugin not enabled or not built")
        print("  3. Module dependencies missing")
        print("\nFix: Change EAutomationTestFlags::ClientContext to EditorContext")
        return False

    # Check for config not loaded
    if "BootMap is empty" in log_content:
        print_header("CONFIG NOT LOADED")
        print("\nBootMap is empty - config file not loaded.")
        print("\nPossible causes:")
        print("  1. UCLASS has wrong config specifier (should be config=ProjectBoot)")
        print("  2. Config file missing: Config/DefaultProjectBoot.ini")
        print("  3. Wrong section name in config")

        config_file = PROJECT_ROOT / "Config" / "DefaultProjectBoot.ini"
        print("\nChecking config file...")
        if config_file.exists():
            print("  [OK] Config file exists")
            print("\nContents:")
            print(config_file.read_text())
        else:
            print("  [ERROR] Config file NOT found!")
        return False

    # Check for wrong config specifier
    if "still set to engine default" in log_content:
        print_header("CONFIG SPECIFIER WRONG")
        print("\nConfig file not being read - UCLASS specifier is wrong.")
        print("\nFix required in:")
        print("  Plugins/Core/ProjectBoot/Source/ProjectBoot/Public/ProjectBootSettings.h")
        print("\nChange:")
        print("  UCLASS(config=Game, ...)  // WRONG")
        print("To:")
        print("  UCLASS(config=ProjectBoot, ...)  // CORRECT")
        return False

    # Check for success
    if "BootMap configured correctly" in log_content and "ProjectBootSubsystem exists" in log_content:
        print_header("ALL TESTS PASSED!")
        print("\nBoot flow validation successful:")
        print()

        # Extract key metrics
        for pattern in [
            r"BootMap Package: (.+)",
            r"BootFlowControllerClass: (.+)",
            r"IsBootWorld: (.+)"
        ]:
            match = re.search(pattern, log_content)
            if match:
                print(f"  {match.group(0)}")

        print(f"\nFull log: {LOG_FILE}")
        return True

    # Generic failure
    print_header("TEST RESULTS UNCLEAR")
    print("\nCould not determine test status from output.")
    print(f"\nPlease review the log file:")
    print(f"  {LOG_FILE}")
    print("\nLast 50 lines:")
    print()
    lines = log_content.splitlines()
    for line in lines[-50:]:
        print(f"  {line}")

    return False


def main():
    """Main entry point"""
    print_header("Autonomous Boot Flow Test")

    if not UE_EDITOR_CMD.exists():
        print(f"\n[ERROR] UnrealEditor-Cmd.exe not found at:")
        print(f"  {UE_EDITOR_CMD}")
        print("\nPlease check your UE installation path.")
        return 1

    if not PROJECT_FILE.exists():
        print(f"\n[ERROR] Project file not found at:")
        print(f"  {PROJECT_FILE}")
        return 1

    # Run tests
    if not run_tests():
        print_header("STATUS: FAILED [X]")
        print(f"\nEnd Time: {datetime.now()}\n")
        return 1

    # Analyze results
    success = analyze_results()

    if success:
        print_header("STATUS: SUCCESS [PASS]")
        print(f"\nEnd Time: {datetime.now()}\n")
        return 0
    else:
        print_header("STATUS: FAILED [X]")
        print(f"\nEnd Time: {datetime.now()}\n")
        return 1


if __name__ == "__main__":
    sys.exit(main())
