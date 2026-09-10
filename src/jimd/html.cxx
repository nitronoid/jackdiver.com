#include <jimd/html.hxx>

#include <algorithm>
#include <cstddef>
#include <deque>
#include <flat_map>
#include <format>
#include <functional>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include <jimd/ast.hxx>
#include <jimd/detail/align.hxx>
#include <jimd/detail/newline.hxx>
#include <jimd/detail/overloaded.hxx>
#include <jimd/detail/position.hxx>
#include <jimd/result.hxx>

namespace jimd::html
{
namespace
{
constexpr std::string_view blanks = " \t";

auto escape_into(std::string_view input, std::string& out, bool quotes) -> void
{
	for (char const chr : input) {
		switch (chr) {
		case '&':
			out += "&amp;";
			break;
		case '<':
			out += "&lt;";
			break;
		case '>':
			out += "&gt;";
			break;
		case '"':
			out += quotes ? "&quot;" : "\"";
			break;
		default:
			out += chr;
			break;
		}
	}
}

constexpr std::size_t max_heading_level = 6;

constexpr char32_t line_start = U'\n';
constexpr char32_t word_char = U'a';

[[nodiscard]] constexpr auto is_slug_char(char chr) noexcept -> bool
{
	return (chr >= 'a' && chr <= 'z') || (chr >= '0' && chr <= '9');
}

[[nodiscard]] constexpr auto apostrophe_size(std::string_view text) noexcept -> std::size_t
{
	constexpr std::string_view right_single_quote = "\xE2\x80\x99";
	if (text.starts_with('\'')) {
		return 1;
	}
	return text.starts_with(right_single_quote) ? right_single_quote.size() : 0;
}

struct decoded {
	char32_t code = U'\xFFFD';
	std::size_t size = 1;
};

struct sequence_shape {
	std::size_t size = 0;
	char32_t lead_bits = 0;
	char32_t least = 0;
};

[[nodiscard]] constexpr auto shape_of(unsigned char lead) noexcept -> std::optional<sequence_shape>
{
	constexpr unsigned char two_first = 0xC2;
	constexpr unsigned char three_first = 0xE0;
	constexpr unsigned char four_first = 0xF0;
	constexpr unsigned char four_last = 0xF4;
	constexpr sequence_shape two{.size = 2, .lead_bits = 0x1FU, .least = 0x80U};
	constexpr sequence_shape three{.size = 3, .lead_bits = 0x0FU, .least = 0x800U};
	constexpr sequence_shape four{.size = 4, .lead_bits = 0x07U, .least = 0x10000U};
	if (lead < two_first || lead > four_last) {
		return std::nullopt;
	}
	if (lead < three_first) {
		return two;
	}
	return lead < four_first ? three : four;
}

[[nodiscard]] auto decode_utf8(std::string_view text) -> decoded
{
	constexpr unsigned char ascii_limit = 0x80;
	constexpr unsigned char continuation_tag = 0xC0;
	constexpr char32_t payload_mask = 0x3FU;
	constexpr unsigned payload_bits = 6;
	auto const lead = static_cast<unsigned char>(text.front());
	if (lead < ascii_limit) {
		return {.code = lead, .size = 1};
	}
	auto const shape = shape_of(lead);
	if (!shape || text.size() < shape->size) {
		return {};
	}
	char32_t code = char32_t{lead} & shape->lead_bits;
	for (auto const byte : text.substr(1, shape->size - 1)) {
		auto const unit = static_cast<unsigned char>(byte);
		if ((unit & continuation_tag) != ascii_limit) {
			return {};
		}
		code = (code << payload_bits) | (char32_t{unit} & payload_mask);
	}
	if (code < shape->least) {
		return {};
	}
	return {.code = code, .size = shape->size};
}

[[nodiscard]] constexpr auto opens_quote(char32_t before) noexcept -> bool
{
	switch (before) {
	case U'\n':
	case U' ':
	case U'\t':
	case U'(':
	case U'[':
	case U'{':
	case U'/':
	case U'-':
	case U'\u2018':
	case U'\u201C':
	case U'\u2014':
	case U'\u2013':
		return true;
	default:
		return false;
	}
}

struct replacement {
	std::size_t consumed;
	std::string_view written;
	char32_t last;
};

[[nodiscard]] constexpr auto replacement_at(std::string_view text, char32_t last) noexcept
	-> std::optional<replacement>
{
	constexpr replacement ellipsis{.consumed = 3, .written = "\u2026", .last = U'\u2026'};
	constexpr replacement em_dash{.consumed = 2, .written = "\u2014", .last = U'\u2014'};
	constexpr replacement open_single{.consumed = 1, .written = "\u2018", .last = U'\u2018'};
	constexpr replacement close_single{.consumed = 1, .written = "\u2019", .last = U'\u2019'};
	constexpr replacement open_double{.consumed = 1, .written = "\u201C", .last = U'\u201C'};
	constexpr replacement close_double{.consumed = 1, .written = "\u201D", .last = U'\u201D'};
	if (text.starts_with("...")) {
		return ellipsis;
	}
	if (text.starts_with("--")) {
		return em_dash;
	}
	if (text.starts_with('\'')) {
		return opens_quote(last) ? open_single : close_single;
	}
	if (text.starts_with('"')) {
		return opens_quote(last) ? open_double : close_double;
	}
	return std::nullopt;
}

auto smarten_into(std::string_view text, char32_t before, std::string& out) -> char32_t
{
	char32_t last = before;
	while (!text.empty()) {
		if (auto const found = replacement_at(text, last)) {
			out += found->written;
			last = found->last;
			text.remove_prefix(found->consumed);
			continue;
		}
		auto const [code, size] = decode_utf8(text);
		out += text.substr(0, size);
		last = code;
		text.remove_prefix(size);
	}
	return last;
}

template <typename Target>
[[nodiscard]] auto resolve(ast::document const& doc, std::string_view key,
                           std::optional<Target> inline_target,
                           std::flat_map<std::string_view, Target> const& defined) -> result<Target>
{
	if (inline_target) {
		return *inline_target;
	}
	if (auto const found = defined.find(key); found != defined.end()) {
		return found->second;
	}
	return std::unexpected(detail::make_error(errc::undefined_reference, doc.source, key));
}

[[nodiscard]] auto resolve_image(ast::document const& doc, ast::image const& node)
	-> result<ast::image_def>
{
	return resolve<ast::image_def>(
		doc, node.key,
		node.target ? std::optional{ast::image_def{.target = *node.target, .width = node.width}} :
					  std::nullopt,
		doc.defs.images);
}

[[nodiscard]] constexpr auto is_svg(std::string_view target) noexcept -> bool
{
	return target.ends_with(".svg");
}

[[nodiscard]] constexpr auto is_unsafe_in_url(char chr) noexcept -> bool
{
	constexpr unsigned char last_blank_or_control = ' ';
	constexpr unsigned char delete_char = 0x7F;
	auto const byte = static_cast<unsigned char>(chr);
	return byte <= last_blank_or_control || byte == delete_char ||
	       std::string_view{"'\\()"}.contains(chr);
}

[[nodiscard]] auto find_ratio(std::flat_map<std::string, std::string, std::less<>> const& ratios,
                              std::string_view target) -> std::optional<std::string_view>
{
	if (auto const found = ratios.find(target); found != ratios.end()) {
		return found->second;
	}
	return std::nullopt;
}

[[nodiscard]] auto figure_image(ast::paragraph const& node) -> ast::image const*
{
	ast::image const* found = nullptr;
	for (auto const& child : node.children) {
		if (auto const* const image = std::get_if<ast::image>(&child)) {
			if (found != nullptr) {
				return nullptr;
			}
			found = image;
		} else if (auto const* const text = std::get_if<ast::text>(&child);
		           text == nullptr ||
		           text->value.find_first_not_of(blanks) != std::string_view::npos) {
			return nullptr;
		}
	}
	return found;
}

[[nodiscard]] constexpr auto language_of(std::string_view info) -> std::string_view
{
	return info.substr(0, info.find_first_of(blanks));
}

[[nodiscard]] auto holds_anchor(ast::inlines const& nodes) -> bool
{
	return std::ranges::any_of(nodes, [](ast::inline_node const& node) -> bool {
		return std::visit(
			detail::overloaded{
				[](ast::link const& /*item*/) -> bool { return true; },
				[](ast::reference const& /*item*/) -> bool { return true; },
				[](ast::emphasis const& item) -> bool { return holds_anchor(item.children); },
				[](ast::strong const& item) -> bool { return holds_anchor(item.children); },
				[](auto const& /*leaf*/) -> bool { return false; },
			},
			node);
	});
}

class renderer {
public:
	renderer(ast::document const& doc, render_options const& options) noexcept
		: m_doc{&doc}
		, m_options{&options}
	{
	}

