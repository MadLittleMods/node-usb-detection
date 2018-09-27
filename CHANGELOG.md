# Changelog

## v4.0.0 - 2018-9-27

 - Use native Promises, https://github.com/MadLittleMods/node-usb-detection/pull/69
    - Now requires Node.js >=4
 - Update dependencies based on npm security audit, https://github.com/MadLittleMods/node-usb-detection/pull/70


## v3.2.0 - 2018-7-23

 - Add TypeScript declarations/definitions
    - Thanks to [@thegecko](https://github.com/thegecko) for the [contribution](https://github.com/MadLittleMods/node-usb-detection/pull/66)


## v3.1.0 - 2018-6-4

 - Add serial number support to Windows
    - Thanks to [@doganmurat](https://github.com/doganmurat) for the [contribution](https://github.com/MadLittleMods/node-usb-detection/pull/62)


## v3.0.0 - 2018-5-13

 - Show multiple/duplicate USB devices on Windows
    - Thanks to [@doganmurat](https://github.com/doganmurat) for the [contribution](https://github.com/MadLittleMods/node-usb-detection/pull/54)
 - Update all dependencies


## v2.1.0 - 2018-2-6

 - Add npm `install` hook that will use our prebuilt binaries instead of having to compile from source with node-gyp.
 - Remove side-effects when you `require('usb-detection')` so the process won't hang from just requiring.
 - Fix 100% CPU usage on Linux, https://github.com/MadLittleMods/node-usb-detection/issues/2
    - Thanks to [@sarakusha](https://github.com/sarakusha) for the [contribution](https://github.com/MadLittleMods/node-usb-detection/pull/21)
 - Ensure the process will exit gracefully across all platforms, https://github.com/MadLittleMods/node-usb-detection/issues/35


## v2.0.1 - 2017-12-27

 - Remove npm `install` hook to prevent hanging the install process caused by `prebuild-install` verify require and our side-effects.
    - Thanks to [@Lange](https://github.com/Lange) for noticing an issue.


## v2.0.0 - 2017-12-19

 - ~~Remove side-effects when you `require('usb-detection')` so the process won't hang from just requiring.~~
   Now requires an explicit call to `usbDetect.startMonitoring()` to begin listening to USB add/remove/change events.
 - Add npm `install` hook that will use our prebuilt binaries instead of having to compile from source with node-gyp.


## v1.4.2 - 2017-11-11

 - Remove npm `install` hook to prevent hanging the install process caused by `prebuild-install` verify require and our side-effects.
    - Thanks to [@Lange](https://github.com/Lange) for [figuring out the root cause](https://github.com/MadLittleMods/node-usb-detection/pull/47#issuecomment-343714022).


## v1.4.1 - 2017-11-11

 - Add check for null before notifying of addition/removal
    - Thanks to [@reidmweber](https://github.com/reidmweber) for [this contribution](https://github.com/MadLittleMods/node-usb-detection/pull/32) via [#37](https://github.com/MadLittleMods/node-usb-detection/pull/37)
 - Create prebuilt binaries on tagged releases
    - Thanks to [@jayalfredprufrock](https://github.com/jayalfredprufrock) for the PoC and [@Lange](https://github.com/Lange) for the [contribution](https://github.com/MadLittleMods/node-usb-detection/pull/47)

## v1.4.0 - 2016-3-20

 - Add compatibility for `node@0.10.x` by using more `nan` types and methods.
    - Thanks to [@apla](https://github.com/apla) for [this contribution](https://github.com/MadLittleMods/node-usb-detection/pull/26)!


## v1.3.0 - 2015-10-11

 - Add compatibility for Node 4
    - Upgrade [`nan`](https://www.npmjs.com/package/nan) dependency nan@2.x. Thank you [@lorenc-tomasz](https://github.com/lorenc-tomasz)


## v1.2.0 - 2015-6-12

 - New maintainer/owner [@MadLittleMods](https://github.com/MadLittleMods). Previously maintained by [@adrai](https://github.com/adrai) :+1:
 - Add tests `npm test`
 - `find` now also returns a promise
 - Format js and c++
    - Added eslint file, linter code style guidelines
 - Alias `insert` as the `add` event name for `.on`
 - Update readme
    - Fix usage section `.on` callbacks which do not actually have a `err` parameter passed to the callback
    - Add API section to document clearly all of the methods and events emitted
    - Add test instructions


## v1.1.0

 - Add support for Node v0.12.x


## v1.0.3

- Revert "ready for node >= 0.11.4"


## v1.0.2

- Fix issues found via cppcheck


## v1.0.1

- Ready for node >= 0.11.4


## v1.0.0

- First release
