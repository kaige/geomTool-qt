# Crop the canvas region of the captured window and zoom in for inspection.
Add-Type -AssemblyName System.Drawing
$src = [System.Drawing.Image]::FromFile("C:\code\geomTool-qt\build-gl\shot.png")
# Canvas ≈ right of the 250px sidebar, below the toolbar, above the status bar.
$x = 300; $y = 90; $w = [Math]::Min(900, $src.Width - $x); $h = [Math]::Min(700, $src.Height - $y)
$crop = New-Object System.Drawing.Bitmap $w, $h
$g = [System.Drawing.Graphics]::FromImage($crop)
$g.DrawImage($src, (New-Object System.Drawing.Rectangle(0,0,$w,$h)), $x, $y, $w, $h, [System.Drawing.GraphicsUnit]::Pixel)
$g.Dispose(); $src.Dispose()
$scale = 2
$big = New-Object System.Drawing.Bitmap ($w*$scale), ($h*$scale)
$g2 = [System.Drawing.Graphics]::FromImage($big)
$g2.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::NearestNeighbor
$g2.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
$g2.DrawImage($crop, 0, 0, $w*$scale, $h*$scale)
$g2.Dispose(); $crop.Dispose()
$big.Save("C:\code\geomTool-qt\build-gl\crop.png")
$big.Dispose()
Write-Output ("crop " + $w + "x" + $h + " -> " + ($w*$scale) + "x" + ($h*$scale))