	[[nodiscard]] auto take() && noexcept -> std::string
	{
		return std::move(m_out);
	}

	auto render_top_level(ast::block const& node) -> result<void>
	{
		TRY(render_block(node));
		return write_notes();
	}

	auto render_block(ast::block const& node) -> result<void>
	{
		return std::visit([this](auto const& item) -> result<void> { return this->emit(item); },
		                  node);
	}

	auto render_inline(ast::inline_node const& node) -> result<void>
	{
		return std::visit([this](auto const& item) -> result<void> { return this->emit(item); },
		                  node);
	}

private:
	auto emit(ast::paragraph const& node) -> result<void>
	{
		m_before = line_start;
		if (auto const* const figure = figure_image(node)) {
			auto const def = TRY(resolve_image(*m_doc, *figure));
			if (is_svg(def.target)) {
				return emit_svg_figure(*figure, def);
			}
		}
		m_out += "<p>";
		for (auto const& child : node.children) {
			TRY(render_inline(child));
		}
		m_out += "</p>\n";
		return {};
	}

	auto emit(ast::heading const& node) -> result<void>
	{
		auto const tag =
			std::format("h{}", std::clamp<std::size_t>(node.level, 1, max_heading_level));
		auto const slug = claim_id(slugify(node.children));
		m_before = line_start;
		m_out += std::format("<{} id=\"{}\">", tag, slug);
		auto const permalink = !holds_anchor(node.children);
		if (permalink) {
			m_out += std::format("<a href=\"#{}\">", slug);
		}
		for (auto const& child : node.children) {
			TRY(render_inline(child));
		}
		if (permalink) {
			m_out += "</a>";
		}
		m_out += std::format("</{}>\n", tag);
		return {};
	}

