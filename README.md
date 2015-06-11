# Installation

    npm install usb-detection

This assumes you have everything on your system necessary to compile ANY native module for Node.js. This may not be the case, though, so please ensure the following are true for your system before filing an issue about "Does not install". For all operatings systems, please ensure you have Python 2.x installed AND not 3.0, [node-gyp](https://github.com/TooTallNate/node-gyp) (what we use to compile) requires Python 2.x.

### Windows:

Ensure you have Visual Studio 2010 installed. If you have any version OTHER THAN VS 2010, please read this: https://github.com/TooTallNate/node-gyp/issues/44 

### Mac OS X:

Ensure that you have at a minimum the xCode Command Line Tools installed appropriate for your system configuration. If you recently upgrade OS, it probably removed your installation of Command Line Tools, please verify before submitting a ticket.

### Linux:

You know what you need for you system, basically your appropriate analog of build-essential. Keep rocking!

**Make sure you've installed libudev!**

# Usage

```nodejs
var monitor = require('usb-detection');
    
monitor.find(function(err, devices) {});
monitor.find(vid, function(err, devices) {});
monitor.find(vid, pid, function(err, devices) {});

monitor.on('add', function(err, devices) {});
monitor.on('add:vid', function(err, devices) {});
monitor.on('add:vid:pid', function(err, devices) {});

monitor.on('remove', function(err, devices) {});
monitor.on('remove:vid', function(err, devices) {});
monitor.on('remove:vid:pid', function(err, devices) {});

monitor.on('change', function(err, devices) {});
monitor.on('change:vid', function(err, devices) {});
monitor.on('change:vid:pid', function(err, devices) {});
```

# Diff with original
Add an attribute 'mountPath' to show mounted point on Linux and Mac. ( Windows need someone to fix ).

# Release Notes

## v1.1.0

 - Add support for Node v0.12.x

## v1.0.3

- revert "ready for node >= 0.11.4"

## v1.0.2

- fixed issues found via cppcheck

## v1.0.1

- ready for node >= 0.11.4

## v1.0.0

- first release


# License

Copyright (c) 2013 Kaba AG

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
