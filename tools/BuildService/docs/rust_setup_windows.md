# Rust Setup for Windows (KISS)

Simple instructions for installing Rust on Windows for Build Service development.

## Prerequisites

- Windows 10/11
- Internet connection
- Administrator access (for installation)

## Installation Steps

### Option 1: Official Rust Installer (Recommended)

**1. Download rustup-init.exe**
- Visit: https://rustup.rs/
- Click "Download rustup-init.exe (64-bit)"
- Save to Downloads folder

**2. Run installer**
```cmd
REM Navigate to Downloads
cd %USERPROFILE%\Downloads

REM Run installer
rustup-init.exe
```

**3. Follow prompts**
- Press `1` to proceed with default installation
- Wait for download and installation (5-10 minutes)
- Press Enter to finish

**4. Open new terminal**
- Close current terminal window
- Open new Command Prompt or PowerShell
- This loads new PATH with Rust tools

**5. Verify installation**
```cmd
rustc --version
cargo --version
```

Expected output:
```
rustc 1.XX.X (hash date)
cargo 1.XX.X (hash date)
```

### Option 2: Package Manager (Chocolatey/Winget)

**Using Chocolatey:**
```cmd
choco install rust
```

**Using winget:**
```cmd
winget install Rustlang.Rust.MSVC
```

## Visual Studio Build Tools

Rust on Windows requires Visual Studio C++ build tools.

**If you already have Visual Studio installed:**
- Ensure "Desktop development with C++" workload is installed
- Done!

**If you don't have Visual Studio:**

**Option A: Full Visual Studio (if you need it for UE)**
- Download Visual Studio 2022 Community: https://visualstudio.microsoft.com/
- Install "Desktop development with C++" workload

**Option B: Build Tools only (minimal)**
```cmd
REM Download VS Build Tools
REM Visit: https://visualstudio.microsoft.com/downloads/
REM Scroll to "Tools for Visual Studio"
REM Download "Build Tools for Visual Studio 2022"

REM Run installer
REM Select "Desktop development with C++"
REM Install
```

## Verify Setup

**Test Rust compilation:**
```cmd
REM Create test project
cargo new hello_test
cd hello_test

REM Build and run
cargo run
```

Expected output:
```
   Compiling hello_test v0.1.0
    Finished dev [unoptimized + debuginfo] target(s) in X.XXs
     Running `target\debug\hello_test.exe`
Hello, world!
```

If you see "Hello, world!" - Rust is working!

## Build Service Setup

**Navigate to Build Service:**
```cmd
cd <repo>\tools\BuildService
```

**Build:**
```cmd
cargo build --release
```

**Run:**
```cmd
cargo run -- build --help
```

## Troubleshooting

**"link.exe not found":**
- Install Visual Studio C++ build tools (see above)
- Restart terminal after installation

**"rustup: command not found":**
- Restart terminal to load new PATH
- Or manually add to PATH: `%USERPROFILE%\.cargo\bin`

**Slow compilation:**
- First build is always slow (downloads dependencies)
- Subsequent builds are faster
- Use `cargo build --release` for optimized builds

**Antivirus blocking:**
- Some antivirus software blocks Rust downloads
- Temporarily disable or add exception for rustup and cargo

## Updating Rust

```cmd
REM Update Rust toolchain
rustup update

REM Check version
rustc --version
```

## Uninstalling Rust

```cmd
rustup self uninstall
```

## IDE Setup (Optional)

**Visual Studio Code:**
1. Install VS Code: https://code.visualstudio.com/
2. Install "rust-analyzer" extension
3. Open `tools/BuildService/` folder
4. Enjoy IntelliSense and debugging

**Visual Studio:**
- Rust support via extensions available but not required

## Next Steps

- [Build Service Configuration](configuration.md)
- [Build Service Architecture](architecture/README.md)
- [Build and run Build Service](../README.md)

## Resources

- Official Rust Book: https://doc.rust-lang.org/book/
- Rust by Example: https://doc.rust-lang.org/rust-by-example/
- Cargo Book: https://doc.rust-lang.org/cargo/