	auto emit(ast::quote const& node) -> result<void>
	{
		m_out += "<blockquote>\n";
		for (auto const& child : node.children) {
			TRY(render_block(child));
		}
		m_out += "</blockquote>\n";
		return {};
	}

	auto emit(ast::code_block const& node) -> result<void>
	{
		auto const language = language_of(node.info);
		auto const body = detail::crlf_to_lf(node.body);
		auto const highlighted = code_entry(language, body);
		m_out += highlighted ? "<pre class=\"highlight\"><code" : "<pre><code";
		if (!language.empty()) {
			m_out += " class=\"language-";
			escape_attr(language, m_out);
			m_out += '"';
		}
		m_out += '>';
		if (highlighted) {
			m_out += *highlighted;
		} else {
			escape_text(body, m_out);
		}
		m_out += "</code></pre>\n";
		return {};
	}

	[[nodiscard]] auto code_entry(std::string_view language, std::string_view body) const
		-> std::optional<std::string_view>
	{
		auto const* const table = m_options->code;
		if (table == nullptr || language.empty()) {
			return std::nullopt;
		}
		if (auto const found = table->find(std::pair{language, body}); found != table->end()) {
			return found->second;
		}
		return std::nullopt;
	}

	auto emit(ast::list const& node) -> result<void>
	{
		m_out += "<ul>\n";
		for (auto const& item : node.items) {
			m_before = line_start;
			m_out += "<li>";
			for (auto const& child : item) {
				TRY(render_inline(child));
			}
			m_out += "</li>\n";
		}
		m_out += "</ul>\n";
		return {};
	}

	auto emit(ast::text const& node) -> result<void>
	{
		write_text(node.value);
		return {};
	}

	auto emit(ast::soft_break const& /*unused*/) -> result<void>
	{
		m_out += '\n';
		m_before = line_start;
		return {};
	}

	auto emit(ast::code_span const& node) -> result<void>
	{
		m_before = word_char;
		m_out += "<code>";
		escape_text(node.value, m_out);
		m_out += "</code>";
		return {};
	}

	auto emit(ast::emphasis const& node) -> result<void>
	{
		return wrap_inlines("em", node.children);
	}

	auto emit(ast::strong const& node) -> result<void>
	{
		return wrap_inlines("strong", node.children);
	}

	auto wrap_inlines(std::string_view tag, ast::inlines const& children) -> result<void>
	{
		m_out += '<';
		m_out += tag;
		m_out += '>';
		for (auto const& child : children) {
			TRY(render_inline(child));
		}
		m_out += "</";
		m_out += tag;
		m_out += '>';
		return {};
	}

	auto emit(ast::link const& node) -> result<void>
	{
		auto const target =
			TRY(resolve<std::string_view>(*m_doc, node.key, node.target, m_doc->defs.links));
		m_out += "<a href=\"";
		escape_attr(target, m_out);
		m_out += "\">";
		write_text(node.text);
		m_out += "</a>";
		return {};
	}

