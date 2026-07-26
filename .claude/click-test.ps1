param([int]$FX, [int]$FY)
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System; using System.Runtime.InteropServices;
public class W5 {
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
[W5]::SetForegroundWindow($h) | Out-Null; Start-Sleep -Milliseconds 350
$script:r = New-Object W5+RECT; [W5]::GetWindowRect($h, [ref]$script:r) | Out-Null
function Click($relX, $relY){
  [W5]::SetCursorPos($script:r.Left + $relX, $script:r.Top + $relY) | Out-Null
  Start-Sleep -Milliseconds 130
  [W5]::mouse_event(0x0002, 0,0,0,0) | Out-Null  # LEFTDOWN
  Start-Sleep -Milliseconds 60
  [W5]::mouse_event(0x0004, 0,0,0,0) | Out-Null  # LEFTUP
  Start-Sleep -Milliseconds 280
}
function Capture($path){
  $r2 = New-Object W5+RECT; [W5]::GetWindowRect($h, [ref]$r2) | Out-Null
  $w=$r2.Right-$r2.Left; $ht=$r2.Bottom-$r2.Top
  $bmp = New-Object System.Drawing.Bitmap $w,$ht
  $g=[System.Drawing.Graphics]::FromImage($bmp)
  $g.CopyFromScreen($r2.Left,$r2.Top,0,0,(New-Object System.Drawing.Size($w,$ht)))
  $g.Dispose(); $bmp.Save($path); $bmp.Dispose()
}
Click 1130 160            # empty area -> deselect
Capture "C:\code\geomTool-qt\build-gl\clickB.png"
Click $FX $FY             # face interior -> should reselect
Capture "C:\code\geomTool-qt\build-gl\clickC.png"
Write-Output ("clicked face at rel " + $FX + "," + $FY)
