var monitor = require('./usb-detection');

monitor.find(function(err, devices) {
	console.log("find: some devices");
});
monitor.find(vid, function(err, devices) {

});

monitor.find(vid, pid, function(err, devices) {
    
});

monitor.on('add', function(devices) { 
	console.log("add USB:", devices); 
	
});

monitor.on('add:vid', function(devices, vid) { 
	
});

monitor.on('add:vid:pid', function(devices) {

});

monitor.on('remove', function(err, devices) {
    console.log("remove USB ");

});

monitor.on('remove:vid', function(err, devices) {

});

monitor.on('remove:vid:pid', function(err, devices) {

});

monitor.on('change', function(err, devices) {
    console.log("change USB");
});

monitor.on('change:vid', function(err, devices) {

});

monitor.on('change:vid:pid', function(err, devices) {

});