	auto emit(ast::image const& node) -> result<void>
	{
		auto const def = TRY(resolve_image(*m_doc, node));
		m_before = word_char;
		m_out += "<img src=\"";
		escape_attr(def.target, m_out);
		m_out += "\" alt=\"";
		escape_attr(node.alt, m_out);
		if (def.width) {
			m_out += "\" style=\"width: ";
			escape_attr(*def.width, m_out);
		}
		m_out += "\">";
		return {};
	}

	auto emit_svg_figure(ast::image const& node, ast::image_def const& def) -> result<void>
	{
		if (auto const* const bad = std::ranges::find_if(def.target, is_unsafe_in_url);
		    bad != def.target.end()) {
			return std::unexpected(detail::make_error(
				errc::invalid_target, m_doc->source,
				def.target.substr(static_cast<std::size_t>(bad - def.target.begin()), 1)));
		}
		auto const* const ratios = m_options->image_ratios;
		auto const found = ratios == nullptr ? std::nullopt : find_ratio(*ratios, def.target);
		if (!found) {
			return std::unexpected(
				detail::make_error(errc::unknown_image_size, m_doc->source, node.key));
		}
		m_out += R"(<div class="ink-mask")";
		if (node.alt.empty()) {
			m_out += R"( aria-hidden="true")";
		} else {
			m_out += R"( role="img" aria-label=")";
			escape_attr(node.alt, m_out);
			m_out += '"';
		}
		m_out += R"( style="--src: url(')";
		escape_attr(def.target, m_out);
		m_out += "'); --ratio: ";
		escape_attr(*found, m_out);
		if (def.width) {
			m_out += "; --width: ";
			escape_attr(*def.width, m_out);
		}
		m_out += "\"></div>\n";
		return {};
	}

	auto emit(ast::math const& node) -> result<void>
	{
		auto const entry = math_entry(math_mode::inline_math, node.tex);
		if (!entry) {
			return unrendered(node.tex);
		}
		m_before = word_char;
		m_out += *entry;
		return {};
	}

	auto emit(ast::display_math const& node) -> result<void>
	{
		auto const entry = math_entry(math_mode::display, ast::math_tex(node));
		if (!entry) {
			return unrendered(node.tex);
		}
		m_out += *entry;
		m_out += '\n';
		return {};
	}

	auto emit(ast::aligned_math const& node) -> result<void>
	{
		auto const entry = math_entry(math_mode::display, ast::math_tex(node));
		if (!entry) {
			return unrendered(node.rows.front());
		}
		m_out += *entry;
		m_out += '\n';
		return {};
	}

	[[nodiscard]] auto math_entry(math_mode mode, std::string_view tex) const
		-> std::optional<std::string_view>
	{
		auto const* const table = m_options->math;
		if (table == nullptr) {
			return std::nullopt;
		}
		if (auto const found = table->find(std::pair{mode, tex}); found != table->end()) {
			return found->second;
		}
		return std::nullopt;
	}

	[[nodiscard]] auto unrendered(std::string_view tex) const -> result<void>
	{
		return std::unexpected(detail::make_error(errc::unrendered_math, m_doc->source, tex));
	}

	auto emit(ast::reference const& node) -> result<void>
	{
		m_before = word_char;
		if (!node.content) {
			if (auto const known = m_footnotes.find(node.key); known != m_footnotes.end()) {
				m_out += std::format(R"(<sup class="note-ref"><a href="#{}">{}</a></sup>)",
				                     known->second.id, known->second.number);
				return {};
			}
		}
		auto const& content = *TRY(footnote_content(node));
		auto const number = ++m_note_count;
		auto const note_id = claim_id(std::format("note-{}", number));
		auto const ref_id = claim_id(std::format("noteref-{}", number));
		if (!node.content) {
			m_footnotes.emplace(node.key, footnote{.number = number, .id = note_id});
		}
		m_pending.push_back(pending_note{.number = number, .id = note_id, .content = &content});
		m_out += std::format(
			R"(<sup class="note-ref" style="anchor-name: --note-{0}"><a href="#{1}" id="{2}">{0}</a></sup>)",
			number, note_id, ref_id);
		return {};
	}

