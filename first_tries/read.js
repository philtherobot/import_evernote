process = require('process');

const readable = process.stdin;

var whole_input = '';

readable.on('readable', () => {
	let chunk;
	while (null !== (chunk = readable.read())) {
	whole_input = whole_input + chunk;
	}
});

readable.on('end', () => {
	console.log(whole_input);
});
