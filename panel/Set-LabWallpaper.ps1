[CmdletBinding()]
param([switch] $LockInLab)

$ErrorActionPreference = 'Stop'
$machine = Get-CimInstance Win32_ComputerSystem
if ($machine.Manufacturer -ne 'Microsoft Corporation' -or $machine.Model -ne 'Virtual Machine') {
    throw 'Apply this wallpaper inside the Hyper-V guest.'
}
Add-Type -AssemblyName System.Drawing
$image = [Drawing.Image]::FromFile((Join-Path $PSScriptRoot 'BNQQ-Wallpaper.png'))
$wallpaper = Join-Path $PSScriptRoot 'BNQQ-Wallpaper.bmp'
try { $image.Save($wallpaper, [Drawing.Imaging.ImageFormat]::Bmp) }
finally { $image.Dispose() }

Add-Type @'
using System.Runtime.InteropServices;
public static class BnqqDesktop {
    [DllImport("user32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    public static extern bool SystemParametersInfo(int action, int param, string path, int flags);
}
'@
$desktop = 'HKCU:\Control Panel\Desktop'
Set-ItemProperty -Path $desktop -Name WallpaperStyle -Value '10'
Set-ItemProperty -Path $desktop -Name TileWallpaper -Value '0'
if (![BnqqDesktop]::SystemParametersInfo(20, 0, $wallpaper, 3)) {
    throw ('Could not apply wallpaper. Win32 error: ' + [Runtime.InteropServices.Marshal]::GetLastWin32Error())
}

if ($LockInLab) {
    $active = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Policies\ActiveDesktop'
    $policy = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Policies\System'
    foreach ($key in $active, $policy) {
        if (!(Test-Path -LiteralPath $key)) { New-Item -Path $key -Force | Out-Null }
    }
    New-ItemProperty -Path $active -Name NoChangingWallPaper -Value 1 -PropertyType DWord -Force | Out-Null
    New-ItemProperty -Path $policy -Name Wallpaper -Value $wallpaper -PropertyType String -Force | Out-Null
    New-ItemProperty -Path $policy -Name WallpaperStyle -Value '2' -PropertyType String -Force | Out-Null
}
