// The shim half of the "echo" fixture. It needs library.js to have run first.
const prefix = decorate("shim");
globalThis.render = (input, arg) => prefix + decorate(arg) + input;
