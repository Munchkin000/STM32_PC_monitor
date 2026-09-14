param(
    [string]$OutputDirectory = "dist"
)

$ErrorActionPreference = "Stop"
Set-Location -LiteralPath $PSScriptRoot
$PythonPrefix = python -c "import sys; print(sys.prefix)"
$TclDirectory = Join-Path $PythonPrefix "Library\lib\tcl8.6"
$TkDirectory = Join-Path $PythonPrefix "Library\lib\tk8.6"

if (-not (Test-Path -LiteralPath (Join-Path $TclDirectory "init.tcl"))) {
    throw "Tcl runtime not found: $TclDirectory"
}
if (-not (Test-Path -LiteralPath (Join-Path $TkDirectory "tk.tcl"))) {
    throw "Tk runtime not found: $TkDirectory"
}

python -m PyInstaller `
    --noconfirm `
    --clean `
    --onefile `
    --windowed `
    --name STM32_PC_Monitor `
    --distpath $OutputDirectory `
    --hidden-import pynvml `
    --hidden-import pystray._win32 `
    --exclude-module numpy `
    --collect-submodules serial `
    --add-data "$TclDirectory;_tcl_data" `
    --add-data "$TkDirectory;_tk_data" `
    --add-data "tools;tools" `
    main.py

Write-Host "Build complete: $PSScriptRoot\$OutputDirectory\STM32_PC_Monitor.exe"
