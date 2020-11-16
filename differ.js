const { promises: fsp } = require('fs');

const replacer = str =>
	str
		.replace(/^\d+\s*pts\/\d+\s*([A-Z]+)\s*\d+:\d+ .*$/gm, '$1') // Replace ps table
		.replace(/^(Job )?[(\d+)] \(\d+\)/gm, '$1[$2]') // Replace Job [jid] (pid), [jid] (pid)
		.replace(/^\.\/sdriver\.pl -t trace\(d+)\.txt -s .*$/gm, 'Test $1') // Replace ./sdriver.pl ...
		

(async () => {
	const tshOut = await fsp.readFile('./tsh.out', 'utf8');
	const refOut = await fsp.readFile('./tshref.out', 'utf8');
	
	await fsp.writeFile('./tsh.out', replacer(tshOut));
	await fsp.writeFile('./tshref.out', replacer(refOut));
	
	console.log('Done :D');
})();