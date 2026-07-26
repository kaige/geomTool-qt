param([int]$X, [int]$Y)
Add-Type @"
using System; using System.Runtime.InteropServices;
public class W6 {
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, uint dw, IntPtr ex);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
}
"@
$p = Get-Process geomTool -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowTitle -like "*DEMO*" } | Select-Object -First 1
if (-not $p) { Write-Output "no demo process"; exit 1 }
$h = $p.MainWindowHandle
[W6]::SetForegroundWindow($h) | Out-Null
Start-Sleep -Milliseconds 350
$r = New-Object W6+RECT
[W6]::GetWindowRect($h, [ref]$r) | Out-Null
[W6]::SetCursorPos($r.Left + $X, $r.Top + $Y) | Out-Null
Start-Sleep -Milliseconds 150
[W6]::mouse_event(0x0002, 0, 0, 0, 0) | Out-Null  # LEFTDOWN
Start-Sleep -Milliseconds 70
[W6]::mouse_event(0x0004, 0, 0, 0, 0) | Out-Null  # LEFTUP
Start-Sleep -Milliseconds 300
Write-Output ("clicked rel " + $X + "," + $Y)
