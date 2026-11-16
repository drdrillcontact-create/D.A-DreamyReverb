# Photographs the screen into a PNG file: all of the main screen, or the given part of it.
# (Called by capture-screen.sh.) Says in one line, beginning "screen:", how it went.
param(
    [Parameter(Mandatory = $true)][string]$Out,
    [int]$X = 0,
    [int]$Y = 0,
    [int]$W = 0,
    [int]$H = 0
)

$ErrorActionPreference = "Stop"

# The way that takes in every kind of window: BitBlt with CAPTUREBLT, which also copies windows
# that are see-through in places (the installer's window is one). .NET's own CopyFromScreen
# cannot be asked for that: it turns down SRCCOPY and CAPTUREBLT together.
function Copy-WithCaptureBlt([int]$fromX, [int]$fromY, [int]$width, [int]$height) {
    Add-Type -Namespace DaAura -Name Gdi -MemberDefinition @'
[DllImport("user32.dll")] public static extern IntPtr GetDC(IntPtr window);
[DllImport("user32.dll")] public static extern int ReleaseDC(IntPtr window, IntPtr dc);
[DllImport("gdi32.dll")] public static extern bool BitBlt(IntPtr target, int x, int y, int width, int height, IntPtr source, int sourceX, int sourceY, uint operation);
'@

    $picture = New-Object System.Drawing.Bitmap $width, $height
    $onto = [System.Drawing.Graphics]::FromImage($picture)
    $screenDc = [DaAura.Gdi]::GetDC([IntPtr]::Zero)
    $pictureDc = $onto.GetHdc()
    $copied = $false

    try {
        # SRCCOPY | CAPTUREBLT
        $copied = [DaAura.Gdi]::BitBlt($pictureDc, 0, 0, $width, $height, $screenDc, $fromX, $fromY, [uint32]0x40CC0020)
    }
    finally {
        $onto.ReleaseHdc($pictureDc)
        [void][DaAura.Gdi]::ReleaseDC([IntPtr]::Zero, $screenDc)
        $onto.Dispose()
    }

    if (-not $copied) {
        $picture.Dispose()
        throw "BitBlt with CAPTUREBLT was refused"
    }

    return $picture
}

try {
    Add-Type -AssemblyName System.Drawing
    Add-Type -AssemblyName System.Windows.Forms

    $screen = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds

    if ($W -le 0 -or $H -le 0) {
        $X = $screen.X; $Y = $screen.Y; $W = $screen.Width; $H = $screen.Height
    }

    # (not beyond the screen's edge)
    if ($X + $W -gt $screen.Right)  { $W = $screen.Right - $X }
    if ($Y + $H -gt $screen.Bottom) { $H = $screen.Bottom - $Y }

    $how = "with CAPTUREBLT"
    $bitmap = $null

    try {
        $bitmap = Copy-WithCaptureBlt $X $Y $W $H
    }
    catch {
        # (Should Windows not go along with that: the plain way, which has always worked here.)
        $how = "the plain way, since: " + $_.Exception.Message
        $bitmap = $null
    }

    if ($null -eq $bitmap) {
        $bitmap = New-Object System.Drawing.Bitmap $W, $H
        $graphics = [System.Drawing.Graphics]::FromImage($bitmap)

        try {
            $graphics.CopyFromScreen($X, $Y, 0, 0, $bitmap.Size)
        }
        finally {
            $graphics.Dispose()
        }
    }

    try {
        $bitmap.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        $bitmap.Dispose()
    }

    Write-Output "screen: photographed $how"
}
catch {
    Write-Output ("screen: could not be photographed: " + $_.Exception.Message)
    exit 1
}
