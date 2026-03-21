# Changelog

All notable changes to this project will be documented in this file.

The format is inspired by Keep a Changelog and the project uses semantic versioning from `include/dxbridge/dxbridge_version.h` and `CMakeLists.txt`.

## [Unreleased]

- No unreleased changes recorded yet.

## [1.0.0] - 2026-03-21

First publication-ready DXBridge baseline.

### Added

- stable C ABI for Direct3D interop in `include/dxbridge/dxbridge.h`
- semantic version macros in `include/dxbridge/dxbridge_version.h`
- shared runtime infrastructure for handle management, logging, and error handling
- Direct3D 11 backend implementation
- Direct3D 12 backend implementation
- exported dispatch layer in `src/dxbridge.cpp`
- CMake-based build with Visual Studio 2022 presets
- CTest-based unit, hardening, smoke, and integration coverage
- native Win32 hello-triangle examples for DX11 and DX12
- multi-language examples including Bun, Node.js, Go, Python, C#, Java, and Rust
- repository-level documentation for architecture, API reference, build/test workflow, and examples

### Notes

- `1.0.0` establishes the initial documented API and repository structure
- Bun and Node.js are documented as first-class interop examples, not experimental notes
- build outputs and local tooling artifacts are expected to remain untracked through the root `.gitignore`
