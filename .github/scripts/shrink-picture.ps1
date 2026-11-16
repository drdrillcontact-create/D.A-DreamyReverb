# Makes a smaller JPEG copy of a picture. (Called by annotate-picture.sh.)
param(
    [Parameter(Mandatory = $true)][string]$In,
    [Parameter(Mandatory = $true)][string]$Out,
    [int]$Width = 640,
    [int]$Quality = 50
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

$picture = [System.Drawing.Image]::FromFile($In)

try {
    if ($Width -gt $picture.Width) { $Width = $picture.Width }
    $height = [int][Math]::Round($picture.Height * $Width / $picture.Width)

    $small = New-Object System.Drawing.Bitmap $Width, $height
    $graphics = [System.Drawing.Graphics]::FromImage($small)

    try {
        $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $graphics.DrawImage($picture, 0, 0, $Width, $height)

        $jpeg = [System.Drawing.Imaging.ImageCodecInfo]::GetImageEncoders() | Where-Object { $_.MimeType -eq "image/jpeg" }
        $options = New-Object System.Drawing.Imaging.EncoderParameters 1
        $options.Param[0] = New-Object System.Drawing.Imaging.EncoderParameter ([System.Drawing.Imaging.Encoder]::Quality, [long]$Quality)
        $small.Save($Out, $jpeg, $options)
    }
    finally {
        $graphics.Dispose()
        $small.Dispose()
    }
}
finally {
    $picture.Dispose()
}
