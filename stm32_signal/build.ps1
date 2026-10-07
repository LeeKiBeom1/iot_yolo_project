param([switch]$Flash)
$ErrorActionPreference = 'Stop'

# Use tools already installed with STM32CubeIDE; no global PATH changes.
$taskToolRoot = 'C:\ST'
$taskTools = @('cmake.exe', 'ninja.exe', 'arm-none-eabi-gcc.exe')
if ($Flash) { $taskTools += 'STM32_Programmer_CLI.exe' }
$taskResolvedTools = @{}
foreach ($taskTool in $taskTools) {
    $taskCommand = Get-Command $taskTool -ErrorAction SilentlyContinue
    if ($taskCommand) {
        $taskResolvedTools[$taskTool] = $taskCommand.Source
    } else {
        $taskFile = Get-ChildItem -LiteralPath $taskToolRoot -Filter $taskTool -File -Recurse -ErrorAction SilentlyContinue |
            Sort-Object FullName -Descending | Select-Object -First 1
        if (!$taskFile) { throw "$taskTool not found. Install STM32CubeIDE tools or add them to PATH." }
        $taskResolvedTools[$taskTool] = $taskFile.FullName
    }
}
$taskPreviousPath = $env:PATH
try {
    $taskToolDirs = $taskResolvedTools.Values | ForEach-Object { Split-Path $_ } | Select-Object -Unique
    $env:PATH = ($taskToolDirs -join ';') + ';' + $env:PATH
    Push-Location $PSScriptRoot
    try {
        & $taskResolvedTools['cmake.exe'] --preset Debug
        if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed.' }
        & $taskResolvedTools['cmake.exe'] --build --preset Debug
        if ($LASTEXITCODE -ne 0) { throw 'Build failed.' }
        if ($Flash) {
            & $taskResolvedTools['STM32_Programmer_CLI.exe'] -c port=SWD -w "$PSScriptRoot/build/Debug/stm32_signal.elf" -v -rst
            if ($LASTEXITCODE -ne 0) { throw 'Programming failed. Check ST-LINK USB connection.' }
        }
    } finally {
        Pop-Location
    }
} finally {
    $env:PATH = $taskPreviousPath
}
