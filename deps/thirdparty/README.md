# Third-party code

Each package is vendored unmodified, with its licence, and built by a `BUILD.bazel` of ours beside it. `.bazelrc` passes `-w` for everything under `deps/thirdparty`, and clang-tidy and clang-format leave it alone.

## PEGTL

|  |  |
|---|---|
| Version | 4.0.1 (`TAO_PEGTL_VERSION` in `include/tao/pegtl/version.hpp`) |
| URL | `https://github.com/taocpp/PEGTL/archive/refs/tags/4.0.1.tar.gz` |
| sha256 | `ebc930bd9e3c37fd40771304cd3f318623ed87fe2880d17f514c080d16a12965` |
| Licence | Boost Software License 1.0, `pegtl/LICENSE_1_0.txt` |
| Vendored | `include/` and `LICENSE_1_0.txt` from the archive |

The vendoring commit recorded no source. The URL and sha256 are those of the GitHub archive of the 4.0.1 tag, whose `include/` and `LICENSE_1_0.txt` match this tree byte for byte, but for the patch below.

One file is patched, marked with a `jackdiver.com:` comment: `include/tao/pegtl/internal/resize_uninitialized.hpp` takes libc++'s private `std::vector::__end_`, which libc++ 23 moved into `__vector_layout`. The hermetic toolchain builds Linux against libc++ 23 (macOS uses the SDK's older libc++), so there the patch uses PEGTL's generic version instead. PEGTL 4.0.2 and its `main` were not fixed when checked (2026-10-16).

To update: unpack the new archive into an empty directory, replace `pegtl/include` and `pegtl/LICENSE_1_0.txt` with its `include/` and `LICENSE_1_0.txt`, change the version, URL and sha256 above, and apply the patch again unless upstream has fixed it.

## QuickJS-ng

|  |  |
|---|---|
| Version | v0.17.0 (2026-09-18) |
| URL | `https://github.com/quickjs-ng/quickjs/releases/download/v0.17.0/quickjs-amalgam.zip` |
| sha256 | `a0955463c74809173a253ff87365095e6972e8b171cfb6aa8a1e781adf35cfeb` |
| Licence | MIT, `quickjs/LICENSE` |
| Vendored | `quickjs-amalgam.c`, `quickjs.h` and `quickjs-libc.h` from the zip; `qjsc.c` and `cutils.h` from the tag |
| File sha256 | `qjsc.c` `0d8ce14fce606a87bbdf9521badd2147c12a1ad5961510fec9e69a3292e59a47`, `cutils.h` `dffec8bea17f08108da5a91c1178804754698bbbb6202dc100268e78e8c5b770`, `quickjs-libc.h` `55c6b4504124941903fc09c35aecedddb88fa45c27d5e62c65962bb670ae17d1` |

The zip has no licence file, so `LICENSE` is the one in the v0.17.0 tag's source tree (`https://raw.githubusercontent.com/quickjs-ng/quickjs/v0.17.0/LICENSE`).

`qjsc.c` is the QuickJS compiler (`https://raw.githubusercontent.com/quickjs-ng/quickjs/v0.17.0/qjsc.c`), which `//tools/js:defs.bzl` runs at build time. It is not in the zip, and it includes `cutils.h`, which is not either, so that is from the tag too (`.../v0.17.0/cutils.h`). It is a header of `static inline` functions, and is the same text as the `CUTILS_H` section of the amalgam, from `#ifndef CUTILS_H` to its `#endif`. `quickjs-libc.h` is the same in the zip and in the tag. Only `qjsc` is built with `QJS_BUILD_LIBC`, which is what makes the amalgam hold `quickjs-libc` (`js_load_file` and `js_std_dump_error` are what `qjsc.c` calls): `:quickjs`, which `blog` links, is built without it.

To update: download the zip for the new tag into an empty directory, check its sha256 against the release, copy `quickjs-amalgam.c`, `quickjs.h` and `quickjs-libc.h` over the vendored ones without editing them, fetch `LICENSE`, `qjsc.c` and `cutils.h` from the same tag, and change the version, URL and sha256s above. Check that `cutils.h` is still the `CUTILS_H` section of the amalgam, and that `qjsc.c` includes nothing else the zip lacks. Update `bytecode_version` in `tests/js/js_bytecode_test.cxx` to the amalgam's `BC_VERSION`. Then run `bazel test //...` and `bazel test //tests/math/... //tests/js/... --config=asan`.

## Temml

|  |  |
|---|---|
| Version | 0.14.0 (2026-10-08) |
| URL | `https://registry.npmjs.org/temml/-/temml-0.14.0.tgz` |
| sha256 | `6be7cc75cd61af39a98fd1d88146732818b042ae665963cf0473b44a44b090e2` |
| Licence | MIT, `temml/LICENSE` |
| Vendored | `dist/temml.js` and `dist/Temml-STIX2.css` from the package, and its `LICENSE` |
| File sha256 | `temml.js` `4bacb8137af19c36cea8c247d80239ef2f1e487ed7ed4c4fa232a9ea7a80b65d`, `Temml-STIX2.css` `e8e17549df716ffc30d3a063dcde31aa3199b1309373e4fe54c87375f98826d0` |

