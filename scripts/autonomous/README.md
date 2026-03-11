# Autonomous Scripts Subrouter

This directory contains scripts and utilities for autonomous agents (Claude).

## Directory Structure

### [`common/`](./common)
Shared utilities and infrastructure used by all autonomous workflows.

- **`Common.psm1`**: Core library for resilience (circuit breaker, exponential backoff, logging).
- **`discover_todos.ps1`**: Scans codebase for agent-compatible TODOs.
- **`Rotate-Logs.ps1`**: Manages log files to prevent disk exhaustion.
- **`Export-Dashboard.ps1`**: Generates HTML status dashboard.
- **`PATH_HANDLING.md`**: Guide for cross-platform (Windows/WSL) path handling.

### [`claude/`](./claude)
Workflows specific to the Claude agent.

- **[`overnight/`](./claude/overnight)**: The primary infinite loop workflow.
  - `main.ps1`: Entry point for the overnight session.
- **[`yolo/`](./claude/yolo)**: Fully autonomous mode (skip permissions).
  - `yolo.ps1`: Script to run Claude without prompts (Use with caution).

## Usage

### Run Overnight Session (Recommended)
```powershell
powershell scripts/autonomous/claude/overnight/main.ps1
```

### Run YOLO Mode (Advanced)
```powershell
powershell scripts/autonomous/claude/yolo/yolo.ps1
```

### Check TODO Status
```powershell
powershell scripts/autonomous/common/discover_todos.ps1
```
