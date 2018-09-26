var spawn = require('child_process').spawn;

class ChildExecutor {
	constructor() {
		this.child = null;
	}

	exec(command) {
		var commandParts = command.split(' ');
		return new Promise((resolve, reject) => {
			var stdout = '';
			var stderr = '';

			// We use `child` so we can manually send a kill
			// ex.
			// var executor = new ChildExecutor();
			// executor.exec('SOMECOMMAND').then((resultInfo) => { console.log('resultInfo'); });
			// executor.child.kill('SIGINT');
			//
			// We have to use `spawn` instead of `exec` because you can't kill it
			this.child = spawn(commandParts[0], commandParts.slice(1));

			this.child.stdout.on('data', (data) => {
			  stdout += data;
			});

			this.child.stderr.on('data', (data) => {
			  stderr += data;
			});

			this.child.on('close', (code) => {
			  resolve({
					command: command,
					stdout: stdout,
					stderr: stderr
				});
			});

			this.child.on('error', (err) => {
			  reject({
					command: command,
					stdout: stdout,
					stderr: stderr,
					error: err
				});
			});

			//this.child.stdout.pipe(process.stdout);
			//this.child.stderr.pipe(process.stderr);
		});
	}
}

module.exports = ChildExecutor;
