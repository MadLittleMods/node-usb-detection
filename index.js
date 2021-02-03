var SegfaultHandler = require('segfault-handler');
SegfaultHandler.registerHandler();

var index = require('./package.json');

function isFunction(functionToCheck) {
	return typeof functionToCheck === 'function';
}

if(global[index.name] && global[index.name].version === index.version) {
	module.exports = global[index.name];
} else {
	var detection = require('bindings')('detection.node');
	var EventEmitter2 = require('eventemitter2').EventEmitter2;

	var detector = new EventEmitter2({
		wildcard: true,
		delimiter: ':',
		maxListeners: 1000 // default would be 10!
	});

	//detector.find = detection.find;
	detector.find = function(vid, pid, callback) {
		// Suss out the optional parameters
		if(isFunction(vid) && !pid && !callback) {
			callback = vid;
			vid = undefined;
		} else if(isFunction(pid) && !callback) {
			callback = pid;
			pid = undefined;
		}

		return new Promise(function(resolve, reject) {
			// Assemble the optional args into something we can use with `apply`
			var args = [];
			if(vid) {
				args.push(vid);
			}
			if(pid) {
				args.push(pid);
			}

			// Tack on our own callback that takes care of things
			args.push(function(err, devices) {

				// We call the callback if they passed one
				if(callback) {
					callback.call(callback, err, devices);
				}

				// But also do the promise stuff
				if(err) {
					reject(err);
					return;
				}
				resolve(devices);
			});

			// Fire off the `find` function that actually does all of the work
			detection.find.apply(detection, args);
		});
	};

	function fireAdded(device) {
		detector.emit('add:' + device.vendorId + ':' + device.productId, device);
		detector.emit('insert:' + device.vendorId + ':' + device.productId, device);
		detector.emit('add:' + device.vendorId, device);
		detector.emit('insert:' + device.vendorId, device);
		detector.emit('add', device);
		detector.emit('insert', device);

		detector.emit('change:' + device.vendorId + ':' + device.productId, device);
		detector.emit('change:' + device.vendorId, device);
		detector.emit('change', device);
	}

	function fireRemoved(device) {
		detector.emit('remove:' + device.vendorId + ':' + device.productId, device);
		detector.emit('remove:' + device.vendorId, device);
		detector.emit('remove', device);

		detector.emit('change:' + device.vendorId + ':' + device.productId, device);
		detector.emit('change:' + device.vendorId, device);
		detector.emit('change', device);
	}

	function fireEvent(type, device) {
		switch (type) {
		case 'add':
			fireAdded(device);
			break;
		case 'remove':
			fireRemoved(device);
			break;
		default:
			// Ignore
			break;
		}
	}

	var started = false;

	detector.startMonitoring = function() {
		if(started) {
			return;
		}

		started = true;
		detection.startMonitoring(fireEvent);
	};

	detector.stopMonitoring = function() {
		if(!started) {
			return;
		}

		started = false;
		detection.stopMonitoring();
	};

	detector.version = index.version;
	global[index.name] = detector;

	module.exports = detector;
}
