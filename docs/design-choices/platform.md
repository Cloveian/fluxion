# Platform
what language, toolchain and CAS engine

## Identify
I need to decide what to write this in, and what does the actual math

### Relevant constraints/nice to haves:
- TI-84 Plus CE: eZ80 at 48 MHz, 320\*240 colour LCD, ~150 KB of user RAM. malloc only gets ~59 KB though, the toolchain's heap is a fixed window (`BSSHEAP_LOW`/`HIGH` in CEdev's `makefile.mk`), appvars in RAM come out of the rest
- Program size: one program variable tops out around 64 KB
- OS 5.5+ blocks C/asm programs unless jailbroken (arTIfiCE)


### What has to be managed/controlled
- Parsing expressions into a tree
- The actual CAS math (simplify, expand, derivative, ...)
- 2D math rendering
- Input / editor
- Screens, menus, history


### Gates

1. runs on the CE (eZ80), with arTIfiCE where needed
2. fits in program size + RAM
3. buildable in a week

## Brainstorm

**Gate cuts:**

- Rust: no eZ80 target. The CE toolchain uses its own LLVM fork for the eZ80, and rustc would have to be built against it (╯ ͡ಥꔢ ͡ಥ）╯︵ ┻━┻
- Giac / GiNaC / SymEngine: C++ and megabytes, way over the program size limit
- TI-BASIC: too slow for a CAS, and no real control over drawing

**Survivors:**

- **A - C (CEdev) + PineappleCAS engine**
	MIT, and the engine is already its own folder (`src/cas/` + `src/imath/`) with a PC build. last commit was Nov 2022 so it might need fixing for toolchain v15. has simplify, evaluate, substitute, expand, derivative (and a `factor.c` that isn't a command)
- **B - C (CEdev) + own engine**
	full control over the tree and output form, but the ~~meth~~math eats most of the time, and the point is the rendering + input.
- **C - C (CEdev) + 'Hybrid' engine**
	Base on Pineapple, but modify it where needed.

## Select

| Criteria                           | Weight | A: PineappleCAS engine |       B: own engine        | **C: Hybrid** |
| ---------------------------------- | :----: | :--------------------: | :------------------------: | :-----------: |
| Time (10=quick)                     |   8    |           7            |             4              |       6       |
| Control                            |   5    |           4            |             10             |       8       |
| Personal ability (feasibility)     |   6    |           7            |             4              |       6       |
| Good Simplifying                   |   9    |          Idk           | i would need to be special |    depends    |
| **Weighted total**                 |  190   |          118           |            106             |    **124**    |


### Decision
C (CEdev) + Hybrid engine.

---
Back to [design choices](index.md) · [goals](../goals.md)
