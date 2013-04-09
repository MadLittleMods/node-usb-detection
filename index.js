if (process.platform === 'win32') {
	module.exports = require('./native');
} else if (process.platform === 'darwin') {
	module.exports = require('./native');
} else {
	module.exports = require('./native');
}