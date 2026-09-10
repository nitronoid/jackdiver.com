// Temml behind the global render(tex, mode) that js::engine calls. Ours; temml.js, which has to run
// first, is vendored as it came.
//   tex   The formula, without delimiters.
//   mode  "display" for display style, anything else for inline.
// It returns the MathML, or throws {message, position?}: Temml's ParseError, without its position
// suffix and with the position as the UTF-16 index into `tex` that Temml gives.

// Temml's ParseError message is " " + message + header + excerpt, where the header is
// " at position N: \n" (N from 1) or " at end of input: ". The message can itself hold TeX that
// looks like a header, so cut at the last one; the excerpt is too short to hold one.
function parseErrorMessage(full, position) {
	const start = full.startsWith(" ") ? 1 : 0;
	if (position === undefined) {
		return full.slice(start);
	}
	let cut = -1;
	for (const header of [" at position " + (position + 1) + ": \n", " at end of input: "]) {
		const found = full.lastIndexOf(header);
		if (found >= start && found > cut) {
			cut = found;
		}
	}
	return cut < 0 ? full.slice(start) : full.slice(start, cut);
}

globalThis.render = function (tex, mode) {
	try {
		return temml.renderToString(tex, {
			displayMode: mode === "display",
			annotate: true,
			throwOnError: true,
			trust: false,
			// Strict mode makes Temml call console.log, which a bare context lacks.
			strict: false,
		});
	} catch (exception) {
		// A primitive, or null (QuickJS throws null when it cannot even allocate its
		// out-of-memory error): the engine reports it as it is.
		if (Object(exception) !== exception) {
			throw exception;
		}
		// The engine reports an object by its message, so the rest keep their name:
		// "TypeError: ...", "RangeError: Maximum call stack size exceeded".
		if (exception.name !== "ParseError") {
			throw { message: String(exception) };
		}
		const position = Number.isInteger(exception.position) && exception.position >= 0 ?
			exception.position : undefined;
		throw { message: parseErrorMessage(String(exception.message), position), position };
	}
};
