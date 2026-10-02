# Build Service Scripts

| Need | Owner |
|---|---|
| Run Cargo with Windows native dependencies | `build.ps1` |
| Run Cargo from Git Bash | `build.sh` |
| Install or inspect Windows build prerequisites | `install_rust_windows.bat`, `setup_dev_env.bat`, and `setup_build_tools.md` |

Build, test, and CLI behavior belong to Cargo and the Rust executable. Do not
add a second pipeline implementation in wrapper scripts.
