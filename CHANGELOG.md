# Changelog

## v1.4.0 - 2016-3-20

 - Add compatibility for `node@0.10.x` by using more `nan` types and methods.


## v1.3.0 - 2015-10-11

 - Add compatibility for Node 4
 	 - Upgrade [`nan`](https://www.npmjs.com/package/nan) dependency nan@2.x. Thank you @lorenc-tomasz


## v1.2.0 - 2015-6-12

 - New maintainer/owner @MadLittleMods. Previously maintained by @adrai :+1:
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
