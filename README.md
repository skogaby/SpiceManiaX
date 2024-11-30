# SpiceManiaX

A spicy companion for running Dance Dance Revolution on a StepManiaX Dedicated Cabinet.

## What is it?

This is a standalone application which allows a user of `spice2x` playing Dance Dance Revolution on a StepManiaX Dedicated Cabinet to have the following features:

* Full stage and cabinet lighting support, including fully addressable RGB lighting, which maps the Gold Cabinet lights onto StepManiaX hardware
* Full support for stage inputs, with 1000Hz polling
* A touchscreen overlay with menu navigation buttons, a pinpad, and a button to scan a virtual card for each player
* Automatic input and output mapping, without needing to use `spicecfg` (apart from the `test` and `service` buttons)

This is accomplished by querying `spice2x` via `SpiceAPI` to gather the lights data, then using a custom fork of the StepManiaX SDK to output the lights to the cabinet. `SpiceAPI` is also used for sending stage and touchscreen inputs to the game.

NOTE: The speaker and subwoofer lights on a StepManiaX Cabinet are not software controllable, and remain statically always on.

The lights mapping from DDR to SMX is as follows:

* DDR Top Panel -> StepManiaX marquee
* DDR Monitor Side Strips -> StepManiaX Vertical Strips (front)
* DDR Subwoofer Corner Lights -> StepManiax 3-Circle Spotlights (front)
* DDR Stage Corner Lights -> SMX Stage Corners (in an L-shape)
* DDR Stage Panels -> SMX Stage Panels

The only buttons you should need to map in `spicecfg` are the test and service buttons. These should be mappable out of the box in `spicecfg`.

## How do I use it?

### Pre-requisites

* A version of `spice2x` which exposes the DDR Gold Cabinet lights via `SpiceAPI`. This should be anything including or after the `2024-10-29` stable release.
* 64-bit Dance Dance Revolution + 64-bit `spice2x`, with hex edits if necessary, to force it into Gold Cabinet mode (32-bit DDR does not have the right codepaths to emulate the BIO2 lights that the Gold Cab uses).
* A copy of `SpiceManiaX` plus the corresponding `SMX.dll` needed to interact with the cabinet lights.

### Setup and configuration

* Place `SpiceManiaX.exe` and `SMX.dll` in the same directory as your `spice64.exe` and `gamestart.bat`
  * It doesn't really matter whether you use the 32-bit or 64-bit release of `SpiceManiaX`, I've included both though.
* Alter your `gamestart.bat` to invoke `SpiceManiaX.exe` before starting the game.
  * I also recommend setting the core affinity for each program such that they sit on different CPU cores. Otherwise, I've seen issues where DDR drops frames due to resource contention on the same CPU core. I also set the thread priorities to `LOW` and `HIGH`, respectively.
  * `SpiceAPI` also needs to be enabled. The port must be `1337` and the password must be `spicemaniax`.
* `SpiceManiaX` also supports the following parameters:
  * Card ID parameters (`--p1card`/`--p2card`), for configuring the cards that are inserted when pressing the `Insert Card` overlay buttons.
  * Opacity (`--opacity`), a number betwen 0 and 1 which specified how opqaue the overlay should be (0 = fully transparent, 1 = fully opaque, 0.5 = 50% transparent, etc.)

Example `gamestart.bat`:
```
@echo off
cd %~dp0
START /AFFINITY 1 /LOW SpiceManiaX.exe --p1card somecardid --p2card anothercardid --opacity 0.6
START /AFFINITY 2 /HIGH spice64.exe -api 1337 -apipass spicemaniax
```
* Map your test and service buttons via `spicecfg.exe`.
* Go ahead and start the game via `gamestart.bat`. If all goes well, once the game window appears, your pads should turn gold until the game actually starts sending lights outputs.

## FAQ

1. How does this work?
* This works by acting as a middleman between `spice2x` via `SpiceAPI`, and StepManiaX hardware via a [custom fork](https://github.com/skogaby/stepmaniax-sdk) of the StepManiaX SDK.
2. Which models of StepManiaX cabinets does this support?
* This currently only supports StepManiax Dedicated Cabinets. If you have an AIO or a DX cabinet and wish to get support added, shoot me a Discord PM (`skogaby#1337`) and we can possibly work something out.
3. Can I emulate a cabinet other than Gold cabinets with this?
* No, this is explicitly meant for doing Gold Cabinet lights on SMX hardware. If you wish to emulate other cabinet lights, use the `smxdedicab` option in Spice2x, and map the lights via `spicecfg`. This tool exists to get around the limitations in mapping large amounts of RGB lights / tape LEDs in `spicecfg`.
4. Does this support inputs, or only lights?
* This tool supports both inputs and lights!
5. Can I re-map the lighting configuration?
* No. If you wish to change the lights, that's all hardcoded right now and would require you to change the source code.
6. Can I run this directly on a StepManiaX Android PC?
* No. The main use case for this is for those who have a mini windows PC inside their cabinet, along with splitters, etc. so that all the USB hardware is connected directly to the PC that's running DDR. Theoretically this could be ported to Android to work over network like `KFChicken` or something, but that's outside the scope of my work.

## Demos

https://github.com/user-attachments/assets/d408d5e6-45d1-4d17-95e0-6fe8a85b939c

https://github.com/user-attachments/assets/8b280984-7d76-4afd-855e-4556fefa5f8e
