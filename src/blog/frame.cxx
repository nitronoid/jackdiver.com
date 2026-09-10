#include <blog/frame.hxx>

#include <chrono>
#include <cstddef>
#include <format>
#include <optional>
#include <string>
#include <string_view>

#include <blog/post.hxx>
#include <blog/site.hxx>
#include <jimd/html.hxx>

using namespace std::literals;

namespace blog
{
namespace
{
constexpr auto site_name = "Jack Diver"sv;

/*
 * Inline and blocking so a saved theme applies before the first paint. The deferred
 * theme.js only drives the toggle.
 */
constexpr auto theme_script = R"(  <script>
    (function () {
      try {
        var t = localStorage.getItem('theme');
        if (t === 'light' || t === 'dark') {
          document.documentElement.style.colorScheme = t;
        }
      } catch (e) {
      }
    })();
  </script>
)"sv;

struct head_info {
	std::optional<std::string_view> title;
	std::optional<std::string_view> description;
};

auto text_of(std::string_view title) -> std::string
{
	std::string out;
	jimd::html::escape_text(jimd::html::smarten(title), out);
	return out;
}

auto head_of(head_info const& info) -> std::string
{
	std::string out = "<!DOCTYPE html>\n"
					  "<html lang=\"en\">\n"
					  "<head>\n"
					  "  <meta charset=\"utf-8\">\n"
					  "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
					  "  <title>";
	if (info.title) {
		out += text_of(*info.title);
		out += " – ";
	}
	out += text_of(site_name);
	out += "</title>\n";
	if (info.description) {
		out += R"(  <meta name="description" content=")";
		jimd::html::escape_attr(*info.description, out);
		out += "\">\n";
	}
	out += theme_script;
	out += "  <link rel=\"stylesheet\" href=\"/style/main.css\">\n"
		   "  <link rel=\"preload\" as=\"font\" type=\"font/woff2\" crossorigin\n"
		   "        href=\"/font/source-serif-4-roman.woff2\">\n"
		   "  <link rel=\"icon\" href=\"/favicon.svg\" type=\"image/svg+xml\">\n"
		   "  <script src=\"/theme.js\" defer></script>\n"
		   "</head>\n";
	return out;
}

auto header_of(site_model const& model) -> std::string
{
	std::string out = "<header class=\"site-header\">\n"
					  "  <a class=\"site-title\" href=\"/\">";
	out += text_of(site_name);
	out += "</a>\n"
		   "  <nav class=\"site-nav\">\n"
		   "    <a href=\"/archive/\">archive</a>\n";
	for (auto const& entry : model.pages) {
		out +=
			std::format("    <a href=\"{}\">{}</a>\n", url(entry.item), text_of(entry.item.title));
	}
	out += "    <button type=\"button\" class=\"theme-toggle\" data-theme-toggle hidden>\n"
		   "      <span class=\"theme-toggle-label\" aria-hidden=\"true\"></span>\n"
		   "    </button>\n"
		   "  </nav>\n"
		   "</header>\n";
	return out;
}

auto document_of(head_info const& info, site_model const& model, std::string_view main_content)
	-> std::string
{
	std::string out = head_of(info);
	out += "<body>\n";
	out += header_of(model);
	out += "<main>\n";
	out += main_content;
	out += "</main>\n"
		   "</body>\n"
		   "</html>\n";
	return out;
}

auto fragment_lines(std::string_view fragment) -> std::string
{
	std::string out{fragment};
	if (!out.empty() && !out.ends_with('\n')) {
		out += '\n';
	}
	return out;
}

auto post_top(post_entry const& entry) -> std::string
{
	return std::format("    <h1>{}</h1>\n"
	                   "    <nav class=\"post-meta\">\n"
	                   "      <span class=\"date\">{}</span>\n"
	                   "    </nav>\n",
	                   text_of(entry.item.title), long_date(entry.item.date));
}

auto nav_link(std::string_view css_class, std::string_view direction, post const& target)
	-> std::string
{
	return std::format("      <a class=\"{}\" href=\"{}\">\n"
	                   "        <span class=\"direction\">{}</span>\n"
	                   "        <span class=\"title\">{}</span>\n"
	                   "      </a>\n",
	                   css_class, url(target), direction, text_of(target.title));
}

auto series_nav(site_model const& model, series_position const& place) -> std::string
{
	auto const& group = model.series.at(place.group);
	std::string out =
		std::format("    <nav class=\"series-nav\" aria-label=\"Series navigation\">\n"
		            "      <p class=\"series-label\">Part {} of {} in a series</p>\n"
		            "      <ol class=\"series-parts\">\n",
		            place.part + 1, group.posts.size());
	for (std::size_t part = 0; part < group.posts.size(); ++part) {
		auto const& item = model.posts.at(group.posts.at(part)).item;
		if (part == place.part) {
			out += std::format("        <li class=\"current\" aria-current=\"true\">{}</li>\n",
			                   text_of(item.title));
		} else {
			out += std::format("        <li><a href=\"{}\">{}</a></li>\n", url(item),
			                   text_of(item.title));
		}
	}
	out += "      </ol>\n"
		   "    </nav>\n";
	return out;
}

