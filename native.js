var detection = require('bindings')('detection.node'),
	EventEmitter2 = require('eventemitter2').EventEmitter2;

var detector = new EventEmitter2({
	wildcard: true,
	delimiter: ':',
	maxListeners: 1000 // default would be 10!
});

detector.find = detection.find;

detection.registerAdded(function(device) {
	detector.emit('add:' + device.vendorId + ':' + device.productId, device);
	detector.emit('add:' + device.vendorId, device);
	detector.emit('add', device);
	detector.emit('change:' + device.vendorId + ':' + device.productId, device);
	detector.emit('change:' + device.vendorId, device);
	detector.emit('change', device);
});

detection.registerRemoved(function(device) {
	detector.emit('remove:' + device.vendorId + ':' + device.productId, device);
	detector.emit('remove:' + device.vendorId, device);
	detector.emit('remove', device);
	detector.emit('change:' + device.vendorId + ':' + device.productId, device);
	detector.emit('change:' + device.vendorId, device);
	detector.emit('change', device);
});

module.exports = detector;


