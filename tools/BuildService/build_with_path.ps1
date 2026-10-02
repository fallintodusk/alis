# Build Build Service with CMake and NASM in PATH (session-only)
# This adds CMake/NASM to PATH for this PowerShell session only

Write-Host "Adding CMake and NASM to PATH for this session..."
$env:Path = "C:\Program Files\CMake\bin;C:\Program Files\NASM;$env:Path"

Write-Host "Verifying tools..."
Write-Host "CMake: $(cmake --version | Select-Object -First 1)"
Write-Host "NASM: $(if (Get-Command nasm -ErrorAction SilentlyContinue) { nasm --version } else { 'Not found' })"

Write-Host "`nRunning cargo check..."
cargo check
