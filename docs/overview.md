# Fluxion

## What is this project?
A CAS for the TI-84 Plus CE.

## Tools
- C, built with the [CE C toolchain](https://github.com/CE-Programming/toolchain) v15.0
- Libraries: `graphx` (drawing), `keypadc` (keys), `fontlibc` (fonts, if needed)
- CAS engine: PineappleCAS's `src/cas/` + `src/imath/` (see [platform](design-choices/platform.md))
- Emulator: CEmu v2.0 (`ti-cemu`)
- Jailbreak: arTIfiCE 2.1 (OS 5.8.0), programs launched through Cesium

## Background

---

The goals, constraints, and design decisions live in the [design doc](goals.md).
