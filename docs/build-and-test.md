# Build and Test

## Purpose

This guide covers the publication-ready v1 workflow for configuring, building, testing, and smoke-checking DXBridge on Windows.

## Supported environment

DXBridge v1 targets modern Windows desktop development.

| Requirement | Notes |
| --- | --- |
| OS | Windows 10 or Windows 11 |
| Compiler toolchain | Visual Studio 2022 / MSVC |
| Build generator | CMake `3.20+` |
| Graphics APIs | Direct3D 11 and Direct3D 12 |
| Windows SDK | `10.x`; source notes reference `10.0.22000.0` |

Windowed examples require a normal desktop session with access to Win32 window creation.

## Fastest successful path

From the repository root:

```bat
cmake --preset debug
cmake --build --preset debug
pwsh -File scripts\run-ctest.ps1 -Preset debug
```

Optional smoke-only pass:

```bat
pwsh -File scripts\run-ctest.ps1 -Preset debug -Label smoke
cmake --build --preset debug --target check_smoke
cmake --build --preset debug --target check_abi
```

## CMake presets

The repository defines three presets in `CMakePresets.json`:

| Preset | Configuration | Purpose |
| --- | --- | --- |
| `debug` | Visual Studio 2022, x64, Debug | local development and debugging |
| `release` | Visual Studio 2022, x64, Release | optimized local build |
| `ci` | Visual Studio 2022, x64, Release | CI-style build with matching `ctest --preset ci` |

Each preset now writes into `out/build/<preset>` so configure and generated Visual Studio files do not spill into the repository root.

## Configure and build

### Debug

```bat
cmake --preset debug
cmake --build --preset debug
```

