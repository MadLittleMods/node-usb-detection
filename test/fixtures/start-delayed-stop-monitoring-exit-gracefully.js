var usbDetect = require('../../');

usbDetect.startMonitoring();

setTimeout(function() {
	usbDetect.stopMonitoring();
}, 100);
