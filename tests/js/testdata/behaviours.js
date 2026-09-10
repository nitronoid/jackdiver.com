// render(input, arg) does what `arg` says, so one script covers every way a shim can end.
//   "return <json>"  returns the JSON value.
//   "throw <json>"   throws the JSON value.
// Anything else is a named behaviour below.
function recurse(depth) {
	return recurse(depth + 1) + 1;
}

globalThis.render = function (input, arg) {
	if (arg.startsWith("return ")) {
		return JSON.parse(arg.slice("return ".length));
	}
	if (arg.startsWith("throw ")) {
		throw JSON.parse(arg.slice("throw ".length));
	}
	switch (arg) {
		case "error":
			throw Object.assign(new Error("boom"), { position: 2 });
		case "nan":
			throw { message: "not a number", position: NaN };
		case "getter":
			throw {
				get message() {
					throw new Error("the getter");
				},
				position: 1,
			};
		case "type error":
			return null.property;
		case "recurse":
			return recurse(0);
		case "allocate": {
			const hoard = [];
			for (;;) {
				hoard.push(new Array(65536).fill(1));
			}
		}
		case "echo":
			return input;
	}
	return "unknown behaviour: " + arg;
};