	auto write_notes() -> result<void>
	{
		while (!m_pending.empty()) {
			auto const note = std::move(m_pending.front());
			m_pending.pop_front();
			m_before = line_start;
			m_out +=
				std::format(R"(<aside id="{}" data-note="{}" style="position-anchor: --note-{}">)"
				            "\n<p>",
				            note.id, note.number, note.number);
			for (auto const& child : *note.content) {
				TRY(render_inline(child));
			}
			m_out += "</p>\n</aside>\n";
		}
		return {};
	}

	[[nodiscard]] auto footnote_content(ast::reference const& node) const
		-> result<ast::inlines const*>
	{
		if (node.content) {
			return &*node.content;
		}
		if (auto const found = m_doc->defs.footnotes.find(node.key);
		    found != m_doc->defs.footnotes.end()) {
			return &found->second;
		}
		return std::unexpected(
			detail::make_error(errc::undefined_reference, m_doc->source, node.key));
	}

	auto write_text(std::string_view text) -> void
	{
		std::string smart;
		m_before = smarten_into(text, m_before, smart);
		escape_text(smart, m_out);
	}

	auto claim_id(std::string const& base) -> std::string
	{
		auto claimed = base;
		for (std::size_t suffix = 2; !m_used_ids.insert(claimed).second; ++suffix) {
			claimed = std::format("{}-{}", base, suffix);
		}
		return claimed;
	}

	struct footnote {
		std::size_t number{};
		std::string id;
	};

	struct pending_note {
		std::size_t number{};
		std::string id;
		ast::inlines const* content = nullptr;
	};

