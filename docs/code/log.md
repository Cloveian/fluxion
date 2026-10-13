# Log

## 2026-10-12 - set up, hello world runs

- CE C toolchain v15 in `~/CEdev`, makefile adds it to PATH itself so VS Code just works
- CEmu is `ti-cemu` on here (`cemu` is the Wii U emulator lol). ROM is my own dump, gitignored, never commit it
- OS 5.8.0 so arTIfiCE 2.1, programs through Cesium
- hello world runs
- CEmu send was being annoying, a relative path silently fails if CEmu is already open from another folder. VS Code configs use `${workspaceFolder}` now
- leaning towards PineappleCAS's engine so the time goes into rendering + input, see [platform](../design-choices/platform.md)

---
Back to [code](index.md)
