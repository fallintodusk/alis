#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Autonomous Standalone Game Test with Auto-Quit

Runs the game in standalone mode (not PIE), auto-quits after N seconds,
and validates boot flow + UI rendering without font errors.

Uses UE's incremental build by default (fast). Clean only when explicitly requested.

Usage:
    python scripts/ue/standalone/test.py [--timeout SECONDS] [--no-build] [--clean]
    python scripts/ue/standalone/test.py                      # Incremental build + test (default: 30 sec, FAST)
    python scripts/ue/standalone/test.py --timeout 45         # Incremental build + 45 sec test
    python scripts/ue/standalone/test.py --no-build           # Skip build entirely (use existing binaries, 30 sec)
    python scripts/ue/standalone/test.py --clean              # Clean + full rebuild + test (slow, use when needed)
"""

import argparse
import os
import shutil
import sys
import subprocess
import time
from datetime import datetime
from pathlib import Path

# Fix encoding for Windows console
if sys.platform == 'win32':
    import codecs
    sys.stdout = codecs.getwriter('utf-8')(sys.stdout.buffer, 'ignore')
    sys.stderr = codecs.getwriter('utf-8')(sys.stderr.buffer, 'ignore')

# Configuration
SCRIPT_DIR = Path(__file__).parent.resolve()
PROJECT_ROOT = SCRIPT_DIR.parent.parent.parent
TIMESTAMP = datetime.now().strftime("%Y%m%d_%H%M%S")
TEST_LOG_DIR = PROJECT_ROOT / "Saved" / "Logs"
REPORT_DIR = PROJECT_ROOT / "Saved" / "AutomatedTests"
REPORT_FILE = REPORT_DIR / f"standalone_test_{TIMESTAMP}.txt"

# Find UE path from config (SOT: local.conf > conf)
UE_PATH = None
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

if UE_PATH is None:
    print("[ERROR] UE_PATH not set. Create scripts/config/ue_path.local.conf.")
    sys.exit(1)

PROJECT_FILE = PROJECT_ROOT / "Alis.uproject"
UE_EDITOR = UE_PATH / "Engine" / "Binaries" / "Win64" / "UnrealEditor.exe"
GAME_EXE = UE_EDITOR  # Same executable, runs with -game flag for standalone mode


def print_header(title):
    """Print formatted header"""
    print("\n" + "=" * 70)
    print(f"  {title}")
    print("=" * 70)


def kill_processes():
    """Kill any existing game and editor processes"""
    for proc_name in ["Alis.exe", "Alis-Win64-Development.exe", "Alis-Win64-DebugGame.exe",
                     "UnrealEditor.exe", "UnrealEditor-Cmd.exe", "CrashReportClient.exe"]:
        try:
            subprocess.run(["taskkill", "/F", "/IM", proc_name],
                         capture_output=True, check=False, timeout=5)
        except Exception:
            pass
    time.sleep(2)


def clean_binaries():
    """Clean project and plugin Binaries/Intermediate using PowerShell scripts"""
    print_header("Cleaning Binaries/Intermediate")

    # Use PowerShell clean scripts (SOLID structure)
    clean_plugins_ps1 = PROJECT_ROOT / "scripts" / "ue" / "clean" / "clean_plugins.ps1"
    clean_project_ps1 = PROJECT_ROOT / "scripts" / "ue" / "clean" / "clean_project.ps1"

    try:
        # Clean plugins first
        print("   Cleaning all plugins...")
        result_plugins = subprocess.run(
            ["powershell.exe", "-ExecutionPolicy", "Bypass", "-File", str(clean_plugins_ps1)],
            capture_output=True,
            text=True,
            timeout=30
        )
        if result_plugins.returncode == 0:
            print("   Plugins cleaned successfully")
        else:
            print(f"   [WARNING] Plugin clean failed: {result_plugins.stderr}")

        # Clean project
        print("   Cleaning project artifacts...")
        result_project = subprocess.run(
            ["powershell.exe", "-ExecutionPolicy", "Bypass", "-File", str(clean_project_ps1)],
            capture_output=True,
            text=True,
            timeout=30
        )
        if result_project.returncode == 0:
            print("   Project cleaned successfully")
        else:
            print(f"   [WARNING] Project clean failed: {result_project.stderr}")

        return True

    except subprocess.TimeoutExpired:
        print("   [ERROR] Clean timed out after 30 seconds")
        return False
    except Exception as e:
        print(f"   [ERROR] Clean failed: {e}")
        return False


def build_project():
    """Build Alis target using build.ps1"""
    print_header("Building Project")

    build_ps1 = PROJECT_ROOT / "scripts" / "ue" / "standalone" / "build.ps1"
    if not build_ps1.exists():
        print(f"[ERROR] build.ps1 not found at: {build_ps1}")
        return False

    cmd = [
        "powershell.exe",
        "-ExecutionPolicy", "Bypass",
        "-File", str(build_ps1)
    ]

    print(f"   Running: {build_ps1.name}...")
    print(f"   This may take a few minutes...")

    try:
        result = subprocess.run(cmd, capture_output=False, check=False, timeout=600)
        if result.returncode == 0:
            print("   Build completed successfully!")
            return True
        else:
            print(f"   Build failed with exit code {result.returncode}")
            return False
    except subprocess.TimeoutExpired:
        print("   Build timed out after 10 minutes")
        return False
    except Exception as e:
        print(f"   Build error: {e}")
        return False


def run_standalone_game(timeout_seconds=30):
    """Run UnrealEditor.exe in standalone game mode (-game flag) with auto-quit"""
    print_header("Running Standalone Game Test")
    print(f"\nTimestamp: {datetime.now()}")
    print(f"UnrealEditor: {GAME_EXE}")
    print(f"Project:      {PROJECT_FILE}")
    print(f"\nTest will run for {timeout_seconds} seconds then auto-quit...")

    REPORT_DIR.mkdir(parents=True, exist_ok=True)

    # Kill existing processes
    print("\n[1/3] Cleaning up existing processes...")
    kill_processes()

    # Run standalone game
    print("[2/3] Launching standalone game executable...")

    # UnrealEditor.exe with -game flag (standalone game mode)
    # Uses GameDefaultMap from Config/DefaultEngine.ini (L_OrchestratorBoot)
    # Triggers full Orchestrator + ProjectLoading boot (same as packaged game)
    cmd = [
        str(GAME_EXE),
        str(PROJECT_FILE),
        "-game",  # Standalone game mode (not PIE)
        "-windowed",  # Windowed mode
        "-ResX=1280",
        "-ResY=720",
        "-log",  # Enable logging
        "-NOSOUND",  # No audio to speed up
        "-NoSplash",  # Skip splash screen
        # NOTE: NOT using -ExecCmds=quit because it quits before GameInstance/World initialization
        # Instead, we run for timeout seconds then terminate the process
    ]

    start_time = time.time()

    try:
        # Start the game process
        proc = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            creationflags=subprocess.CREATE_NEW_CONSOLE if sys.platform == 'win32' else 0
        )

        # Wait for timeout
        print("   Game is running...")
        for i in range(timeout_seconds):
            time.sleep(1)
            if proc.poll() is not None:
                print(f"   Game exited early at {i+1} seconds")
                break
            print(f"   {i+1}/{timeout_seconds} seconds elapsed...")

        # Force quit if still running
        if proc.poll() is None:
            print("   Forcing game to quit...")
            proc.terminate()
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait()

        elapsed = time.time() - start_time
        print(f"\n[3/3] Game closed after {elapsed:.1f} seconds")

    except Exception as e:
        print(f"[ERROR] Failed to run game: {e}")
        return False

    # Cleanup any remaining processes
    kill_processes()

    return True


def analyze_logs():
    """Analyze the log file for boot flow success and font errors"""
    print_header("Analyzing Game Logs")

    # Find the most recent Alis.log
    log_file = TEST_LOG_DIR / "Alis.log"

    if not log_file.exists():
        print(f"[ERROR] Log file not found: {log_file}")
        return False

    print(f"\nLog file: {log_file}")
    print(f"Size: {log_file.stat().st_size / 1024:.1f} KB")

    # Read entire log file (typically 1-2k lines for a standalone run)
    with open(log_file, "r", encoding="utf-8", errors="ignore") as f:
        log_content = f.read()

    # Validation checks - match current Orchestrator/Loading pipeline messages
    boot_success_indicators = [
        "=== Orchestrator Module Loading ===",
        "Boot plugins loaded:",
        "Phase 1: Resolve Assets - Completed",
        "Phase 5: Travel - Completed successfully",
        "Pipeline completed successfully",
        "LoadMap(/MainMenuWorld/Maps/MainMenu_Persistent)"
    ]

    font_error_indicators = [
        "Failed to read file 'Roboto'",
        "GetFontFace failed to load or process 'Roboto'",
        "Last resort fallback font was requested"
    ]

    crash_indicators = [
        "EXCEPTION_ACCESS_VIOLATION",
        "Fatal error!",
        "Assertion failed"
    ]

    # Check results
    results = {
        "boot_success": [],
        "font_errors": [],
        "crashes": [],
        "all_boot_passed": True,
        "no_font_errors": True,
        "no_crashes": True
    }

    print("\n" + "-" * 70)
    print("Boot Flow Validation:")
    print("-" * 70)

    for indicator in boot_success_indicators:
        found = indicator in log_content
        status = "[PASS]" if found else "[FAIL]"
        print(f"{status} {indicator}")
        if found:
            results["boot_success"].append(indicator)
        else:
            results["all_boot_passed"] = False

    print("\n" + "-" * 70)
    print("Font Error Check:")
    print("-" * 70)

    font_error_count = 0
    for indicator in font_error_indicators:
        count = log_content.count(indicator)
        if count > 0:
            print(f"[FAIL] Found {count}x: {indicator}")
            results["font_errors"].append(f"{indicator} ({count}x)")
            results["no_font_errors"] = False
            font_error_count += count

    if font_error_count == 0:
        print("[PASS] No font errors detected")
    else:
        print(f"\n[ERROR] Total font-related warnings: {font_error_count}")

    print("\n" + "-" * 70)
    print("Crash Check:")
    print("-" * 70)

    for indicator in crash_indicators:
        if indicator in log_content:
            print(f"[FAIL] Crash detected: {indicator}")
            results["crashes"].append(indicator)
            results["no_crashes"] = False

    if results["no_crashes"]:
        print("[PASS] No crashes detected")

    # Write report
    with open(REPORT_FILE, "w") as f:
        f.write("=" * 70 + "\n")
        f.write(f"Standalone Game Test Report - {datetime.now()}\n")
        f.write("=" * 70 + "\n\n")

        f.write("BOOT FLOW VALIDATION:\n")
        for indicator in boot_success_indicators:
            status = "PASS" if indicator in results["boot_success"] else "FAIL"
            f.write(f"  [{status}] {indicator}\n")

        f.write(f"\nFONT ERRORS ({len(results['font_errors'])} issues):\n")
        if results["font_errors"]:
            for error in results["font_errors"]:
                f.write(f"  [FAIL] {error}\n")
        else:
            f.write("  [PASS] No font errors\n")

        f.write(f"\nCRASHES ({len(results['crashes'])} found):\n")
        if results["crashes"]:
            for crash in results["crashes"]:
                f.write(f"  [FAIL] {crash}\n")
        else:
            f.write("  [PASS] No crashes\n")

        f.write("\n" + "=" * 70 + "\n")
        if results["all_boot_passed"] and results["no_font_errors"] and results["no_crashes"]:
            f.write("RESULT: SUCCESS\n")
        else:
            f.write("RESULT: FAILED\n")
        f.write("=" * 70 + "\n")

    print(f"\nReport saved to: {REPORT_FILE}")

    # Return overall success
    return results["all_boot_passed"] and results["no_font_errors"] and results["no_crashes"]


def main():
    """Main entry point"""
    parser = argparse.ArgumentParser(description="Autonomous Standalone Game Test (incremental build by default)")
    parser.add_argument("--timeout", type=int, default=30, help="Seconds to run before auto-quit (default: 30)")
    parser.add_argument("--no-build", action="store_true", help="Skip building (use existing binaries)")
    parser.add_argument("--clean", action="store_true", help="Clean before building (slow, use when needed)")
    args = parser.parse_args()

    print_header("Autonomous Standalone Game Test")

    if not UE_EDITOR.exists():
        print(f"\n[ERROR] UnrealEditor.exe not found at: {UE_EDITOR}")
        return 1

    if not PROJECT_FILE.exists():
        print(f"\n[ERROR] Project file not found at: {PROJECT_FILE}")
        return 1

    # Determine if we should build (default: yes, unless --no-build)
    should_build = not args.no_build
    should_clean = args.clean and should_build  # Clean only when explicitly requested

    # Clean only if explicitly requested
    if should_clean:
        kill_processes()  # Kill UE before cleaning
        if not clean_binaries():
            print("[WARNING] Clean failed, continuing with build anyway...")
    elif args.clean and not should_build:
        print("[WARNING] --clean ignored when --no-build is set")

    # Build by default (unless --no-build)
    if should_build:
        if not should_clean:  # Only kill if we didn't already kill during clean
            kill_processes()
        if not build_project():
            print_header("TEST FAILED - Build Error")
            return 1

    # Run the game
    if not run_standalone_game(timeout_seconds=args.timeout):
        print_header("TEST FAILED - Game Execution Error")
        return 1

    # Analyze results
    time.sleep(2)  # Wait for logs to flush
    success = analyze_logs()

    if success:
        print_header("TEST PASSED")
        return 0
    else:
        print_header("TEST FAILED - See Report")
        print(f"\nReport: {REPORT_FILE}")
        return 1


if __name__ == "__main__":
    sys.exit(main())
