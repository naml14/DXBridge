# Changelog

All notable changes to this project will be documented in this file.

The format is inspired by Keep a Changelog and the project uses semantic versioning from `include/dxbridge/dxbridge_version.h` and `CMakeLists.txt`.

## [1.3.1](https://github.com/naml14/DXBridge/compare/v1.3.0...v1.3.1) (2026-03-27)


### Bug Fixes

* corregir lectura fuera de límites en UploadData y validar offsets en SetVertexBuffer/SetIndexBuffer ([096dd3d](https://github.com/naml14/DXBridge/commit/096dd3df9d8bc0d117774ec554f5fb6470ce5bc6))
* corregir lectura fuera de límites en UploadData y validar offsets en SetVertexBuffer/SetIndexBuffer ([d92ccbc](https://github.com/naml14/DXBridge/commit/d92ccbcfa03e442a633a0b00d9f2cd0177e3e2c3))

## [1.3.0](https://github.com/naml14/DXBridge/compare/v1.2.0...v1.3.0) (2026-03-22)


### Features

* add build and upload steps for release artifacts in CI workflow ([e1b5d1a](https://github.com/naml14/DXBridge/commit/e1b5d1a8ca721c5a8cd2eb3a56e5b1c7cd23fda9))
* add CI and security review workflows for pull requests ([bf77f5e](https://github.com/naml14/DXBridge/commit/bf77f5e2728a8fc389adc7a9420f404c12f5db34))
* add release automation configuration and workflows ([ae3eaf6](https://github.com/naml14/DXBridge/commit/ae3eaf6248abf625d16468c4500872a3725f888e))
* initialize dxbridge v1 repository with 8 different languages examples and full documentation ([97a736d](https://github.com/naml14/DXBridge/commit/97a736d1c4c04fc4fe166dbb3296970f8275e483))
* release dxbridge v1.3.0 with capability discovery ([56db925](https://github.com/naml14/DXBridge/commit/56db925766b5c494485a1682efb21463a35280a0))
* release dxbridge v1.3.0 with capability discovery, integration examples, and validation tooling ([e22ce44](https://github.com/naml14/DXBridge/commit/e22ce44c45506508732c0a67bb37947e52b4bb9a))
* update CI workflows to use actions/checkout@v6 and actions/github-script@v8; add configuration files for AI agent guidelines ([301a42b](https://github.com/naml14/DXBridge/commit/301a42b7c91db998f035c3e36ec9b5af857778c1))


### Bug Fixes

* add GH_REPO environment variable to release notes update step ([54cbd60](https://github.com/naml14/DXBridge/commit/54cbd60ceaeeae0317144dcde2c2f342514c1304))
* format prompt output for security review diff in workflow ([a2fb143](https://github.com/naml14/DXBridge/commit/a2fb143899dd2cda5fec8c38d34d3ddb9b2fe536))
* update release notes formatting to include markdown fence ([930b2ab](https://github.com/naml14/DXBridge/commit/930b2abc99fe32d303a6b5683443c757b8b8fce0))

## [1.2.0](https://github.com/naml14/DXBridge/compare/v1.1.2...v1.2.0) (2026-03-22)


### Features

* add build and upload steps for release artifacts in CI workflow ([e1b5d1a](https://github.com/naml14/DXBridge/commit/e1b5d1a8ca721c5a8cd2eb3a56e5b1c7cd23fda9))

## [1.1.2](https://github.com/naml14/DXBridge/compare/v1.1.1...v1.1.2) (2026-03-22)


### Bug Fixes

* add GH_REPO environment variable to release notes update step ([54cbd60](https://github.com/naml14/DXBridge/commit/54cbd60ceaeeae0317144dcde2c2f342514c1304))

## [1.1.1](https://github.com/naml14/DXBridge/compare/v1.1.0...v1.1.1) (2026-03-22)


### Bug Fixes

* update release notes formatting to include markdown fence ([930b2ab](https://github.com/naml14/DXBridge/commit/930b2abc99fe32d303a6b5683443c757b8b8fce0))

## [1.1.0](https://github.com/naml14/DXBridge/compare/v1.0.1...v1.1.0) (2026-03-22)


### Features

* add CI and security review workflows for pull requests ([bf77f5e](https://github.com/naml14/DXBridge/commit/bf77f5e2728a8fc389adc7a9420f404c12f5db34))
* add release automation configuration and workflows ([ae3eaf6](https://github.com/naml14/DXBridge/commit/ae3eaf6248abf625d16468c4500872a3725f888e))
* initialize dxbridge v1 repository with 8 different languages examples and full documentation ([97a736d](https://github.com/naml14/DXBridge/commit/97a736d1c4c04fc4fe166dbb3296970f8275e483))


### Bug Fixes

* format prompt output for security review diff in workflow ([a2fb143](https://github.com/naml14/DXBridge/commit/a2fb143899dd2cda5fec8c38d34d3ddb9b2fe536))

## [Unreleased]

Prepared release payload for the compatible `1.3.0` minor.

### Public additive value

- add the `DXBridge_QueryCapability()` pre-init capability-discovery family with `DXBCapabilityQuery`, `DXBCapabilityInfo`, and additive capability IDs
- add wrapper/helper coverage for capability discovery in Bun, Node.js, Python, Go, and Rust bindings
- add aligned `example06_capability_preflight` onboarding samples plus the shared `examples/run_capability_preflight.ps1` launcher

### Internal hardening

- strengthen ABI and compatibility validation with `test_public_abi_contract` and capability-focused regression coverage
- add runtime validation tooling through `scripts/run-runtime-validation.ps1` to smoke the maintained language bindings against one DLL
- reinforce validation and CI guidance around release confidence without changing the base backend or rendering model

### Documentation and release work

- update the README, API reference, examples guide, integration guide, and language-specific example docs to explain the compatible `v1.3.0` story clearly
- add release-ready notes in `docs/releases/v1.3.0-release-notes.md` and a publication checklist in `docs/releases/v1.3.0-publishing-checklist.md`
- keep the release message split between public additive value, internal hardening, and documentation or release-process work

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
