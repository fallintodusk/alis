# Reliably bring the Unreal Editor window to the foreground.
# Windows refuses SetForegroundWindow from a non-foreground process; the sanctioned
# workaround is to attach the calling thread's input queue to the current
# foreground thread for the duration of the call.
param([int]$ProcessId = 0)

Add-Type @'
using System;
using System.Runtime.InteropServices;
public class WinFocus {
  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, IntPtr pid);
  [DllImport("user32.dll")] public static extern bool AttachThreadInput(uint a, uint b, bool attach);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr h);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int cmd);
  [DllImport("kernel32.dll")] public static extern uint GetCurrentThreadId();
  public static bool Focus(IntPtr target) {
    IntPtr fg = GetForegroundWindow();
    uint fgThread = GetWindowThreadProcessId(fg, IntPtr.Zero);
    uint myThread = GetCurrentThreadId();
    AttachThreadInput(myThread, fgThread, true);
    ShowWindow(target, 3);
    BringWindowToTop(target);
    bool ok = SetForegroundWindow(target);
    AttachThreadInput(myThread, fgThread, false);
    return ok;
  }
}
'@

$proc = if ($ProcessId -gt 0) { Get-Process -Id $ProcessId -ErrorAction Stop }
        else { Get-Process UnrealEditor -ErrorAction Stop | Select-Object -First 1 }
$ok = [WinFocus]::Focus($proc.MainWindowHandle)
Start-Sleep -Milliseconds 400
$isFg = ([WinFocus]::GetForegroundWindow() -eq $proc.MainWindowHandle)
Write-Output "pid=$($proc.Id) set=$ok foreground=$isFg"
if (-not $isFg) { exit 1 }
