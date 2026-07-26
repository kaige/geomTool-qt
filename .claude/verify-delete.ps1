Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System; using System.Runtime.InteropServices;
public class W4 {
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
}
"@
function Capture($hwnd, $path){
  $r = New-Object W4+RECT
  [W4]::GetWindowRect($hwnd, [ref]$r) | Out-Null
  $w=$r.Right-$r.Left; $h=$r.Bottom-$r.Top
  if ($w -le 0 -or $h -le 0) { return }
  $bmp = New-Object System.Drawing.Bitmap $w,$h
  $g=[System.Drawing.Graphics]::FromImage($bmp)
  $g.CopyFromScreen($r.Left,$r.Top,0,0,(New-Object System.Drawing.Size($w,$h)))
  $g.Dispose(); $bmp.Save($path); $bmp.Dispose()
}
$p = Get-Process geomTool -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowTitle -like "*DEMO*" } | Select-Object -First 1
if (-not $p) { Write-Output "no demo process"; exit 1 }
$h = $p.MainWindowHandle
[W4]::SetForegroundWindow($h) | Out-Null
Start-Sleep -Milliseconds 400
Capture $h "C:\code\geomTool-qt\build-gl\before.png"
[System.Windows.Forms.SendKeys]::SendWait("{DELETE}")
Start-Sleep -Milliseconds 500
Capture $h "C:\code\geomTool-qt\build-gl\after.png"
Write-Output "captured before.png and after.png"
