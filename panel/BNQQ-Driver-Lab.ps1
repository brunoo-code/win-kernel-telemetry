[CmdletBinding()]
param(
    [ValidateSet('Query', 'Stress')] [string] $Worker,
    [string] $SmokeReport
)

$ErrorActionPreference = 'Stop'
$runner = Join-Path $PSScriptRoot 'test-in-guest.ps1'
$driver = Join-Path $PSScriptRoot 'BnqqKernelTelemetry.sys'
$client = Join-Path $PSScriptRoot 'telemetry-client.exe'

if ($Worker) {
    try {
        & $runner -Driver $driver -Client $client -Mode $Worker
        exit 0
    } catch {
        [Console]::Error.WriteLine($_.Exception.Message)
        exit 1
    }
}

Add-Type -AssemblyName System.Windows.Forms,System.Drawing
[Windows.Forms.Application]::EnableVisualStyles()
$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = [Security.Principal.WindowsPrincipal]::new($identity)
if (!$principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Start-Process powershell.exe -Verb RunAs -ArgumentList @(
        '-NoProfile', '-STA', '-File', ('"{0}"' -f $PSCommandPath)
    )
    return
}

$form = [Windows.Forms.Form]::new()
$form.Text = 'BNQQ Driver Lab'
$form.ClientSize = [Drawing.Size]::new(805, 530)
$form.MinimumSize = [Drawing.Size]::new(705, 480)
$form.StartPosition = 'CenterScreen'
$form.BackColor = [Drawing.Color]::FromArgb(15, 23, 37)
$form.ForeColor = [Drawing.Color]::FromArgb(229, 237, 250)
$form.Font = [Drawing.Font]::new('Segoe UI', 10)

function Add-Label($text, $x, $y, $width, $height, $fontSize) {
    $label = [Windows.Forms.Label]::new()
    $label.Text = $text
    $label.SetBounds($x, $y, $width, $height)
    $label.Font = [Drawing.Font]::new('Segoe UI', $fontSize)
    $form.Controls.Add($label)
    return $label
}

function Add-Button($text, $x) {
    $button = [Windows.Forms.Button]::new()
    $button.Text = $text
    $button.SetBounds($x, 108, 191, 40)
    $button.FlatStyle = 'Flat'
    $button.BackColor = [Drawing.Color]::FromArgb(34, 57, 78)
    $form.Controls.Add($button)
    return $button
}

$title = Add-Label 'BNQQ DRIVER LAB' 23 18 755 44 27
$title.ForeColor = [Drawing.Color]::FromArgb(127, 232, 221)
$null = Add-Label 'SYSTEM ENGINEERING  /  TELEMETRIA EM MODO KERNEL' 25 67 755 25 10
$queryButton = Add-Button 'Consultar driver' 25
$stressButton = Add-Button 'Rodar teste de carga' 226
$exportButton = Add-Button 'Exportar relatório' 427
$exportButton.Enabled = $false
$status = Add-Label 'Pronto. Escolha uma operação.' 25 159 755 25 10
$log = [Windows.Forms.TextBox]::new()
$log.SetBounds(25, 194, 755, 308)
$log.Anchor = 'Top, Bottom, Left, Right'
$log.Multiline = $true
$log.ReadOnly = $true
$log.ScrollBars = 'Both'
$log.WordWrap = $false
$log.Font = [Drawing.Font]::new('Consolas', 10)
$log.BackColor = [Drawing.Color]::FromArgb(23, 35, 52)
$log.ForeColor = $form.ForeColor
$form.Controls.Add($log)

$timer = [Windows.Forms.Timer]::new()
$timer.Interval = 200
$job = @{ Process = $null; Output = ''; Error = ''; Mode = ''; Reports = [Collections.Generic.List[string]]::new() }
$smoke = @{ Step = 0; Exit = 0 }

function Export-Report([string] $path) {
    [IO.File]::WriteAllText($path, ($job.Reports -join ([Environment]::NewLine * 2)), [Text.UTF8Encoding]::new($true))
}