	ast::document const* m_doc;
	render_options const* m_options;
	std::string m_out;
	std::unordered_set<std::string> m_used_ids;
	std::unordered_map<std::string_view, footnote> m_footnotes;
	std::size_t m_note_count = 0;
	std::deque<pending_note> m_pending;
	char32_t m_before = line_start;
};

auto locate_in_body(std::string_view origin, std::size_t offset) -> std::string_view
{
	std::size_t index = 0;
	for (std::size_t pos = 0; pos < origin.size(); ++pos) {
		if (origin.substr(pos).starts_with("\r\n")) {
			continue;
		}
		if (index == offset) {
			return origin.substr(pos, 0);
		}
		++index;
	}
	return origin.substr(origin.size(), 0);
}

auto locate_in_rows(std::vector<std::string_view> const& rows, std::size_t offset)
	-> std::string_view
{
	if (offset < detail::align_begin.size()) {
		return rows.front().substr(0, 0);
	}
	offset -= detail::align_begin.size();
	for (auto const row : rows | std::views::take(rows.size() - 1)) {
		if (offset < row.size()) {
			return row.substr(offset, 0);
		}
		offset -= row.size();
		if (offset < detail::align_join.size()) {
			return row.substr(row.size(), 0);
		}
		offset -= detail::align_join.size();
	}
	auto const last = rows.back();
	return last.substr(std::min(offset, last.size()), 0);
}

auto collect(ast::inlines const& nodes, std::vector<math_request>& out) -> void;

auto collect(ast::block const& node, std::vector<math_request>& out) -> void
{
	auto const visitor = detail::overloaded{
		[&out](ast::paragraph const& item) -> void { collect(item.children, out); },
		[&out](ast::heading const& item) -> void { collect(item.children, out); },
		[&out](ast::quote const& item) -> void {
			for (auto const& child : item.children) {
				collect(child, out);
			}
		},
		[&out](ast::list const& item) -> void {
			for (auto const& entry : item.items) {
				collect(entry, out);
			}
		},
		[&out](ast::display_math const& item) -> void {
			out.push_back(math_request{.mode = math_mode::display,
			                           .tex = ast::math_tex(item),
			                           .origin = item.tex,
			                           .rows = {}});
		},
		[&out](ast::aligned_math const& item) -> void {
			auto const first = item.rows.front();
			auto const last = item.rows.back();
			out.push_back(math_request{.mode = math_mode::display,
			                           .tex = ast::math_tex(item),
			                           .origin = std::string_view{first.begin(), last.end()},
			                           .rows = item.rows});
		},
		[](auto const& /*leaf*/) -> void {},
	};
	std::visit(visitor, node);
}

auto collect(ast::inlines const& nodes, std::vector<math_request>& out) -> void
{
	auto const visitor = detail::overloaded{
		[&out](ast::emphasis const& item) -> void { collect(item.children, out); },
		[&out](ast::strong const& item) -> void { collect(item.children, out); },
		[&out](ast::reference const& item) -> void {
			if (item.content) {
				collect(*item.content, out);
			}
		},
		[&out](ast::math const& item) -> void {
			out.push_back(math_request{.mode = math_mode::inline_math,
			                           .tex = ast::math_tex(item),
			                           .origin = item.tex,
			                           .rows = {}});
		},
		[](auto const& /*leaf*/) -> void {},
	};
	for (auto const& node : nodes) {
		std::visit(visitor, node);
	}
}
}

auto math_request::locate(std::size_t offset) const -> std::string_view
{
	return rows.empty() ? locate_in_body(origin, offset) : locate_in_rows(rows, offset);
}

auto math_requests(ast::document const& doc) -> std::vector<math_request>
{
	std::vector<math_request> requests;
	for (auto const& block : doc.blocks) {
		collect(block, requests);
	}
	for (auto const& entry : doc.defs.footnotes) {
		collect(entry.second, requests);
	}
	/*
	 * Source order is the order of the origins in doc.source. A node of a tree not built by
	 * parse() can have its text outside it, with no position to sort by, so those go last in
	 * the order they were collected, blocks before definitions.
	 */
	auto const placed = [&doc](math_request const& request) -> bool {
		return detail::contains(doc.source, request.origin);
	};
	std::ranges::stable_sort(
		requests, [&placed](math_request const& left, math_request const& right) -> bool {
			auto const left_placed = placed(left);
			if (left_placed != placed(right)) {
				return left_placed;
			}
			return left_placed && std::less<char const*>{}(left.origin.data(), right.origin.data());
		});
	return requests;
}

namespace
{
auto collect_code(ast::block const& block, std::vector<code_request>& out) -> void
{
	if (auto const* const quote = std::get_if<ast::quote>(&block)) {
		for (auto const& child : quote->children) {
			collect_code(child, out);
		}
	} else if (auto const* const code = std::get_if<ast::code_block>(&block)) {
		if (auto const language = language_of(code->info); !language.empty()) {
			out.push_back(
				code_request{.language = language, .body = detail::crlf_to_lf(code->body)});
		}
	}
}
}

auto code_requests(ast::document const& doc) -> std::vector<code_request>
{
	std::vector<code_request> requests;
	for (auto const& block : doc.blocks) {
		collect_code(block, requests);
	}
	return requests;
}

auto is_figure(ast::paragraph const& node) -> bool
{
	return figure_image(node) != nullptr;
}

namespace
{
auto collect_svg_figures(ast::document const& doc, ast::block const& block,
                         std::vector<svg_figure>& out) -> void
{
	if (auto const* const quote = std::get_if<ast::quote>(&block)) {
		for (auto const& child : quote->children) {
			collect_svg_figures(doc, child, out);
		}
	} else if (auto const* const paragraph = std::get_if<ast::paragraph>(&block)) {
		if (auto const* const image = figure_image(*paragraph)) {
			if (auto const def = resolve_image(doc, *image); def && is_svg(def->target)) {
				out.push_back(svg_figure{.target = def->target, .key = image->key});
			}
		}
	}
}
}

auto svg_figures(ast::document const& doc) -> std::vector<svg_figure>
{
	std::vector<svg_figure> figures;
	for (auto const& block : doc.blocks) {
		collect_svg_figures(doc, block, figures);
	}
	return figures;
}

auto smarten(std::string_view text, char32_t before) -> std::string
{
	std::string out;
	smarten_into(text, before, out);
	return out;
}

auto escape_text(std::string_view input, std::string& out) -> void
{
	escape_into(input, out, false);
}

auto escape_attr(std::string_view input, std::string& out) -> void
{
	escape_into(input, out, true);
}

auto slugify(ast::inlines const& nodes) -> std::string
{
	std::string slug;
	bool pending_dash = false;
	auto const text = ast::plain_text(nodes);
	for (std::size_t pos = 0; pos < text.size(); ++pos) {
		if (auto const size = apostrophe_size(std::string_view{text}.substr(pos)); size != 0) {
			pos += size - 1;
			continue;
		}
		char chr = text.at(pos);
		if (chr >= 'A' && chr <= 'Z') {
			chr = static_cast<char>(chr - 'A' + 'a');
		}
		if (!is_slug_char(chr)) {
			pending_dash = true;
			continue;
		}
		if (pending_dash && !slug.empty()) {
			slug += '-';
		}
		pending_dash = false;
		slug += chr;
	}
	return slug.empty() ? "section" : slug;
}

auto render(ast::document const& doc, render_options const& options) -> result<std::string>
{
	renderer rend{doc, options};
	for (auto const& block : doc.blocks) {
		TRY(rend.render_top_level(block));
	}
	return std::move(rend).take();
}
}