Outputs land under `out\build\debug\`.

### Release

```bat
cmake --preset release
cmake --build --preset release
```

Outputs land under `out\build\release\`.

### CI-style build

```bat
cmake --preset ci
cmake --build --preset ci
```

Outputs land under `out\build\ci\`.

## Expected outputs

After a successful build, expect:

- `dxbridge.dll` as the primary shared library output
- backend/static libraries used to compose the DLL
- test executables under the selected build tree
- example executables such as `hello_triangle.exe` and `hello_triangle_dx12.exe`

Build artifacts are intentionally excluded from version control through `.gitignore`.

## Compiler policy

`cmake/CompilerFlags.cmake` enables a strict MSVC configuration for v1:

- `/W4`
- `/WX`
- `/std:c++17`
- `/utf-8`
- `/permissive-`

Warnings are treated as errors, so a successful build implies the active target stayed warning-clean.

## Running tests

### Full suite

```bat
pwsh -File scripts\run-ctest.ps1 -Preset debug
```

### Smoke subset

```bat
pwsh -File scripts\run-ctest.ps1 -Preset debug -Label smoke
cmake --build --preset debug --target check_smoke
```

### CI preset tests

```bat
pwsh -File scripts\run-ctest.ps1 -Preset ci
```

Use the PowerShell wrapper when possible on Windows so Git Bash and PowerShell invoke the same preset-based `ctest` path.

## Version verification

Before cutting or validating a release, re-check the synchronized version metadata:

```bat
pwsh -File scripts\verify-version.ps1 -SkipBinaryCheck
```

After building a preset, you can also verify the generated DLL export version:

```bat
pwsh -File scripts\verify-version.ps1 -Preset release -Configuration Release
```

The same script now checks the synchronized version markers in:

- `version.txt`
- `CMakeLists.txt`
- `include/dxbridge/dxbridge_version.h`
- `.release-please-manifest.json`
- `README.md`
- `docs/api-reference.md`

For the closed `v1.3.0` release branch, keep `version.txt`, `CMakeLists.txt`, `include/dxbridge/dxbridge_version.h`, `.release-please-manifest.json`, `README.md`, and `docs/api-reference.md` synchronized at `1.3.0`.

## Runtime capability validation

For the `v1.3.0` compatible-growth line, the fastest cross-runtime preflight check for the additive capability API is:

```bat
pwsh -File scripts\run-runtime-validation.ps1 -Dll out\build\debug\Debug\dxbridge.dll
```

By default this validates the main maintained runtimes:

- Node.js
- Python
- Go
- Rust

It reuses `examples/run_capability_preflight.ps1`, so the same backend flags and explicit DLL override can be used for local or CI validation.

## `v1.3.0` release validation ladder

Recommended order when validating the compatible minor before publication:

1. `pwsh -File scripts\verify-version.ps1 -SkipBinaryCheck`
2. `cmake --build --preset debug --target check_abi`
3. `pwsh -File scripts\run-ctest.ps1 -Preset debug -Label smoke`
4. `pwsh -File scripts\run-runtime-validation.ps1 -Dll out\build\debug\Debug\dxbridge.dll`
5. review `CHANGELOG.md`, `docs/releases/v1.3.0-release-notes.md`, and `docs/releases/v1.3.0-publishing-checklist.md` together

## Test organization

CTest labels in v1 include:

- `unit`
- `integration`
- `hardening`
- `dx11`
- `dx12`
- `smoke`
- `core`
- `example`
- `abi`

Representative coverage:

- `test_handle_table`
- `test_error_mapping`
- `test_dx11_validation`
- `test_dx12_validation`
- `test_headless_triangle_dx11`
- `test_headless_triangle_dx12`
- `smoke_hello_triangle_dx11`

For a shorter label-oriented view, see `tests/README.md`.

## Example validation commands

These commands validate the published example paths against a built DLL.

### Native C++

```bat
out\build\debug\Debug\hello_triangle.exe --self-test
out\build\debug\Debug\hello_triangle_dx12.exe
```

### Bun

```bat
bun run examples/bun/example01_load_version_logs.ts
bun run examples/bun/example02_enumerate_adapters.ts
bun run examples/bun/example03_create_device_errors.ts --debug
bun run examples/bun/example04_dx11_clear_window.ts --hidden --frames 3
bun run examples/bun/example05_dx11_triangle.ts --hidden --frames 3 --sync-interval 0
```

### Node.js

```bat
npm --prefix examples/nodejs ci
node examples/nodejs/example01_load_version_logs.js
node examples/nodejs/example02_enumerate_adapters.js
node examples/nodejs/example03_create_device_errors.js --debug
node examples/nodejs/example04_dx11_clear_window.js --hidden --frames 3
node examples/nodejs/example05_dx11_triangle.js --hidden --frames 3 --sync-interval 0
```

### Other runtimes

See the runtime-specific READMEs for Go, Python, C#, Java, and Rust under `examples/`.

Reproducibility notes:

- Bun examples do not require a package install step because they only use Bun built-ins plus the checked-in `.ts` sources.
- Node.js examples commit `examples/nodejs/package-lock.json`, so `npm ci` is the preferred install path for repeatable local and CI runs.

## Validation ladder

When diagnosing a new machine or a packaging issue, use this order:

1. build the DLL with `cmake --build --preset debug`
2. run `pwsh -File scripts\run-ctest.ps1 -Preset debug`
3. run the native DX11 smoke example
4. run one non-windowed language example such as Bun or Node.js example 01
5. run one hidden windowed example with `--frames 3`

This sequence isolates whether the issue is in the core build, the native runtime, or a specific binding layer.

## Common failure causes

- CMake, MSVC, or the Windows SDK is missing or not on `PATH`
- `dxbridge.dll` was not built before running examples
- the process is not running inside a normal Windows desktop session
- runtime-specific dependencies are missing, such as `koffi` for Node.js or JNA jars for Java
- the requested backend or adapter is unavailable on the current machine
- DX11 shader compilation support is unavailable on the current machine

## Related documentation

- `README.md`
- `docs/architecture.md`
- `docs/examples.md`
- `docs/releases/v1.3.0-publishing-checklist.md`
- `tests/README.md`
