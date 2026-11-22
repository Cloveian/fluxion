# Shipping checklist

What has to be done before a release.

## The app
- [ ] `make installer` builds clean (no warnings past the linker's executable stack ones)
- [ ] `FLUXINST` installs it on OS 5.8.0 and it runs from the apps menu
- [ ] Doesn't crash or RAM-reset on nasty inputs (huge exponents, deep nesting, divide by zero)

## README
- [ ] What it is, with screenshots
- [ ] Install steps: arTIfiCE on OS 5.5+, and the [CE C libraries](https://github.com/CE-Programming/libraries/releases) (`graphx` etc. have to be on the calculator too)
- [ ] How to use it (keys)
- [ ] Credit PineappleCAS, keep its MIT licence text

## Release
- [ ] `make release`, then a GitHub release with everything in `release/Fluxion-<version>/` (`Fluxion-TI84CE-TIConnectCE.b84`, `Fluxion-TI84CE-TiLP.tig`, `Fluxion-TI84CE-loose-files.zip` with the loose `FLUXINST.8xp` + `FLUXAPP` appvars, and `Fluxion-Nspire.tns`)
- [ ] Demo video recorded in CEmu (most people don't have a CE)

---
Back to [goals](goals.md)
