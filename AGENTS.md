# AI Agent Guidelines for DXBridge

This file provides context, rules, and workflows for AI agents and assistants working in the DXBridge repository. If you are an AI, you must read and adhere to these guidelines before suggesting or applying changes.

## 1. Project Architecture & Tech Stack

**DXBridge** is a Windows-native Direct3D bridge distributed as a dynamic library (`dxbridge.dll`).

- **Implementation:** C++17 (`src/common/`, `src/dx11/`, `src/dx12/`).
- **Interface:** Stable C ABI (`include/dxbridge/dxbridge.h`, `src/dxbridge.cpp`).
- **Platform:** Windows 10/11, Visual Studio 2022 / MSVC toolchain.
- **Backends:** Direct3D 11 and Direct3D 12.
- **Bindings:** The C ABI is meant to be portable across multiple runtimes without generated C++ wrappers (examples include Bun, Node.js, Go, Python, C#, Java, Rust).

## 2. Core Architectural Mandates

- **Strict C ABI:** Never expose C++ classes, standard library containers (`std::string`, `std::vector`), or raw COM pointers (`ID3D11Device*`, `ID3D12Device*`) through the public API boundary.
- **Opaque Handles:** All internal resources are exposed to the consumer as 64-bit encoded handles. This allows generation counting and stale-handle detection. The logic lives in `src/common/handle_table.hpp`.
- **Struct Versioning:** Every public struct in the ABI must carry `uint32_t struct_version` at offset 0 (e.g., initialized with `DXBRIDGE_STRUCT_VERSION`).
- **Thread-Local Errors:** Public API functions return a minimal `DXBResult`. Detailed error messages or HRESULTs must be stored in thread-local storage, retrievable via `DXBridge_GetLastError()` or `DXBridge_GetLastHRESULT()`.
- **Backend Abstraction:** The `IBackend` interface in `src/common/backend.hpp` defines the contract for DX11 and DX12. `src/dxbridge.cpp` simply forwards calls to the currently active process-wide backend.

## 3. Code Style & Conventions

- **C++ Standards:** Use C++17.
- **Compiler Warnings:** The build runs with strict MSVC flags (`/W4`, `/WX`, `/permissive-`, `/utf-8`). Treat all warnings as errors. Ensure any new code builds warning-clean.
- **Naming Conventions:**
  - Exported C functions must be prefixed with `DXBridge_` (e.g., `DXBridge_Init`).
  - Internal C++ classes use PascalCase (e.g., `DX11Backend`, `HandleTable`).
  - C++ member variables should generally follow existing repo conventions (e.g., `m_` prefix or similar, check local context).
  - Public ABI types should be prefixed with `DXB` (e.g., `DXBResult`, `DXBBackend`).
- **Log Callbacks:** Use internal logging functions defined in `src/common/log.hpp` (`DXB_LOG_INFO`, `DXB_LOG_ERROR`) which forward to the consumer's registered log callback. Never use `std::cout` or `printf` in the core DLL.

## 4. Build & Testing Workflow

### Build System

DXBridge uses CMake `3.20+` with Presets. Do not run `cmake` commands manually without presets.

- **Debug:** `cmake --preset debug` -> `cmake --build --preset debug`
- **Release:** `cmake --preset release` -> `cmake --build --preset release`
- **CI:** `cmake --preset ci` -> `cmake --build --preset ci`
Build outputs land in `out/build/<preset>/`.

### Testing

- Tests are managed by CTest and run via PowerShell scripts to ensure consistent environments.
- **Run all tests:** `pwsh -File scripts\run-ctest.ps1 -Preset debug`
- **Run smoke tests:** `pwsh -File scripts\run-ctest.ps1 -Preset debug -Label smoke`
- Tests mix unit validation (handles, errors), backend constraints, and integration rendering tests (headless triangles).

### Example Validation

The repository treats cross-language examples as first-class citizens. When changing the ABI, ensure compatibility with all examples (`examples/bun`, `examples/nodejs`, `examples/python`, etc.).

- Validate examples via `pwsh -File scripts\run-runtime-validation.ps1 -Dll out\build\debug\Debug\dxbridge.dll`.

## 5. Git & Release Workflow

- **Release Please Automation:** The project uses `googleapis/release-please-action` to manage version tags and changelogs.
- **Conventional Commits:** All commit messages MUST follow conventional commits standard (`feat:`, `fix:`, `docs:`, `chore:`, `refactor:`, `feat!:`, etc.) to trigger appropriate semver bumps.
- **Version Synchronization:** Delegated to Github workflow

## 6. Guidelines for AI Modifying Files

1. **Self-Verification Loop:** Before presenting changes to the user, ensure you have reviewed the code to see if it violates the C ABI boundary.
2. **Path Usage:** Always use absolute paths or paths relative to the root explicitly when reading/writing files.
3. **No Proactive File Creation:** Unless directed by the user, do not proactively create new `.md` files or structural overhauls. Keep changes scoped to the task.
4. **Backend Parity:** If you are asked to add a feature (e.g., a new pipeline state configuration), remember that it often needs implementing in both `src/dx11/` and `src/dx12/`, and the ABI must be updated in `src/dxbridge.cpp`.