auto series_footer(site_model const& model, series_position const& place) -> std::string
{
	auto const& group = model.series.at(place.group);
	std::string links;
	if (place.part > 0) {
		links += nav_link("previous", "← Previous part",
		                  model.posts.at(group.posts.at(place.part - 1)).item);
	}
	if (place.part + 1 < group.posts.size()) {
		links +=
			nav_link("next", "Next part →", model.posts.at(group.posts.at(place.part + 1)).item);
	}
	if (links.empty()) {
		return {};
	}
	return "    <nav class=\"series-footer\" aria-label=\"Series parts\">\n" + links +
	       "    </nav>\n";
}

auto post_nav(site_model const& model, post_entry const& entry) -> std::string
{
	std::string links;
	if (entry.older) {
		links += nav_link("older", "← Older", model.posts.at(*entry.older).item);
	}
	if (entry.newer) {
		links += nav_link("newer", "Newer →", model.posts.at(*entry.newer).item);
	}
	if (links.empty()) {
		return {};
	}
	return "    <nav class=\"post-nav\" aria-label=\"Adjacent posts\">\n" + links + "    </nav>\n";
}
}

auto frame_post(site_model const& model, std::size_t index, std::string_view fragment)
	-> std::string
{
	auto const& entry = model.posts.at(index);
	std::string body = "  <article class=\"post\">\n";
	body += post_top(entry);
	if (entry.series) {
		body += series_nav(model, *entry.series);
	}
	body += fragment_lines(fragment);
	if (entry.series) {
		body += series_footer(model, *entry.series);
	}
	body += post_nav(model, entry);
	body += "  </article>\n";
	return document_of({.title = entry.item.title, .description = entry.item.description}, model,
	                   body);
}

auto frame_page(site_model const& model, std::size_t index, std::string_view fragment)
	-> std::string
{
	auto const& entry = model.pages.at(index);
	std::string body = "  <article class=\"page\">\n";
	body += std::format("    <h1>{}</h1>\n", text_of(entry.item.title));
	body += fragment_lines(fragment);
	body += "  </article>\n";
	return document_of({.title = entry.item.title, .description = entry.item.description}, model,
	                   body);
}

auto frame_index(site_model const& model, std::string_view fragment) -> std::string
{
	std::string body;
	if (!model.posts.empty()) {
		body = "  <article class=\"post\">\n";
		body += post_top(model.posts.front());
		body += fragment_lines(fragment);
		body += "  </article>\n";
		body += std::format("  <p class=\"more\"><a href=\"/archive/\">All {} {}…</a></p>\n",
		                    model.posts.size(), model.posts.size() == 1 ? "post" : "posts");
	}
	return document_of({.title = std::nullopt, .description = std::nullopt}, model, body);
}

auto frame_archive(site_model const& model) -> std::string
{
	std::string body = "  <article class=\"archive-index\">\n"
					   "    <h1>Archive</h1>\n";
	if (!model.posts.empty()) {
		body += "    <table class=\"archive\">\n"
				"      <tbody>\n";
		std::optional<std::chrono::year> year;
		for (auto const& entry : model.posts) {
			auto const& item = entry.item;
			bool const starts_year = year != item.date.year();
			std::string const year_cell =
				starts_year ? std::format(R"(<th class="year" scope="rowgroup">{}</th>)",
				                          static_cast<int>(item.date.year())) :
							  R"(<th class="year" scope="row"></th>)";
			body += std::format("        <tr{}>\n"
			                    "          {}\n"
			                    "          <td class=\"title\"><a href=\"{}\">{}</a></td>\n"
			                    "          <td class=\"date\">{}</td>\n"
			                    "        </tr>\n",
			                    starts_year ? " class=\"year-start\"" : "", year_cell, url(item),
			                    text_of(item.title), short_date(item.date));
			year = item.date.year();
		}
		body += "      </tbody>\n"
				"    </table>\n";
	}
	body += "  </article>\n";
	return document_of({.title = "Archive"sv, .description = std::nullopt}, model, body);
}

auto frame_not_found(site_model const& model) -> std::string
{
	constexpr auto body =
		"  <article class=\"page\">\n"
		"    <h1>Page not found</h1>\n"
		"    <p>There is nothing here. Try the <a href=\"/\">home page</a> or the "
		"<a href=\"/archive/\">archive</a>.</p>\n"
		"  </article>\n"sv;
	return document_of({.title = "Page not found"sv, .description = std::nullopt}, model, body);
}
}