`temml.js` is the unminified build, for readable stack traces. `js_bytecode` compiles it, then `temml_shim.js`, to QuickJS bytecode at build time (`//deps/thirdparty:temml_bytecode`). The shim is ours, not vendored: it defines the global `render(tex, mode)` that `js::engine` calls, passes Temml its options, and turns Temml's `ParseError` into `{message, position}`.

To update: download the tarball for the new version into an empty directory, check its sha256 against the registry's, copy `package/dist/temml.js`, `package/dist/Temml-STIX2.css` and `package/LICENSE` over the vendored files without editing them, and change the version, URL and sha256s above. Check that `temml_shim.js` still fits: it relies on the global `temml`, on `renderToString` and its options, and on the `ParseError` message, which is `" "`, the message, then `" at position N: \n"` or `" at end of input: "` and an excerpt (`tests/math/temml_test.cxx` pins these). Then compare `style/_temml.scss` with the new `Temml-STIX2.css`: it is that file with the lines named in its header comment taken out, so carry any new rule across, and decide whether it clashes with `style/_math.scss`. Then run `bazel test //...` and `bazel test //tests/math/... --config=asan`.

## highlight.js

|  |  |
|---|---|
| Version | 11.12.0 (2026-08-12) |
| URL | `https://registry.npmjs.org/@highlightjs/cdn-assets/-/cdn-assets-11.12.0.tgz` |
| sha256 | `b8a006d30f45afe783072569f3d69c5b60c0e7b9ca28cd474e12f2584e2a3bd9` |
| Licence | BSD 3-Clause, `highlightjs/LICENSE` |
| Vendored | `highlight.js` from the package, and its `LICENSE` |
| File sha256 | `highlight.js` `4b77ecc2fc67ad3bfc5a9b0ff768accad2ab35ec2e0296594f5a47d55310fc8c`, `LICENSE` `6c081431591d9df696c82dc598fe1423765b8a299b200ed00b281afd0f64c490` |

`highlight.js` is the unminified "common" bundle of highlight.js's CDN build (`@highlightjs/cdn-assets`): a plain script that defines the global `hljs` and registers the languages below. `js_bytecode` compiles it, then `hljs_shim.js`, to QuickJS bytecode at build time (`//deps/thirdparty:hljs_bytecode`). The shim is ours, not vendored: it defines the global `render(code, language)` that `js::engine` calls, throws `{message: "unknown language"}` for a language `hljs.getLanguage()` doesn't know, and turns an error that highlight.js swallowed (`errorRaised`) into a thrown one. It never detects the language, and leaves `hljs.configure()` alone, so the class prefix is `hljs-`.

The 36 languages, as `hljs.listLanguages()` returns them from the bundle (a language is known by its name or an alias, in any case): `bash`, `c`, `cpp`, `csharp`, `css`, `diff`, `go`, `graphql`, `ini`, `java`, `javascript`, `json`, `kotlin`, `less`, `lua`, `makefile`, `markdown`, `objectivec`, `perl`, `php`, `php-template`, `plaintext`, `python`, `python-repl`, `r`, `ruby`, `rust`, `scss`, `shell`, `sql`, `swift`, `typescript`, `vbnet`, `wasm`, `xml`, `yaml`.

Aliases that matter:

| Language | Aliases |
|---|---|
| `cpp` | `c++`, `cc`, `cxx`, `hpp`, `hh`, `hxx`, `h++` |
| `c` | `h` |
| `plaintext` | `text`, `txt` (the code comes back escaped, with no spans) |
| `bash` | `sh`, `zsh` |
| `shell` | `console`, `shellsession` (a terminal session, with its prompts) |
| `javascript` | `js`, `jsx`, `mjs`, `cjs` |
| `typescript` | `ts`, `tsx`, `mts`, `cts` |
| `python` | `py` |
| `xml` | `html`, `xhtml`, `svg`, `rss`, `atom`, `xsd`, `xsl`, `plist` |
| `ini` | `toml` |
| `yaml` | `yml` |
| `markdown` | `md` |
| `rust` | `rs` |
| `objectivec` | `objc`, `obj-c`, `mm` |

The others are in the bundle's `getLanguage(name).aliases`. A language that is not in the bundle (`haskell`, `lisp`, ...) is an unknown language; `tests/highlight/hljs_test.cxx` checks two, and every one above.

To update: download the tarball for the new version into an empty directory, check its sha256 and integrity against the registry's, copy `package/highlight.js` and `package/LICENSE` over the vendored files without editing them, and change the version, URL and sha256s above. Check that `highlight.js` is still the unminified common bundle with a global `hljs`, and update the languages and aliases above from its `hljs.listLanguages()` and `hljs.getLanguage(name).aliases`: `tests/highlight/hljs_test.cxx` lists the languages. Check that `hljs_shim.js` still fits: it relies on `hljs.getLanguage()`, on `hljs.highlight(code, {language, ignoreIllegals})` and on the `errorRaised` of its result, and highlight.js must not start using what a bare context lacks (`console` is only reached for a broken grammar). To get a bigger or smaller set of languages, take the core and the language files instead of the common bundle, and record the choice. Then run `bazel test //...` and `bazel test //tests/highlight/... --config=asan`.
