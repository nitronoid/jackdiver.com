#include <blog/build.hxx>

#include <algorithm>
#include <cstddef>
#include <deque>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <blog/error.hxx>
#include <blog/file.hxx>
#include <blog/frame.hxx>
#include <blog/post.hxx>
#include <blog/render.hxx>
#include <blog/site.hxx>
#include <highlight/hljs.hxx>
#include <jimd/ast.hxx>
#include <jimd/parse.hxx>
#include <jimd/position.hxx>
#include <jimd/result.hxx>
#include <math/temml.hxx>

using namespace std::literals;

namespace blog
{
namespace
{
namespace fs = std::filesystem;

using errors_t = std::vector<file_error>;

auto add_error(errors_t& errors, fs::path const& path, error err) -> void
{
	errors.push_back({.path = path, .error = std::move(err)});
}

/*
 * One content file, from reading to rendering. The document views into text, so a file must
 * stay where it is: files live in a deque and are never copied or moved.
 */
struct content_file {
	fs::path path;
	std::string text;
	jimd::result<jimd::ast::document> doc; // an error if text didn't parse
	std::string fragment; // empty until rendered
	bool failed = false; // has an error, so is not rendered

	content_file(fs::path file, std::string source)
		: path{std::move(file)}
		, text{std::move(source)}
		, doc{jimd::parse(text)}
	{
	}
	content_file(content_file const&) = delete;
	content_file(content_file&&) = delete;
	auto operator=(content_file const&) -> content_file& = delete;
	auto operator=(content_file&&) -> content_file& = delete;
	~content_file() = default;
};

using content_files = std::deque<content_file>;

auto is_listable(fs::path const& dir, errors_t& errors) -> bool
{
	std::error_code status_error;
	auto const kind = fs::status(dir, status_error).type();
	if (kind == fs::file_type::not_found) {
		return false;
	}
	if (kind != fs::file_type::directory) {
		add_error(errors, dir, errc::read_failed);
		return false;
	}
	return true;
}

auto entries_of(fs::path const& dir, errc bad_name, errors_t& errors) -> std::vector<fs::path>
{
	std::error_code status_error;
	if (!is_listable(dir, errors)) {
		return {};
	}
	std::vector<fs::path> files;
	fs::directory_iterator iter{dir, status_error};
	for (; !status_error && iter != fs::directory_iterator{}; iter.increment(status_error)) {
		auto const& path = iter->path();
		auto const name = path.filename().string();
		if (name.starts_with('.')) {
			continue;
		}
		std::error_code entry_error;
		if (!fs::is_regular_file(path, entry_error)) {
			add_error(errors, path, error{bad_name, jimd::position{}, "not a file"});
		} else if (auto named = check_extension(bad_name, name); !named) {
			add_error(errors, path, std::move(named).error());
		} else {
			files.push_back(path);
		}
	}
	if (status_error) {
		add_error(errors, dir, errc::read_failed);
	}
	std::ranges::sort(files);
	return files;
}

/* Files that fail to read or parse are reported and left out, so every file here has a doc. */
auto load(fs::path const& dir, errc bad_name, errors_t& errors) -> content_files
{
	content_files files;
	for (auto const& path : entries_of(dir, bad_name, errors)) {
		auto text = read_file(path);
		if (!text) {
			add_error(errors, path, std::move(text).error());
			continue;
		}
		auto& file = files.emplace_back(path, *std::move(text));
		if (!file.doc) {
			add_error(errors, path, file.doc.error());
			files.pop_back();
		}
	}
	return files;
}

/* The files as build_site() wants them: input i is file i, so an entry's source indexes both. */
auto inputs_of(content_files const& files) -> std::vector<input>
{
	std::vector<input> inputs;
	inputs.reserve(files.size());
	for (auto const& file : files) {
		inputs.push_back({.path = file.path, .doc = *file.doc});
	}
	return inputs;
}

auto mark_failed(content_files& files, fs::path const& path) -> void
{
	if (auto const found = std::ranges::find(files, path, &content_file::path);
	    found != files.end()) {
		found->failed = true;
	}
}

struct output_file {
	std::string path;
	std::string content;
};

struct static_file {
	fs::path source;
	std::string path;
	std::string content;
};

auto walk_static(fs::path const& root, std::vector<static_file>& files, errors_t& errors) -> void
{
	std::error_code walk_error;
	fs::path dir = root; // where an iterator error is reported: the directory being read
	fs::recursive_directory_iterator iter{root, walk_error};
	for (; !walk_error && iter != fs::recursive_directory_iterator{}; iter.increment(walk_error)) {
		auto const& path = iter->path();
		std::error_code entry_error;
		bool const real_dir = !iter->is_symlink(entry_error) && iter->is_directory(entry_error);
		dir = real_dir ? path : path.parent_path();
		if (iter->is_regular_file(entry_error)) {
			if (path.filename() != ".DS_Store") {
				files.push_back({.source = path,
				                 .path = path.lexically_relative(root).generic_string(),
				                 .content = {}});
			}
		} else if (!real_dir) {
			add_error(errors, path, errc::read_failed);
		}
	}
	if (walk_error) {
		add_error(errors, dir, errc::read_failed);
	}
}

auto read_static(fs::path const& root, errors_t& errors) -> std::vector<static_file>
{
	std::vector<static_file> files;
	if (!is_listable(root, errors)) {
		return files;
	}
	walk_static(root, files, errors);
	std::ranges::sort(files, {}, &static_file::path);
	for (auto& file : files) {
		auto text = read_file(file.source);
		if (text) {
			file.content = *std::move(text);
		} else {
			add_error(errors, file.source, std::move(text).error());
		}
	}
	return files;
}

struct written_paths {
	std::set<std::string, std::less<>> files;
	std::map<std::string, std::string, std::less<>> dirs;

