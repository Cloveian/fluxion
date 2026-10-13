# Fluxion

The working design doc. Project context and tools live in the [overview](overview.md).

## Goals & constraints
listed roughly from hardest constraints down to less necessary stuff:

### Must haves
- **Really, really good simplifying - better than the Nspire CAS** (#1 priority)
- Run on a TI-84 Plus CE (with arTIfiCE on OS 5.5+)
- Do real CAS stuff: at least simplify, expand, derivative

### Must haves in order to be better than PineappleCAS
- 2D math rendering
- Own input, not the OS's
- Intuitive, nothing to memorise

### Nice to haves
- LaTeX-looking output

### Nice to have, but slightly far-fetched (harder to achieve)
- Nya\~\~


## Design Choices

- [Platform](design-choices/platform.md) - C (CEdev) + Hybrid (Modified pineappleCAS) engine
- [Rendering](design-choices/rendering.md) - Box thing (steal LaTeX spacing)
- [Input](design-choices/input.md) -
- [Screens & operations](design-choices/ui.md) -

## Building it
- [Code](code/index.md) - layout, testing, dated log
- [Shipping checklist](shipping-checklist.md) - what has to be done before a release
