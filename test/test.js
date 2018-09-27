var path = require('path');

var chai = require('chai');
var expect = require('chai').expect;
var chalk = require('chalk');
var commandRunner = require('./lib/command-runner');
var ChildExecutor = require('./lib/child-executor');
var getSetTimeoutPromise = require('./lib/set-timeout-promise-helper');

// The plugin to test
var usbDetect = require('../');

const MANUAL_INTERACTION_TIMEOUT = 10000;

// We just look at the keys of this device object
var DEVICE_OBJECT_FIXTURE = {
	locationId: 0,
	vendorId: 5824,
	productId: 1155,
	deviceName: 'Teensy USB Serial (COM3)',
	manufacturer: 'PJRC.COM, LLC.',
	serialNumber: '',
	deviceAddress: 11
};

function once(eventName) {
	return new Promise(function(resolve) {
		usbDetect.on(eventName, function(device) {
			resolve(device);
		});
	});
}

function testDeviceShape(device) {
	expect(device)
		.to.have.all.keys(DEVICE_OBJECT_FIXTURE)
		.that.is.an('object');
}

describe('usb-detection', function() {
	describe('API', function() {
		beforeAll(function() {
			usbDetect.startMonitoring();
		});

		afterAll(function() {
			usbDetect.stopMonitoring();
		});

		describe('`.find`', function() {
			var testArrayOfDevicesShape = function(devices) {
				expect(devices.length).to.be.greaterThan(0);
				devices.forEach(function(device) {
					testDeviceShape(device);
				});
			};

			it('should find some usb devices', function(done) {
				usbDetect.find(function(err, devices) {
					testArrayOfDevicesShape(devices);
					expect(err).to.equal(undefined);
					done();
				});
			});

			it('should return a promise', function(done) {
				usbDetect.find()
					.then(function(devices) {
						testArrayOfDevicesShape(devices);
					})
					.then(done)
					.catch(done.fail);
			});
		});

		describe('Events `.on`', function() {
			it('should listen to device add/insert', function(done) {
				console.log(chalk.black.bgCyan('Add/Insert a USB device'));
				once('add')
					.then(function(device) {
						testDeviceShape(device);
					})
					.then(done)
					.catch(done.fail);
			}, MANUAL_INTERACTION_TIMEOUT);

			it('should listen to device remove', function(done) {
				console.log(chalk.black.bgCyan('Remove a USB device'));
				once('remove')
					.then(function(device) {
						testDeviceShape(device);
					})
					.then(done)
					.catch(done.fail);
			}, MANUAL_INTERACTION_TIMEOUT);

			it('should listen to device change', function(done) {
				console.log(chalk.black.bgCyan('Add/Insert or Remove a USB device'));
				once('change')
					.then(function(device) {
						testDeviceShape(device);
					})
					.then(done)
					.catch(done.fail);
			}, MANUAL_INTERACTION_TIMEOUT);
		});
	});

	describe('can exit gracefully', () => {
		it('when requiring package (no side-effects)', (done) => {
			commandRunner(`node ${path.join(__dirname, './fixtures/requiring-exit-gracefully.js')}`)
				.then(done)
				.catch((resultInfo) => {
					done.fail(resultInfo.err);
				});
		});

		it('after `startMonitoring` then `stopMonitoring`', (done) => {
			commandRunner(`node ${path.join(__dirname, './fixtures/start-stop-monitoring-exit-gracefully.js')}`)
				.then(done)
				.catch((resultInfo) => {
					done.fail(resultInfo.err);
				});
		});

		it('after `startMonitoring` then an async delayed `stopMonitoring`', (done) => {
			commandRunner(`node ${path.join(__dirname, './fixtures/start-delayed-stop-monitoring-exit-gracefully.js')}`)
				.then(done)
				.catch((resultInfo) => {
					done.fail(resultInfo.err);
				});
		});

		it('when SIGINT (Ctrl + c) after `startMonitoring`', (done) => {
			const executor = new ChildExecutor();

			executor.exec(`node ${path.join(__dirname, './fixtures/sigint-after-start-monitoring-exit-gracefully.js')}`)
				.then(done)
				.catch((resultInfo) => {
					done.fail(resultInfo.err);
				});

			getSetTimeoutPromise(100)
				.then(() => {
					executor.child.kill('SIGINT');
				})
				.catch(done.fail);
		});
	});
});
