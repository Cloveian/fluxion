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
- [ ] GitHub release with `Fluxion-app.b84` (or `FLUXINST.8xp` + the six `FLUXAPP` appvars)
- [ ] Demo video recorded in CEmu (most people don't have a CE)

---
Back to [goals](goals.md)
