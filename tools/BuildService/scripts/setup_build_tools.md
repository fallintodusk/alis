# Build Tools Setup for Build Service

**Issue:** Cargo compilation fails with `aws-lc-sys` error: "Missing dependency: cmake" and "NASM command not found"

**Root Cause:** AWS SDK requires native build tools (CMake, NASM) to compile cryptographic libraries.

---

## Quick Fix Options

### Option 1: Install CMake (Recommended - 5 minutes)

**Using Chocolatey (if installed):**
```powershell
# Run in PowerShell as Administrator
choco install cmake -y
```

**Using winget:**
```powershell
# Run in PowerShell
winget install Kitware.CMake
```

**Manual Download:**
1. Go to: https://cmake.org/download/
2. Download "Windows x64 Installer": `cmake-3.28.1-windows-x86_64.msi`
3. Run installer
4. **Important:** Check "Add CMake to system PATH for all users"
5. Restart terminal
6. Verify: `cmake --version`

**After installation:**
```bash
# Restart Git Bash, then:
cd tools/BuildService
cargo check
```

---

### Option 2: Install Visual Studio Build Tools (Complete Solution - 30-60 minutes)

This installs CMake, NASM, and all C++ build tools needed for Rust projects.

**Steps:**
1. Download: https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022
2. Run "Build Tools for Visual Studio 2022" installer
3. Select: "Desktop development with C++"
4. Includes:
   - MSVC v143 - VS 2022 C++ x64/x86 build tools
   - Windows 11 SDK
   - CMake tools for Windows
   - NASM
5. Restart computer
6. Verify:
   ```bash
   cmake --version
   cl.exe  # C++ compiler
   ```

---

### Option 3: Use Docker (Alternative - 10 minutes)

If installing build tools is problematic, compile inside Docker:

**Create `tools/BuildService/Dockerfile`:**
```dockerfile
FROM rust:1.91-bullseye

# Install CMake and build tools
RUN apt-get update && apt-get install -y \\
    cmake \\
    nasm \\
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace
```

**Build in Docker:**
```bash
# Build Docker image
docker build -t build-service-builder tools/BuildService

# Compile project
docker run --rm \\
  -v <repo>:/workspace \\
  -w /workspace/tools/BuildService \\
  build-service-builder \\
  cargo build --release

# Binary output: target/release/build_service.exe
```

---

## Verification

After installing build tools:

```bash
# 1. Verify CMake
cmake --version
# Expected: cmake version 3.28.1 or higher

# 2. Clean previous build attempts
cd tools/BuildService
rm -rf target

# 3. Try compilation
cargo check

# 4. Run tests
cargo test --lib

# 5. Build release binary
cargo build --release
```

---

## Troubleshooting

### CMake installed but still not found

**Issue:** `PATH` not updated

**Solution:**
```bash
# Windows: Add to PATH manually
# 1. Search "Environment Variables" in Start menu
# 2. Edit "Path" system variable
# 3. Add: C:\\Program Files\\CMake\\bin
# 4. Restart terminal
```

### NASM still missing

**Install NASM separately:**
```bash
# Chocolatey
choco install nasm -y

# Manual
# 1. Download from: https://www.nasm.us/pub/nasm/releasebuilds/2.16.01/win64/
# 2. Extract to C:\\Program Files\\NASM
# 3. Add to PATH: C:\\Program Files\\NASM
```

### Permission errors during compilation

**Solution:**
```bash
# Run Git Bash as Administrator
# Or: Grant write permissions to:
#   - <repo>\\tools\\BuildService\\target
#   - %USERPROFILE%\\.cargo
```

### Network errors downloading crates

**Solution:**
```bash
# Retry with:
cargo clean
cargo check --locked
```

---

## Alternative: Switch to Pure Rust TLS

**Note:** Already attempted in Cargo.toml, but aws-sdk still pulls aws-lc-sys transitively. CMake is required for AWS SDK regardless of features.

---

## Summary

**Easiest solution:** Install CMake via winget or direct download (5 min)

**Most complete:** Install Visual Studio Build Tools (30-60 min, but sets up full Rust development environment)

**No local install:** Use Docker (10 min, but slower builds)
