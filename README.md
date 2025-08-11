# DanT.Synth

A VCV Rack plugin by DanT.

[![Maintenance](https://img.shields.io/badge/Maintained%3F-yes-green.svg)](https://github.com/Miff-Real/DanT.Synth/commits/main/)
[![GitHub license](https://img.shields.io/github/license/Miff-Real/DanT.Synth.svg)](https://github.com/Miff-Real/DanT.Synth/blob/main/LICENSE)
[![GitHub release](https://img.shields.io/github/release/Miff-Real/DanT.Synth.svg)](https://github.com/Miff-Real/DanT.Synth/releases/)
[![GitHub latest commit](https://badgen.net/github/last-commit/Miff-Real/DanT.Synth)](https://github.com/Miff-Real/DanT.Synth/commit/)
[![GitHub issues-open](https://badgen.net/github/open-issues/Miff-Real/DanT.Synth)](https://github.com/Miff-Real/DanT.Synth/issues?q=is%3Aopen)

## Modules

* [AOCR](docs/aocr.md) - `[5HP][Polyphonic]` Attenuverter & Offset & Clip & Rectify. Reorderable.

## Tests on Windows

* In order ot run the tests in a Windows environment (MSys2 MinGW64) you will need to install `nanovg`

```
pacman -S mingw-w64-x86_64-nanovg
```

* This should install `libnanovg.a` to the path `/mingw64/lib`.

* You will also need to check that the path to `libRack.dll` is correct in the `tests.mk` file

```
	RACK_APP_DIR = "/c/Program Files/VCV/Rack2Pro"
```

* If this is correct, then you should be able to run the tests from the directory that contains `tests.mk` with

```
make test
```
