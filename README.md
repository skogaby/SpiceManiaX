# SpiceManiaX

A spicy companion for running Dance Dance Revolution on a StepManiaX Dedicated Cabinet.

## What is it?

This is a standalone application which allows a user of spice2x to emulate the cabinet lights from a DDR 20th Anniverary Gold Cabinet directly on a StepManiaX Dedicated Cabinet. This includes fully addressable RGB lighting on the cabinet and marquee, full RGB control of the pad lighting (though DDR sticks to red/blue by default), support for the pad corner lights, etc. without having to manually map the lights via `spicecfg`, as well as getting the entirety of each LED strip, rather than just the average color of each strip.

This is accomplished by querying spice2x via `SpiceAPI` to gather the lights data, then using a custom fork of the StepManiaX SDK to output the lights to the cabinet.

NOTE: The speaker and subwoofer lights on a StepManiaX Cabinet are not software controllable, and remain statically always on. Therefore, the lights mapping is as follows:

* DDR Top Panel -> StepManiaX marquee
* DDR Monitor Side Strips -> StepManiaX Vertical Strips (front)
* DDR Subwoofer Corner Lights -> StepManiax 3-Circle Spotlights (front)
* DDR Stage Corner Lights -> SMX Stage Corners (in an L-shape)
* DDR Stage Panels -> SMX Stage Panels

## How do I use it?

### Pre-requisites

* A version of spice2x which exposes the DDR Gold Cabinet lights via `SpiceAPI`. As of this writing, this is the `2024-10-29` beta, or anything newer.
* 64-bit DDR + 64-bit spice2x, with hex edits if necessary, to force it into Gold Cabinet mode (32-bit DDR does not have the right codepaths to emulate the BIO2 lights that the Gold Cab uses).
  * If you wish to play 32-bit DDR using White Cab lights, `spice2x` has an option called `smxdedicab` which allows you to map the lights via `spicecfg`. However, you can't use this method for getting the full RGB lights from a Gold Cabinet. You do not need `SpiceManiaX` for that approach, though.
* A copy of `SpiceManiaX` plus the corresponding `SMX.dll` needed to interact with the cabinet lights.

### Setup and configuration

* Place `SpiceManiaX.exe` and `SMX.dll` in the same directory as your `spice64.exe` and `gamestart.bat`
  * It doesn't really matter whether you use the 32-bit or 64-bit release of `SpiceManiaX`, I've included both though.
* Alter your `gamestart.bat` to invoke `SpiceManiaX.exe` before starting the game.
  * I also recommend setting the core affinity for each program such that they sit on different CPU cores. Otherwise, I've seen issues where DDR drops frames due to resource contention on the same CPU core. I also set the thread priorities to `LOW` and `HIGH`, respectively.
  * `SpiceAPI` also needs to be enabled. The port must be `1337` and the password must be `spicemaniax`.

Example `gamestart.bat`:
```
@echo off
cd %~dp0
START /AFFINITY 1 /LOW SpiceManiaX.exe
START /AFFINITY 2 /HIGH spice64.exe -api 1337 -apipass spicemaniax
```
* Map your stage inputs via `spicecfg.exe` as normal (this tool only handles lights, not inputs).
* Go ahead and start the game via `gamestart.bat`. If all goes well, once the game window appears, your pads should turn gold until the game actually starts sending lights outputs.

## FAQ

1. How does this work?
* This works by querying spice2x for the lights data via `SpiceAPI`, and then outputting the data through a [custom fork](https://github.com/skogaby/stepmaniax-sdk) of the StepManiaX SDK.
1. Which models of StepManiaX cabinets does this support?
* This currently only supports StepManiax Dedicated Cabinets. If you have an AIO or a DX cabinet and wish to get support added, shoot me a Discord PM (skogaby#1337) and we can possibly work something out.
2. Can I emulate a cabinet other than Gold cabinets with this?
* No, this is explicitly meant for doing Gold Cabinet lights on SMX hardware. If you wish to emulate other cabinet lights, use the `smxdedicab` option in Spice2x, and map the lights via `spicecfg`. This tool exists to get around the limitations in mapping large amounts of RGB lights / tape LEDs in `spicecfg`.
3. Does this support inputs, or only lights?
* This only supports lights, at the moment. This allows the tool to be a lot looser with timing requirements, and you should have fine results mapping the inputs via `spicecfg` directly.
4. Can I re-map the lighting configuration?
* No. If you wish to change the lights, that's all hardcoded right now.
5. Can I run this directly on a StepManiaX Android PC?
* No. The main use case for this is for those who have a mini windows PC inside their cabinet, along with splitters, etc. so that all the USB hardware is connected directly to the PC that's running DDR. Theoretically this could be ported to Android to work over network like `KFChicken` or something, but that's outside the scope of my work.