var exec = require('child_process').exec;

var commandRunner = function(command) {
	return new Promise(function(resolve, reject) {
		var child = exec(command, function(err, stdout, stderr) {
			var resultInfo = {
				command: command,
				stdout: stdout,
				stderr: stderr,
				error: err
			};

			if(err) {
				reject(resultInfo);
			}
			else {
				resolve(resultInfo);
			}
		});

		//child.stdout.pipe(process.stdout);
		//child.stderr.pipe(process.stderr);
	});
};

module.exports = commandRunner;
