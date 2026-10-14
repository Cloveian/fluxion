# Installer

Fluxion is a Flash App, so it needs an installer: a small program that copies the app from some appvars into flash. you run it once.

I'm using [CEaShell-Installer](https://github.com/RoccoLoxPrograms/CEaShell-Installer) by RoccoLox Programs (with TIny_Hacker), the installer from [CEaShell](https://github.com/RoccoLoxPrograms/CEaShell). It's a git submodule in `ceashell-installer/`, pinned to one commit and not modified.

Licence stuff: the installer repo doesn't have its own LICENSE file at that commit, CEaShell is GPL-3.0. Fluxion itself is MIT, the installer is a separate program (`FLUXINST.8xp`) that just ships next to it, none of its code goes into Fluxion.

## Config

It didn't need any code changes, its makefile takes make variables and Fluxion's `makefile` passes them in:

| Variable            | Value               | What                                     |
|---------------------|---------------------|------------------------------------------|
| `NAME`              | `FLUXINST`          | installer program name                   |
| `DESCRIPTION`       | `Fluxion Installer` | what shells show                         |
| `ICON`              | Fluxion's `icon.png`| icon shells show                         |
| `APPVAR_PREFIX`     | `FLUXAPP`           | appvars are `FLUXAPP0`, `FLUXAPP1`, ...  |
| `APPVAR_SPLIT_SIZE` | `65200`             | bytes per appvar (same as CEaShell)      |

## Build

```sh
git submodule update --init   # once, after cloning
make installer
```

That builds Fluxion as an app (`bin-app/Fluxion.8ek`), builds the installer in a copy (`build-installer/`, so the submodule never gets touched), and puts the stuff to send in `bin-installer/`:

- `FLUXINST.8xp` the installer
- `FLUXAPP.0.8xv`, `FLUXAPP.1.8xv`, ... the app split into appvars

Send all of them and run `FLUXINST`.

## Updating the pinned commit

```sh
git -C installer/ceashell-installer fetch
git -C installer/ceashell-installer checkout <commit>   # e.g. origin/main
git add installer/ceashell-installer
make installer
```

It builds whatever commit is staged, so `git add` it first. Also check upsteam's makefile still has the same variables.
