const { exec, spawn } = require('child_process');
const { promises: fsp } = require('fs');

const replacer = str =>
	str
		.replace(/^\s*\d+\s*(?:pts\/|tty)\d+\s*([A-Za-z+]+)\s*\d+:\d+\s*(.*)$/gm, (match, p1, p2) => {
			const p2r = p2.replace(/rtest/g, 'test').replace(/tshref/g, 'tsh');

			return `${p1}: ${p2r}`;
		}) // Replace ps table
		.replace(/^(Job )?\[(\d+)\] \(\d+\)/gm, '$1[$2]') // Replace Job [jid] (pid), [jid] (pid)
		.replace(/^\.\/sdriver\.pl -t trace(\d+)\.txt -s .*$/gm, 'Test $1') // Replace ./sdriver.pl ...
		

const execPromise = command => 
	new Promise(resolve => {
		exec(command, (err, stdout, stderr) => {
			if (err) {
				console.error("Differ Error!");
				console.error(err);
			}
			
			const output =
				'======= STDOUT =======\n' +
				`${stdout}\n` +
				'======= OUTPUT =======\n' +
				`${stderr}\n`;
			
			resolve(output);
		});
	});

const testRun = async testPrefix => {
	let output = '';
	for (let i = 1; i <= 16; i++) {
		const testName = `${testPrefix}${i.toString().padStart(2, '0')}`;
		console.log(`Running ${testName}`);

		output += await execPromise(`make ${testName}`);
	}
	
	return output;
};

(async () => {
	const tshOut = replacer(await testRun('test'));
	const refOut = replacer(await testRun('rtest'));
	
	await fsp.writeFile('./tsh.out', tshOut);
	await fsp.writeFile('./tshref.out', refOut);
	
	if (tshOut === refOut) {
		console.log("Two output matches!");
		return;
	}
	
	spawn('vimdiff', ['./tsh.out', './tshref.out'], {
		detached: true,
		stdio: 'inherit'
	});
})();
