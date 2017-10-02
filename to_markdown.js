var process = require('process');
var toMarkdown = require('to-markdown')

const readable = process.stdin;

var whole_input = '';

dbg = function(s) { 
	//process.stderr.write(s + "\n"); 
}

dbg("to-markdown");

readable.on('readable', () => {
	dbg("readable");
	let chunk;
	while (null !== (chunk = readable.read())) {
		dbg(chunk.length.toString());
		whole_input = whole_input + chunk;
	}
});

readable.on('end', () => {
	dbg("end");
	converter = { filter: 'div',
		replacement: function(content) {
			return content + "\n\n";
		}}

	process.stdout.write(toMarkdown(whole_input, {converters: [converter]}));
	process.exit()
});
