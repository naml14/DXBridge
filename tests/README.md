# Testing

DXBridge tests are organized into small CTest labels so local runs and CI can use clearer confidence gates.

- `unit`: fast validation of internal logic and backend-facing validation helpers.
- `integration`: headless end-to-end rendering checks.
- `hardening`: tests that exercise defensive validation and backend contract handling.
- `abi`: checks that exported ordinals, handle widths, and public struct assumptions stay stable.
- `dx11` / `dx12`: backend-specific coverage.
- `smoke`: the smallest high-signal subset for quick confidence checks.

## Typical commands

Examples:

```powershell
pwsh -File scripts\run-ctest.ps1 -Preset debug
pwsh -File scripts\run-ctest.ps1 -Preset debug -Label smoke
cmake --build --preset debug --target check_smoke
cmake --build --preset debug --target check_abi
```

## How to use the labels

- Run the full suite before cutting a release or publishing a doc update tied to behavior.
- Run `-L smoke` for the fastest high-signal local confidence pass.
- Filter by `dx11` or `dx12` when diagnosing backend-specific regressions.
- Use `integration` when you need end-to-end rendering coverage without reading every test target individually.

## Representative targets

- `test_handle_table`
- `test_error_mapping`
- `test_dx11_validation`
- `test_dx12_validation`
- `test_capability_discovery`
- `test_public_abi_contract`
- `test_headless_triangle_dx11`
- `test_headless_triangle_dx12`
- `smoke_hello_triangle_dx11`

## Related documentation

- `README.md`
- `docs/build-and-test.md`
- `docs/architecture.md`

## v1.3.0 capability-growth focus

For the compatible `v1.3.0` workstream, the most relevant regression targets are:

- `test_capability_discovery` - validates pre-init capability queries, adapter-scoped queries, and the distinction from `DXBridge_SupportsFeature()`
- `test_public_abi_contract` - validates the additive export ordinal, capability constants, and public struct layout assumptions
