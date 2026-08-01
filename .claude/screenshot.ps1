# Find the geomTool process window and capture it directly.
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class W2 {
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool ShowWindowAsync(IntPtr h, int n);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
}
"@
$procs = Get-Process -Name geomTool -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowHandle -ne 0 }
$p = $procs | Where-Object { $_.MainWindowTitle -like "*DEMO*" } | Select-Object -First 1
if (-not $p) { $p = $procs | Select-Object -First 1 }
if (-not $p) { Write-Output "geomTool process not found"; exit 1 }
# Write shot.png next to the running geomTool.exe (works for any build dir).
$exeDir = if ($p.Path) { Split-Path $p.Path -Parent } else { Join-Path $PSScriptRoot '..\build' }
$out = Join-Path $exeDir 'shot.png'
$h = $p.MainWindowHandle
[W2]::ShowWindowAsync($h, 9) | Out-Null      # SW_RESTORE
[W2]::SetForegroundWindow($h) | Out-Null
Start-Sleep -Milliseconds 400
$r = New-Object W2+RECT
if (-not [W2]::GetWindowRect($h, [ref]$r)) { Write-Output "GetWindowRect failed"; exit 1 }
$w = $r.Right - $r.Left; $ht = $r.Bottom - $r.Top
$bmp = New-Object System.Drawing.Bitmap $w, $ht
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($r.Left, $r.Top, 0, 0, (New-Object System.Drawing.Size($w, $ht)))
$g.Dispose(); $bmp.Save($out); $bmp.Dispose()
Write-Output ("captured '" + $p.MainWindowTitle + "' " + $w + "x" + $ht)
