# CS2 dump update

`Update-Cs2Dumper.ps1` keeps the project's compile-time offsets, schemas,
interfaces, signatures, and selected engine layouts in sync with the two local
dumper repositories.

## Automatic build update

Every x64 build runs the updater before C++ compilation. When `cs2.exe` is
running, the updater prefers an existing release executable and falls back to
`cargo run --release`. When the game is not running, it validates and reuses
the existing dumper outputs.

The generated files are replaced only after required project symbols have been
validated. `cs2-dumper` is the compatibility source for `dumper/*.hpp`;
`cs2-universal-offsets` supplies the project signatures and non-schema engine
layouts, and cross-checks common offsets when both outputs target the same CS2
build.

## Commands

Run a normal update while CS2 is open:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\Update-Cs2Dumper.ps1
```

Sync already generated output without attaching to CS2:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\Update-Cs2Dumper.ps1 -SkipRun
```

Require both dumpers to produce fresh output and fail on common-offset
disagreement:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\Update-Cs2Dumper.ps1 -RequireFresh -StrictCrossCheck
```

Disable the build hook for one build:

```powershell
msbuild "Sero draw.vcxproj" /m /p:Configuration=Release /p:Platform=x64 /p:RunCs2DumperBeforeBuild=false
```

The built DLL verifies the live `engine2.dll` build number against
`dumper/build_info.hpp`. Features stay disabled when the game and compiled dump
do not match. Features that consume universal signatures or engine layouts also
stay disabled until the universal and a2x outputs report the same CS2 build.
