var monitor = require('./usb-detection');
var usb     = require('usb');

monitor.find(function(err, devices) {
	console.log("find: some devices");
});
//monitor.find(vid, function(err, devices) {});
//monitor.find(vid, pid, function(err, devices) {});

monitor.on('add', function(devices) { 
	console.log("add USB:", devices); 
	console.log(usb.findByIds(devices.vendorId, devices.productId).interfaces);
});

monitor.on('add:vid', function(devices, vid) { 
	
});

monitor.on('add:vid:pid', function(devices) {

});

monitor.on('remove', function(err, devices) {

});

monitor.on('remove:vid', function(err, devices) {

});

monitor.on('remove:vid:pid', function(err, devices) {

});

monitor.on('change', function(err, devices) {

});

monitor.on('change:vid', function(err, devices) {

});

monitor.on('change:vid:pid', function(err, devices) {

});