	explicit written_paths(std::vector<output_file> const& outputs)
	{
		for (auto const& file : outputs) {
			files.insert(file.path);
			for (auto slash = file.path.find('/'); slash != std::string::npos;
			     slash = file.path.find('/', slash + 1)) {
				dirs.emplace(file.path.substr(0, slash), file.path);
			}
		}
	}

	[[nodiscard]] auto clash(std::string_view path) const -> std::optional<std::string>
	{
		if (files.contains(path)) {
			return std::format("/{} is also written by the site", path);
		}
		if (auto const found = dirs.find(path); found != dirs.end()) {
			return std::format("/{} is a directory of /{}", path, found->second);
		}
		for (auto slash = path.find('/'); slash != std::string_view::npos;
		     slash = path.find('/', slash + 1)) {
			if (files.contains(path.substr(0, slash))) {
				return std::format("/{} is inside /{}, a file the site writes", path,
				                   path.substr(0, slash));
			}
		}
		return std::nullopt;
	}
};

auto write_file(fs::path const& path, std::string_view content) -> std::expected<void, error>
{
	std::error_code create_error;
	fs::create_directories(path.parent_path(), create_error);
	if (create_error) {
		return std::unexpected(errc::write_failed);
	}
	std::ofstream file{path, std::ios::binary};
	file.write(content.data(), static_cast<std::streamsize>(content.size()));
	file.close();
	if (!file) {
		return std::unexpected(errc::write_failed);
	}
	return {};
}

auto output_problem(fs::path const& output) -> bool
{
	std::error_code status_error;
	auto const kind = fs::status(output, status_error).type();
	if (kind == fs::file_type::not_found) {
		return false;
	}
	if (kind != fs::file_type::directory) {
		return true;
	}
	return fs::directory_iterator{output, status_error} != fs::directory_iterator{} || status_error;
}

auto index_path(std::string_view url) -> std::string
{
	return std::string{url.substr(1)} + "index.html";
}

auto render_all(content_files& files, math::temml& engine, highlight::hljs& highlighter,
                fs::path const& static_root, errors_t& errors) -> void
{
	for (auto& file : files) {
		if (file.failed) {
			continue;
		}
		auto text = render_fragment(
			*file.doc, {.engine = engine, .highlighter = highlighter, .static_root = static_root});
		if (text) {
			file.fragment = *std::move(text);
		} else {
			file.failed = true;
			add_error(errors, file.path, std::move(text).error());
		}
	}
}

auto pages_of(site_model const& model, content_files const& posts, content_files const& pages)
	-> std::vector<output_file>
{
	std::vector<output_file> out;
	auto const newest = model.posts.empty() ? std::string{} :
	                                          posts.at(model.posts.front().source).fragment;
	out.push_back({.path = "index.html", .content = frame_index(model, newest)});
	out.push_back({.path = "archive/index.html", .content = frame_archive(model)});
	out.push_back({.path = "404.html", .content = frame_not_found(model)});
	for (std::size_t idx = 0; idx < model.posts.size(); ++idx) {
		auto const& entry = model.posts.at(idx);
		out.push_back({.path = index_path(url(entry.item)),
		               .content = frame_post(model, idx, posts.at(entry.source).fragment)});
	}
	for (std::size_t idx = 0; idx < model.pages.size(); ++idx) {
		auto const& entry = model.pages.at(idx);
		out.push_back({.path = index_path(url(entry.item)),
		               .content = frame_page(model, idx, pages.at(entry.source).fragment)});
	}
	return out;
}
}

auto build(build_options const& options, math::temml& engine, highlight::hljs& highlighter)
	-> std::expected<void, std::vector<file_error>>
{
	errors_t errors;
	if (output_problem(options.output)) {
		return std::unexpected(errors_t{{.path = options.output, .error = errc::output_not_empty}});
	}
	std::error_code status_error;
	if (!fs::is_directory(options.content, status_error)) {
		return std::unexpected(errors_t{{.path = options.content, .error = errc::file_not_found}});
	}

	auto posts = load(options.content / "posts", errc::bad_post_name, errors);
	auto pages = load(options.content / "pages", errc::bad_page_name, errors);
	std::optional<std::string> css;
	if (auto text = read_file(options.css); text) {
		css = *std::move(text);
	} else {
		add_error(errors, options.css, std::move(text).error());
	}
	auto const static_root = options.content / "static";
	auto statics = read_static(static_root, errors);

	auto const post_inputs = inputs_of(posts);
	auto const page_inputs = inputs_of(pages);
	auto model = build_site({.posts = post_inputs, .pages = page_inputs});
	if (!model) {
		for (auto& err : model.error()) {
			mark_failed(posts, err.path);
			mark_failed(pages, err.path);
			errors.push_back(std::move(err));
		}
	}

	render_all(posts, engine, highlighter, static_root, errors);
	render_all(pages, engine, highlighter, static_root, errors);

	std::vector<output_file> outputs;
	if (model) {
		outputs = pages_of(*model, posts, pages);
		if (css) {
			outputs.push_back({.path = "style/main.css", .content = *css});
		}
		written_paths const written{outputs};
		for (auto const& file : statics) {
			if (auto const detail = written.clash(file.path)) {
				add_error(errors, file.source,
				          error{errc::static_collision, jimd::position{}, *detail});
			}
		}
	}

	if (!errors.empty()) {
		std::ranges::stable_sort(errors, {}, &file_error::path);
		return std::unexpected(std::move(errors));
	}

	for (auto& file : statics) {
		outputs.push_back({.path = std::move(file.path), .content = std::move(file.content)});
	}
	for (auto const& file : outputs) {
		if (auto done = write_file(options.output / file.path, file.content); !done) {
			return std::unexpected(
				errors_t{{.path = options.output / file.path, .error = done.error()}});
		}
	}
	return {};
}
}
