[CmdletBinding()]
param(
    [Parameter(Mandatory)] [string] $Driver,
    [Parameter(Mandatory)] [string] $Client
)

$ErrorActionPreference = 'Stop'
$machine = Get-CimInstance Win32_ComputerSystem
if ($machine.Model -notmatch 'Virtual Machine|VMware|VirtualBox|KVM|QEMU') {
    throw 'Run this test inside a disposable Windows VM.'
}
$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = [Security.Principal.WindowsPrincipal]::new($identity)
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw 'Use an elevated PowerShell session inside the guest.'
}

$driverPath = (Resolve-Path -LiteralPath $Driver).Path
$clientPath = (Resolve-Path -LiteralPath $Client).Path
if ((Get-AuthenticodeSignature -LiteralPath $driverPath).Status -ne 'Valid') {
    throw 'Provide a signed driver accepted by the guest test-signing setup.'
}

function Invoke-ServiceControl {
    sc.exe @args
    if ($LASTEXITCODE -ne 0) {
        throw "sc.exe failed with exit code $LASTEXITCODE"
    }
}

$name = 'BnqqKernelTelemetry'
if (Get-Service -Name $name -ErrorAction SilentlyContinue) {
    throw "Service $name already exists. This script will not reuse or modify it."
}

$registered = $false
$started = $false
try {
    Invoke-ServiceControl create $name type= kernel start= demand binPath= ('"{0}"' -f $driverPath)
    $registered = $true
    Invoke-ServiceControl start $name
    $started = $true
    & $clientPath --self-test
    if ($LASTEXITCODE -ne 0) { throw "Client test failed ($LASTEXITCODE)" }
} finally {
    if ($registered) {
        try {
            if ($started) { Invoke-ServiceControl stop $name }
        } finally {
            Invoke-ServiceControl delete $name
        }
    }
}
