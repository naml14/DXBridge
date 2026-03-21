@echo off
setlocal
cargo build --manifest-path "%~dp0Cargo.toml" %*
