param(
    [string]$BindAddress = '127.0.0.1',
    [ValidateRange(1024, 65535)][int]$Port = 1883
)
$ErrorActionPreference = 'Stop'
$bindIP = $null
if (-not [System.Net.IPAddress]::TryParse($BindAddress, [ref]$bindIP) -or
    $bindIP.AddressFamily -ne [System.Net.Sockets.AddressFamily]::InterNetwork -or
    $bindIP.Equals([System.Net.IPAddress]::Any) -or
    $bindIP.Equals([System.Net.IPAddress]::Broadcast)) {
    throw 'Use 127.0.0.1 or one specific laptop LAN IPv4 address.'
}
$brokerPath = Join-Path $PSScriptRoot '.runtime/mosquitto-2.1.2/mosquitto.exe'
if (-not (Test-Path -LiteralPath $brokerPath)) { throw 'Workspace Mosquitto runtime is missing; see README.md.' }
$configPath = Join-Path $PSScriptRoot '.runtime/active.conf'
@"
# Temporary AAI05 bring-up listener; no Internet port forwarding.
listener $Port $bindIP
allow_anonymous true
persistence false
log_dest stdout
"@ | Set-Content -LiteralPath $configPath -Encoding Ascii
Write-Host "AAI05 broker: $bindIP`:$Port. Leave this terminal open; Ctrl+C stops it."
& $brokerPath -c $configPath -v
exit $LASTEXITCODE
