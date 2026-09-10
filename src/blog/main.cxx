#include <cstddef>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <span>
#include <stdexcept>
#include <string>

#include <blog/args.hxx>
#include <blog/build.hxx>
#include <blog/error.hxx>
#include <highlight/hljs.hxx>
#include <math/temml.hxx>

auto main(int argc, char* argv[]) -> int
try {
	auto const args = std::span<char const* const>{argv, static_cast<std::size_t>(argc)}.subspan(1);
	auto const opts = blog::parse_args(args);
	if (!opts) {
		blog::handle_error(opts.error());
		return 1;
	}

	auto engine = math::temml::create();
	if (!engine) {
		throw std::runtime_error{"cannot start the maths engine: " + engine.error().message};
	}
	auto highlighter = highlight::hljs::create();
	if (!highlighter) {
		throw std::runtime_error{"cannot start the highlighter: " + highlighter.error().message};
	}
	auto const built = blog::build({.content = std::filesystem::path{opts->content},
	                                .css = std::filesystem::path{opts->css},
	                                .output = std::filesystem::path{opts->output}},
	                               *engine, *highlighter);
	if (!built) {
		for (auto const& err : built.error()) {
			blog::handle_error(err.error, err.path.string());
		}
		return 1;
	}
	return 0;
} catch (std::exception const& ex) {
	std::fputs("fatal: ", stderr);
	std::fputs(ex.what(), stderr);
	std::fputc('\n', stderr);
	return 1;
} catch (...) {
	std::fputs("fatal: unknown exception\n", stderr);
	return 1;
}