function Start-LabAction([string] $mode) {
    if ($job.Process) { return }
    $prefix = Join-Path ([IO.Path]::GetTempPath()) ('bnqq-lab-' + [Guid]::NewGuid())
    $job.Output = "$prefix.out"
    $job.Error = "$prefix.err"
    $job.Mode = $mode
    try {
        $job.Process = Start-Process powershell.exe -ArgumentList @(
            '-NoProfile', '-NonInteractive', '-File', ('"{0}"' -f $PSCommandPath), '-Worker', $mode
        ) -RedirectStandardOutput $job.Output -RedirectStandardError $job.Error -WindowStyle Hidden -PassThru
        $null = $job.Process.Handle
        $queryButton.Enabled = $stressButton.Enabled = $exportButton.Enabled = $false
        $status.Text = if ($mode -eq 'Query') { 'Consultando o driver…' } else { 'Executando 2.000 consultas em 8 conexões…' }
        $timer.Start()
    } catch {
        $status.Text = 'Falha ao iniciar o teste.'
        $log.Text = $_.Exception.Message
        if ($SmokeReport) { $smoke.Exit = 1; $form.Close() }
    }
}

$queryButton.Add_Click({ Start-LabAction 'Query' })
$stressButton.Add_Click({ Start-LabAction 'Stress' })
$exportButton.Add_Click({
    $dialog = [Windows.Forms.SaveFileDialog]::new()
    try {
        $dialog.Filter = 'Relatório de texto (*.txt)|*.txt'
        $dialog.FileName = 'BNQQ-Driver-Lab.txt'
        if ($dialog.ShowDialog($form) -eq 'OK') {
            Export-Report $dialog.FileName
            $status.Text = 'Relatório exportado.'
        }
    } catch { $status.Text = 'Não foi possível salvar o relatório.'; $log.AppendText("`r`n$($_.Exception.Message)") }
    finally { $dialog.Dispose() }
})

$timer.Add_Tick({
    if (!$job.Process.HasExited) { return }
    $timer.Stop()
    $job.Process.WaitForExit()
    $exitCode = $job.Process.ExitCode
    $output = Get-Content -LiteralPath $job.Output -Raw
    $errors = Get-Content -LiteralPath $job.Error -Raw
    $report = "[{0:o}] {1} | exit={2}`r`n{3}{4}" -f (Get-Date), $job.Mode, $exitCode, $output, $errors
    $job.Reports.Add($report.TrimEnd())
    $log.Text = $report
    $job.Process.Dispose()
    $job.Process = $null
    [IO.File]::Delete($job.Output)
    [IO.File]::Delete($job.Error)
    $queryButton.Enabled = $stressButton.Enabled = $exportButton.Enabled = $true
    $status.Text = if ($exitCode -eq 0) { 'Concluído. Relatório disponível.' } else { 'Falha. Confira a saída abaixo.' }
    if ($SmokeReport) {
        if ($exitCode -ne 0 -or ($smoke.Step -eq 0 -and $output -notmatch 'Requests: 1\b') -or
            ($smoke.Step -eq 1 -and $output -notmatch 'Requests: 2002\b')) {
            $smoke.Exit = 1
        } elseif ($smoke.Step -eq 0) {
            $smoke.Step = 1
            $stressButton.PerformClick()
            return
        }
        Export-Report $SmokeReport
        $form.Close()
    }
})

$form.Add_FormClosing({
    param($sender, $eventArgs)
    if ($job.Process) {
        $eventArgs.Cancel = $true
        $status.Text = 'Aguarde o teste finalizar para fechar.'
    }
})
if ($SmokeReport) { $form.Add_Shown({ $queryButton.PerformClick() }) }
try { [Windows.Forms.Application]::Run($form) }
finally { $timer.Dispose(); $form.Dispose() }
if ($SmokeReport) { exit $smoke.Exit }
