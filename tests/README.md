# Testing

DXBridge tests are organized into small CTest labels so local runs and CI can use clearer confidence gates.

- `unit`: fast validation of internal logic and backend-facing validation helpers.
- `integration`: headless end-to-end rendering checks.
- `hardening`: tests that exercise defensive validation and backend contract handling.
- `dx11` / `dx12`: backend-specific coverage.
- `smoke`: the smallest high-signal subset for quick confidence checks.

## Typical commands

Examples:

```powershell
ctest --test-dir build\debug -C Debug --output-on-failure
ctest --test-dir build\debug -C Debug -L smoke --output-on-failure
cmake --build build\debug --config Debug --target check_smoke
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
- `test_headless_triangle_dx11`
- `test_headless_triangle_dx12`
- `smoke_hello_triangle_dx11`

## Related documentation

- `README.md`
- `docs/build-and-test.md`
- `docs/architecture.md`
