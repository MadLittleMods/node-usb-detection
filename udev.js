var udev = require('udev'),
	_ = require('lodash'),
	EventEmitter2 = require('eventemitter2').EventEmitter2,
	devices = [],
	devs = udev.list(),
	monitor = udev.monitor();

for (var i = 0, len = devs.length; i < len; i++) {
	var dev = devs[i];
	if (dev.ID_VENDOR_ID) {
		devices.push(dev);
	}
}

var detector = new EventEmitter2({
	wildcard: true,
	delimiter: ':',
	maxListeners: 1000 // default would be 10!
});

detector.find = function(vid, pid, callback) {
	var found = _.find(devices, function(d) {
		return d.ID_VENDOR_ID == vid && d.ID_MODEL_ID;
	});

	callback(null, found);
};

monitor.on('add', function (device) {
	if (device.ID_VENDOR_ID) {
		devices.push(device);
		detector.emit('add:' + device.ID_VENDOR_ID + ':' + device.ID_MODEL_ID, device);
		detector.emit('add:' + device.ID_VENDOR_ID, device);
		detector.emit('add', device);
		detector.emit('change:' + device.ID_VENDOR_ID + ':' + device.ID_MODEL_ID, device);
		detector.emit('change:' + device.ID_VENDOR_ID, device);
		detector.emit('change', device);
	}
});
monitor.on('remove', function (device) {
	if (device.ID_VENDOR_ID) {
		devices = _.reject(devices, function(dev) {
			return dev.syspath === device.syspath;
		});
		detector.emit('remove:' + device.ID_VENDOR_ID + ':' + device.ID_MODEL_ID, device);
		detector.emit('remove:' + device.ID_VENDOR_ID, device);
		detector.emit('remove', device);
		detector.emit('change:' + device.ID_VENDOR_ID + ':' + device.ID_MODEL_ID, device);
		detector.emit('change:' + device.ID_VENDOR_ID, device);
		detector.emit('change', device);
	}
});

module.exports = detector;


