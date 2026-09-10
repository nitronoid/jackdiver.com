// highlight.js behind the global render(code, language) that js::engine calls. Ours; highlight.js,
// which has to run first, is vendored as it came.
//   code      The source text.
//   language  A language name or alias that hljs.getLanguage() knows, in any case.
// It returns `code` as HTML, escaped, with <span class="hljs-..."> around the tokens, or throws
// {message}. There is no auto-detection: a language it doesn't know is an error.

// highlight.js stays in its default safe mode, where an error inside highlight() is swallowed and
// the result is the escaped text with the error in `errorRaised`. That would put unhighlighted code
// in the site with no word of it, so it is thrown here. Debug mode would also throw it, but it
// turns every benign quirk of a grammar (a "0 width match" that safe mode skips over) into a
// failure too, and it can't tell us more than errorRaised does.
//
// A bare QuickJS context has no `console`, and highlight.js writes to it only to report a broken
// grammar or an unknown language (which is checked first, below). Neither happens with the bundled
// grammars (the tests run every one), and if one ever did, the ReferenceError would fail the load
// or the render loudly, so there is no stand-in console.
globalThis.render = function (code, language) {
	if (hljs.getLanguage(language) === undefined) {
		throw { message: "unknown language" };
	}
	const result = hljs.highlight(code, { language, ignoreIllegals: true });
	// `in`, not a test of the value: QuickJS throws null when it cannot allocate its own
	// out-of-memory error, and highlight.js would keep that as the error.
	if ("errorRaised" in result) {
		throw { message: String(result.errorRaised) };
	}
	return result.value;
};
