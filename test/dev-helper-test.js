// This is just used to debug the package while you develop things

var usbDetect = require('../');

/* */
console.log('startMonitoring');
usbDetect.startMonitoring();
/* */

/* */
usbDetect.find()
	.then(function(devices) {
		console.log('find', devices.length, devices.sort((a, b) => {
			if(a.vendorId !== b.vendorId) {
				return a.vendorId - b.vendorId;
			}
			else if(a.productId !== b.productId) {
				return a.productId - b.productId;
			}

			return a.deviceName.localeCompare(b.deviceName);
		}));
	});


usbDetect.on('add', function(device) {
	console.log('add', device);

	usbDetect.find()
		.then(function(devices) {
			console.log('find', devices.length, devices);
		});
});

usbDetect.on('remove', function(device) {
	console.log('remove', device);
});

usbDetect.on('change', function(device) {
	console.log('change', device);
});
/* */

/* * /
setTimeout(() => {
	console.log('stopMonitoring');
	usbDetect.stopMonitoring();
}, 1000);
/* */

/* * /
//console.log('stopMonitoring');
usbDetect.stopMonitoring();
/* */


/* * /
console.log('startMonitoring0');
usbDetect.startMonitoring();

var addIndex = 0;
usbDetect.on('add', function(device) {
	console.log('add', addIndex);

	if(addIndex === 0) {
		console.log('stopMonitoring0 (after add)');
		usbDetect.stopMonitoring();

		setTimeout(function() {
			console.log('startMonitoring1');
			usbDetect.startMonitoring();
		}, 250);
	}
	else if(addIndex === 1) {
		console.log('stopMonitoring1 (after add)');
		usbDetect.stopMonitoring();
	}

	addIndex++;
});
/* */
