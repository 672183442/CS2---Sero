[CmdletBinding()]
param(
    [string]$ProjectRoot = (Split-Path -Parent $PSScriptRoot),
    [string]$A2xExecutable,
    [string]$UniversalExecutable,
    [switch]$SkipRun,
    [switch]$RequireFresh,
    [switch]$StrictCrossCheck
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$ProjectRoot = [System.IO.Path]::GetFullPath($ProjectRoot)
$a2xRoot = Join-Path $ProjectRoot "cs2-dumper-main"
$universalRoot = Join-Path $ProjectRoot "cs2-universal-offsets-main"
$destinationRoot = Join-Path $ProjectRoot "dumper"
$stageRoot = Join-Path $ProjectRoot (".dump-staging\" + [Guid]::NewGuid().ToString("N"))

$generatedHeaders = @(
    "animationsystem_dll.hpp",
    "buttons.hpp",
    "client_dll.hpp",
    "engine2_dll.hpp",
    "host_dll.hpp",
    "interfaces.hpp",
    "materialsystem2_dll.hpp",
    "networksystem_dll.hpp",
    "offsets.hpp",
    "panorama_dll.hpp",
    "particles_dll.hpp",
    "pulse_system_dll.hpp",
    "rendersystemdx11_dll.hpp",
    "resourcesystem_dll.hpp",
    "scenesystem_dll.hpp",
    "schemasystem_dll.hpp",
    "server_dll.hpp",
    "soundsystem_dll.hpp",
    "steamaudio_dll.hpp",
    "vphysics2_dll.hpp",
    "worldrenderer_dll.hpp"
)

$requiredOffsets = @(
    "dwCSGOInput",
    "dwEntityList",
    "dwGameEntitySystem",
    "dwGameEntitySystem_highestEntityIndex",
    "dwLocalPlayerPawn",
    "dwPlantedC4",
    "dwViewAngles",
    "dwViewMatrix",
    "dwWeaponC4"
)

$requiredSchemaSymbols = @(
    "C_BaseEntity",
    "C_BaseModelEntity",
    "C_BasePlayerPawn",
    "C_CSPlayerPawn",
    "CGlowProperty",
    "EntitySpottedState_t",
    "C_PlantedC4",
    "CCSPlayer_AimPunchServices",
    "m_iHealth",
    "m_iTeamNum",
    "m_pGameSceneNode",
    "m_vecViewOffset",
    "m_vOldOrigin",
    "m_hPawn",
    "m_hActiveWeapon"
)

$projectPatterns = @(
    "OnGlowTypeChanged",
    "WriteSubtickFromEntry",
    "OverrideView"
)

function Write-Status {
    param([string]$Message)
    Write-Host "[cs2-dumper] $Message"
}

function Invoke-Checked {
    param(
        [string]$FilePath,
        [string[]]$Arguments,
        [string]$WorkingDirectory
    )

    Push-Location $WorkingDirectory
    try {
        & $FilePath @Arguments
        if ($LASTEXITCODE -ne 0) {
            throw "'$FilePath' exited with code $LASTEXITCODE."
        }
    }
    finally {
        Pop-Location
    }
}

function Resolve-DumperExecutable {
    param(
        [string]$ExplicitPath,
        [string[]]$Candidates
    )

    if ($ExplicitPath) {
        $resolved = [System.IO.Path]::GetFullPath($ExplicitPath)
        if (-not (Test-Path -LiteralPath $resolved -PathType Leaf)) {
            throw "Dumper executable not found: $resolved"
        }
        return $resolved
    }

    foreach ($candidate in $Candidates) {
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            return $candidate
        }
    }

    return $null
}

function Invoke-A2xDumper {
    param([string]$OutputDirectory)

    $executable = Resolve-DumperExecutable $A2xExecutable @(
        (Join-Path $a2xRoot "target\release\cs2-dumper.exe")
    )

    $arguments = @("-f", "hpp,json", "-o", $OutputDirectory, "--no-log-file")
    if ($executable) {
        Invoke-Checked $executable $arguments $a2xRoot
        return $true
    }

    $cargo = Get-Command cargo -ErrorAction SilentlyContinue
    if ($cargo) {
        Invoke-Checked $cargo.Source (@("run", "--release", "--") + $arguments) $a2xRoot
        return $true
    }

    return $false
}

function Invoke-UniversalDumper {
    param([string]$OutputDirectory)

    $executable = Resolve-DumperExecutable $UniversalExecutable @(
        (Join-Path $universalRoot "target\release\cs2-sdk.exe"),
        (Join-Path $universalRoot "target\release\cs2-universal-offsets.exe")
    )

    $arguments = @("-o", $OutputDirectory, "--no-sound")
    if ($executable) {
        Invoke-Checked $executable $arguments $universalRoot
        return $true
    }

    $cargo = Get-Command cargo -ErrorAction SilentlyContinue
    if ($cargo) {
        Invoke-Checked $cargo.Source (@("run", "--release", "--") + $arguments) $universalRoot
        return $true
    }

    return $false
}

function Assert-ContainsSymbols {
    param(
        [string]$Path,
        [string[]]$Symbols
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Required dump output is missing: $Path"
    }

    $content = Get-Content -LiteralPath $Path -Raw
    foreach ($symbol in $Symbols) {
        if ($content -notmatch ("\b" + [regex]::Escape($symbol) + "\b")) {
            throw "Dump validation failed: '$symbol' is missing from $Path."
        }
    }
}

function Get-BuildNumber {
    param(
        [string]$Path,
        [string]$Property
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return $null
    }

    try {
        $json = Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json
        return [long]$json.$Property
    }
    catch {
        Write-Warning "Unable to read build number from '$Path': $($_.Exception.Message)"
        return $null
    }
}

function Convert-OffsetValue {
    param($Value)

    if ($Value -is [string] -and $Value.StartsWith("0x", [StringComparison]::OrdinalIgnoreCase)) {
        return [Convert]::ToInt64($Value.Substring(2), 16)
    }
    return [long]$Value
}

function Compare-OffsetOutputs {
    param(
        [string]$A2xJsonPath,
        [string]$UniversalJsonPath,
        [Nullable[long]]$A2xBuild,
        [Nullable[long]]$UniversalBuild
    )

    if (-not (Test-Path -LiteralPath $A2xJsonPath) -or -not (Test-Path -LiteralPath $UniversalJsonPath)) {
        Write-Warning "Cross-check skipped because one offsets.json file is missing."
        return
    }

    if ($null -ne $A2xBuild -and $null -ne $UniversalBuild -and $A2xBuild -ne $UniversalBuild) {
        Write-Warning "Cross-check skipped: a2x build $A2xBuild and universal build $UniversalBuild differ."
        return
    }

    $a2x = Get-Content -LiteralPath $A2xJsonPath -Raw | ConvertFrom-Json
    $universal = Get-Content -LiteralPath $UniversalJsonPath -Raw | ConvertFrom-Json
    $mismatches = New-Object System.Collections.Generic.List[string]

    foreach ($moduleProperty in $a2x.PSObject.Properties) {
        $universalModule = $universal.PSObject.Properties[$moduleProperty.Name]
        if ($null -eq $universalModule) {
            continue
        }

        foreach ($offsetProperty in $moduleProperty.Value.PSObject.Properties) {
            $other = $universalModule.Value.PSObject.Properties[$offsetProperty.Name]
            if ($null -eq $other) {
                continue
            }

            $a2xValue = Convert-OffsetValue $offsetProperty.Value
            $universalValue = Convert-OffsetValue $other.Value
            if ($a2xValue -ne $universalValue) {
                $mismatches.Add(("{0}!{1}: a2x=0x{2:X}, universal=0x{3:X}" -f
                    $moduleProperty.Name, $offsetProperty.Name, $a2xValue, $universalValue))
            }
        }
    }

    if ($mismatches.Count -eq 0) {
        Write-Status "The common offsets from both dumpers agree."
        return
    }

    $message = "Dumper cross-check found $($mismatches.Count) mismatch(es): " + ($mismatches -join "; ")
    if ($StrictCrossCheck) {
        throw $message
    }
    Write-Warning $message
}

function New-ProjectPatternsHeader {
    param(
        [string]$PatternsJsonPath,
        [string]$DestinationPath
    )

    if (-not (Test-Path -LiteralPath $PatternsJsonPath -PathType Leaf)) {
        throw "Universal patterns output is missing: $PatternsJsonPath"
    }

    # The universal catalogue uses hexadecimal VA/RVA literals, so extract the
    # canonical wildcard pattern strings instead of treating the file as JSON.
    $lines = Get-Content -LiteralPath $PatternsJsonPath
    $values = @{}
    foreach ($name in $projectPatterns) {
        $nameToken = '"name"\s*:\s*"' + [regex]::Escape($name) + '"'
        $line = $lines | Where-Object { $_ -match $nameToken } | Select-Object -First 1
        if (-not $line) {
            throw "Pattern '$name' was not emitted by the universal dumper."
        }

        $match = [regex]::Match($line, '"pattern"\s*:\s*"([^"]+)"')
        if (-not $match.Success -or [string]::IsNullOrWhiteSpace($match.Groups[1].Value)) {
            throw "Pattern '$name' has no usable wildcard signature."
        }
        $values[$name] = $match.Groups[1].Value
    }

    $body = New-Object System.Collections.Generic.List[string]
    $body.Add("// Generated by tools/Update-Cs2Dumper.ps1 from cs2-universal-offsets.")
    $body.Add("#pragma once")
    $body.Add("")
    $body.Add("namespace cs2_patterns {")
    foreach ($name in $projectPatterns) {
        $body.Add(('    inline constexpr char {0}[] = "{1}";' -f $name, $values[$name]))
    }
    $body.Add("}")
    $body.Add("")

    [System.IO.File]::WriteAllLines($DestinationPath, $body, [System.Text.UTF8Encoding]::new($false))
}

function New-ProjectEngineHeader {
    param(
        [string]$EngineJsonPath,
        [string]$DestinationPath
    )

    if (-not (Test-Path -LiteralPath $EngineJsonPath -PathType Leaf)) {
        throw "Universal engine struct output is missing: $EngineJsonPath"
    }

    $document = Get-Content -LiteralPath $EngineJsonPath -Raw | ConvertFrom-Json
    $viewSetup = $document.structs | Where-Object { $_.name -eq "CViewSetup" } | Select-Object -First 1
    if ($null -eq $viewSetup) {
        throw "Universal engine struct output does not contain CViewSetup."
    }

    $csgoInput = $document.structs | Where-Object { $_.name -eq "CCSGOInput" } | Select-Object -First 1
    if ($null -eq $csgoInput) {
        throw "Universal engine struct output does not contain CCSGOInput."
    }

    $fieldRequests = @(
        @{ Struct = $viewSetup; Key = "CViewSetup_m_vecOrigin"; Field = "m_vecOrigin" },
        @{ Struct = $viewSetup; Key = "CViewSetup_m_angViewAngles"; Field = "m_angViewAngles" },
        @{ Struct = $csgoInput; Key = "CCSGOInput_m_angViewAngles"; Field = "m_angViewAngles" }
    )
    $fields = @{}
    foreach ($request in $fieldRequests) {
        $field = $request.Struct.fields | Where-Object { $_.name -eq $request.Field } | Select-Object -First 1
        if ($null -eq $field) {
            throw "$($request.Struct.name) field '$($request.Field)' is missing from the universal engine struct output."
        }
        $fields[$request.Key] = Convert-OffsetValue $field.offset
    }

    $body = New-Object System.Collections.Generic.List[string]
    $body.Add("// Generated by tools/Update-Cs2Dumper.ps1 from cs2-universal-offsets.")
    $body.Add("#pragma once")
    $body.Add("")
    $body.Add("#include <cstddef>")
    $body.Add("")
    $body.Add("namespace cs2_engine {")
    $body.Add(('    inline constexpr std::ptrdiff_t CViewSetup_m_vecOrigin = 0x{0:X};' -f $fields["CViewSetup_m_vecOrigin"]))
    $body.Add(('    inline constexpr std::ptrdiff_t CViewSetup_m_angViewAngles = 0x{0:X};' -f $fields["CViewSetup_m_angViewAngles"]))
    $body.Add(('    inline constexpr std::ptrdiff_t CCSGOInput_m_angViewAngles = 0x{0:X};' -f $fields["CCSGOInput_m_angViewAngles"]))
    $body.Add("}")
    $body.Add("")

    [System.IO.File]::WriteAllLines($DestinationPath, $body, [System.Text.UTF8Encoding]::new($false))
}

function New-BuildInfoHeader {
    param(
        [Nullable[long]]$A2xBuildNumber,
        [Nullable[long]]$UniversalBuildNumber,
        [string]$DestinationPath
    )

    if ($null -eq $A2xBuildNumber -or $A2xBuildNumber -le 0) {
        throw "The a2x dump did not provide a valid CS2 build number."
    }
    if ($null -eq $UniversalBuildNumber -or $UniversalBuildNumber -le 0) {
        $UniversalBuildNumber = 0
    }

    $body = @(
        "// Generated by tools/Update-Cs2Dumper.ps1 from cs2-dumper info.json.",
        "#pragma once",
        "",
        "#include <cstdint>",
        "",
        "namespace cs2_build_info {",
        ('    inline constexpr std::uint32_t BuildNumber = {0};' -f $A2xBuildNumber),
        ('    inline constexpr std::uint32_t UniversalBuildNumber = {0};' -f $UniversalBuildNumber),
        "}",
        ""
    )
    [System.IO.File]::WriteAllLines($DestinationPath, $body, [System.Text.UTF8Encoding]::new($false))
}

function Publish-FileIfChanged {
    param(
        [string]$SourcePath,
        [string]$DestinationPath
    )

    $sourceHash = (Get-FileHash -LiteralPath $SourcePath -Algorithm SHA256).Hash
    if (Test-Path -LiteralPath $DestinationPath -PathType Leaf) {
        $destinationHash = (Get-FileHash -LiteralPath $DestinationPath -Algorithm SHA256).Hash
        if ($sourceHash -eq $destinationHash) {
            return $false
        }
    }

    $temporaryPath = $DestinationPath + ".new"
    Copy-Item -LiteralPath $SourcePath -Destination $temporaryPath -Force
    Move-Item -LiteralPath $temporaryPath -Destination $DestinationPath -Force
    return $true
}

if (-not (Test-Path -LiteralPath $a2xRoot -PathType Container)) {
    throw "Missing a2x dumper source directory: $a2xRoot"
}
if (-not (Test-Path -LiteralPath $universalRoot -PathType Container)) {
    throw "Missing universal dumper source directory: $universalRoot"
}

$a2xOutput = Join-Path $a2xRoot "output"
$universalOutput = Join-Path $universalRoot "include"
$freshA2x = $false
$freshUniversal = $false

try {
    if (-not $SkipRun) {
        $cs2Process = Get-Process -Name "cs2" -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($cs2Process) {
            New-Item -ItemType Directory -Path $stageRoot -Force | Out-Null

            $a2xStage = Join-Path $stageRoot "a2x"
            $universalStage = Join-Path $stageRoot "universal"
            New-Item -ItemType Directory -Path $a2xStage, $universalStage -Force | Out-Null

            Write-Status "cs2.exe detected; running both dumpers."
            $freshA2x = Invoke-A2xDumper $a2xStage
            $freshUniversal = Invoke-UniversalDumper $universalStage

            if ($freshA2x) {
                $a2xOutput = $a2xStage
            }
            if ($freshUniversal) {
                $universalOutput = $universalStage
            }

            if (-not $freshA2x -or -not $freshUniversal) {
                $missing = @()
                if (-not $freshA2x) { $missing += "cs2-dumper" }
                if (-not $freshUniversal) { $missing += "cs2-universal-offsets" }
                $message = "Could not run $($missing -join ' and '): no release executable or cargo command was found."
                if ($RequireFresh) { throw $message }
                Write-Warning "$message Using the checked-in dumper outputs instead."
            }
        }
        else {
            $message = "cs2.exe is not running; using the existing dumper outputs."
            if ($RequireFresh) { throw $message }
            Write-Status $message
        }
    }
    else {
        Write-Status "Dumper execution skipped; validating and syncing existing outputs."
    }

    foreach ($header in $generatedHeaders) {
        $sourcePath = Join-Path $a2xOutput $header
        if (-not (Test-Path -LiteralPath $sourcePath -PathType Leaf)) {
            throw "a2x output is incomplete; missing $sourcePath"
        }
    }

    $a2xOffsetsHeader = Join-Path $a2xOutput "offsets.hpp"
    $a2xClientHeader = Join-Path $a2xOutput "client_dll.hpp"
    Assert-ContainsSymbols $a2xOffsetsHeader $requiredOffsets
    Assert-ContainsSymbols $a2xClientHeader $requiredSchemaSymbols

    $patternsJson = Join-Path $universalOutput "patterns\patterns.json"
    $generatedPatterns = Join-Path $stageRoot "patterns.hpp"
    $engineStructsJson = Join-Path $universalOutput "engine\engine_structs.json"
    $generatedEngine = Join-Path $stageRoot "engine_structs.hpp"
    if (-not (Test-Path -LiteralPath $stageRoot)) {
        New-Item -ItemType Directory -Path $stageRoot -Force | Out-Null
    }
    New-ProjectPatternsHeader $patternsJson $generatedPatterns
    New-ProjectEngineHeader $engineStructsJson $generatedEngine

    $a2xInfo = Join-Path $a2xOutput "info.json"
    $universalManifest = Join-Path $universalOutput "manifest.json"
    $a2xBuild = Get-BuildNumber $a2xInfo "build_number"
    $universalBuild = Get-BuildNumber $universalManifest "build_number"
    $generatedBuildInfo = Join-Path $stageRoot "build_info.hpp"
    New-BuildInfoHeader $a2xBuild $universalBuild $generatedBuildInfo
    Compare-OffsetOutputs (Join-Path $a2xOutput "offsets.json") `
        (Join-Path $universalOutput "offsets\offsets.json") $a2xBuild $universalBuild

    New-Item -ItemType Directory -Path $destinationRoot -Force | Out-Null
    $changed = New-Object System.Collections.Generic.List[string]
    foreach ($header in $generatedHeaders) {
        if (Publish-FileIfChanged (Join-Path $a2xOutput $header) (Join-Path $destinationRoot $header)) {
            $changed.Add($header)
        }
    }
    if (Publish-FileIfChanged $generatedPatterns (Join-Path $destinationRoot "patterns.hpp")) {
        $changed.Add("patterns.hpp")
    }
    if (Publish-FileIfChanged $generatedEngine (Join-Path $destinationRoot "engine_structs.hpp")) {
        $changed.Add("engine_structs.hpp")
    }
    if (Publish-FileIfChanged $generatedBuildInfo (Join-Path $destinationRoot "build_info.hpp")) {
        $changed.Add("build_info.hpp")
    }

    if ($changed.Count -eq 0) {
        Write-Status "Generated offsets, schemas, interfaces, and project patterns are already current."
    }
    else {
        Write-Status ("Updated {0} generated file(s): {1}" -f $changed.Count, ($changed -join ", "))
    }

    if ($null -ne $a2xBuild) {
        Write-Status "Active a2x CS2 build: $a2xBuild"
    }
}
finally {
    if (Test-Path -LiteralPath $stageRoot) {
        Remove-Item -LiteralPath $stageRoot -Recurse -Force
    }
}
