# Changelog

All notable changes to this project will be documented in this file.

The format is inspired by Keep a Changelog and the project uses semantic versioning from `include/dxbridge/dxbridge_version.h` and `CMakeLists.txt`.

## [1.1.0](https://github.com/naml14/DXBridge/compare/v1.0.1...v1.1.0) (2026-03-22)


### Features

* add CI and security review workflows for pull requests ([bf77f5e](https://github.com/naml14/DXBridge/commit/bf77f5e2728a8fc389adc7a9420f404c12f5db34))
* add release automation configuration and workflows ([ae3eaf6](https://github.com/naml14/DXBridge/commit/ae3eaf6248abf625d16468c4500872a3725f888e))
* initialize dxbridge v1 repository with 8 different languages examples and full documentation ([97a736d](https://github.com/naml14/DXBridge/commit/97a736d1c4c04fc4fe166dbb3296970f8275e483))


### Bug Fixes

* format prompt output for security review diff in workflow ([a2fb143](https://github.com/naml14/DXBridge/commit/a2fb143899dd2cda5fec8c38d34d3ddb9b2fe536))

## [Unreleased]

- No unreleased changes recorded yet.

## [1.0.1] - 2026-03-21

Patch release focused on public version consistency and release-documentation alignment.

### Changed

- synchronized repository-level version references to `1.0.1` across release metadata and published docs
- aligned the README, API reference, and example index docs with the current patch release

### Notes

- `1.0.1` preserves the DXBridge v1 public ABI and focuses on documentation/versioning corrections

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
