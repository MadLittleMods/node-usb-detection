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
		return parseInt(d.ID_VENDOR_ID, 16) == vid && parseInt(d.ID_MODEL_ID, 16);
	});

	callback(null, found);
};

monitor.on('add', function (device) {
	if (device.ID_VENDOR_ID && device.ID_MODEL_ID) {
		var vid = parseInt(device.ID_VENDOR_ID, 16);
		var pid = parseInt(device.ID_MODEL_ID, 16);
		devices.push(device);
		detector.emit('add:' + vid + ':' + pid, device);
		detector.emit('add:' + vid, device);
		detector.emit('add', device);
		detector.emit('change:' + vid + ':' + pid, device);
		detector.emit('change:' + vid, device);
		detector.emit('change', device);
	}
});
monitor.on('remove', function (device) {
	if (device.ID_VENDOR_ID && device.ID_MODEL_ID) {
		var vid = parseInt(device.ID_VENDOR_ID, 16);
		var pid = parseInt(device.ID_MODEL_ID, 16);
		devices = _.reject(devices, function(dev) {
			return dev.syspath === device.syspath;
		});
		detector.emit('remove:' + vid + ':' + pid, device);
		detector.emit('remove:' + vid, device);
		detector.emit('remove', device);
		detector.emit('change:' + vid + ':' + pid, device);
		detector.emit('change:' + vid, device);
		detector.emit('change', device);
	}
});

module.exports = detector;


