# Repository Guidelines

## Project Structure & Module Organization

- `Main/` contains the DLL entry point, DirectX/ImGui hook, menu, memory helpers, and cheat features.
- `imgui_d11/` is the vendored Dear ImGui core and Win32/DX11 backends; change it only when updating the dependency.
- `dumper/` contains generated CS2 offsets, schemas, and interface headers. Treat these files as generated inputs and update them together when offsets change.
- `minhook_debug_x64/` contains the MinHook header and x64 library used by the project.
- `Sero draw.vcxproj` is the Visual Studio project. `x64/`, `Debug/`, and `Release/` are generated build output and are ignored by Git.

## Build, Test, and Development Commands

Run these from a Visual Studio 2022 Developer PowerShell (with MSBuild and the Windows 10 SDK available):

```powershell
msbuild "Sero draw.vcxproj" /m /p:Configuration=Release /p:Platform=x64
msbuild "Sero draw.vcxproj" /m /p:Configuration=Debug /p:Platform=x64
```

`Release|x64` is the supported DLL build and links D3D11, DXGI, and MinHook. Use `Debug|x64` for diagnostics; select the matching configuration in Visual Studio when debugging. There is no automated test suite or repository lint command.

## Coding Style & Naming Conventions

Use C++20, four-space indentation, braces on the same line, and UTF-8 source encoding. Keep headers and implementations paired (`feature.hpp`/`feature.cpp`). Use `PascalCase` for types and namespaces, `camelCase` for functions and locals, and the existing `b`-prefixed style for boolean menu settings (for example, `bMainESP`). Prefer the existing helpers in `Main/core`, `Main/utils`, and ImGui wrappers over new duplicated access code.

## Testing Guidelines

No unit or integration tests are currently configured. Before submitting changes, build `Release|x64` and smoke-test the affected hook/menu/feature in an authorized test environment. For UI changes, verify every affected menu tab and include a screenshot in the pull request.

## Commit & Pull Request Guidelines

History uses short, descriptive subjects, often in Chinese, without a strict Conventional Commits prefix. Keep commits focused and describe the behavior or offset change. Pull requests should include a summary, affected configuration/platform, validation performed, and linked issue when applicable. Call out regenerated `dumper/` headers explicitly; do not commit build output or user-specific Visual Studio files.

## Security & Configuration Tips

Never commit game binaries, private offsets, credentials, or local debugger settings. Keep generated offsets synchronized with the target game build, and use this DLL only where modification/injection is explicitly permitted.
