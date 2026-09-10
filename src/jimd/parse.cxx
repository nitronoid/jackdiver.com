#include <jimd/parse.hxx>
#include <jimd/detail/overloaded.hxx>
#include <jimd/detail/position.hxx>
#include <jimd/grammar.hxx>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <optional>
#include <print>
#include <ranges>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include <tao/pegtl.hpp>
#include <tao/pegtl/extra/parse_tree.hpp>

namespace jimd
{
namespace
{
namespace pegtl = tao::pegtl;

template <std::size_t Size> struct fixed_string {
	std::array<char, Size> value{};

	// NOLINTNEXTLINE(cppcoreguidelines-explicit-constructor,google-explicit-constructor,cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)
	constexpr fixed_string(char const (&source)[Size]) noexcept
	{
		// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
		std::copy_n(source, Size, value.data());
	}

	[[nodiscard]] constexpr auto view() const noexcept -> std::string_view
	{
		return {value.data(), Size - 1};
	}
};

template <fixed_string Prefix, fixed_string Postfix = Prefix>
struct strip : pegtl::parse_tree::apply<strip<Prefix, Postfix>> {
	template <typename Node, typename... States>
	static constexpr auto transform(std::unique_ptr<Node>& node, States&&... /*unused*/) noexcept
		-> void
	{
		auto text = node->data;
		if (text.starts_with(Prefix.view())) {
			text.remove_prefix(Prefix.view().size());
		}
		if (text.ends_with(Postfix.view())) {
			text.remove_suffix(Postfix.view().size());
		}
		node->data = text;
	}
};

template <typename Rule>
struct selector
    : pegtl::parse_tree::selector<
		  Rule,
		  pegtl::parse_tree::remove_content::on<
			  grammar::doc, grammar::paragraph, grammar::header, grammar::quote,
			  grammar::bullet_list, grammar::list_item, grammar::block_code, grammar::block_math,
			  grammar::block_aligned, grammar::link, grammar::image, grammar::reference,
			  grammar::link_def, grammar::image_def, grammar::reference_def, grammar::italic,
			  grammar::bold, grammar::footnote_target, grammar::metadata_def>,
		  pegtl::parse_tree::store_content::on<
			  grammar::header_prefix, grammar::text, grammar::intra_newline, grammar::code_info,
			  grammar::code_body, grammar::math_body, grammar::aligned_body, grammar::escape_char,
			  grammar::footnote_text, grammar::metadata_key, grammar::metadata_value,
			  grammar::resource_def_content>,
		  strip<"`">::on<grammar::inline_code>, strip<"$">::on<grammar::inline_math>,
		  strip<"[@", "]">::on<grammar::link_id>, strip<"[!", "]">::on<grammar::image_id>,
		  strip<"[^", "]">::on<grammar::reference_id>,
		  strip<"(", ")">::on<grammar::resource_target>> {};

struct syntax_failure : std::exception {
	errc code{};
	std::size_t line{};
	std::size_t column{};

	syntax_failure(errc failure_code, pegtl::text_position const& pos) noexcept
		: code{failure_code}
		, line{pos.line}
		, column{pos.column}
	{
	}
};

template <typename Rule> inline constexpr errc error_code_v = errc::syntax;

template <> inline constexpr errc error_code_v<grammar::quote_close> = errc::unclosed_quote;
template <> inline constexpr errc error_code_v<grammar::invalid_escape> = errc::invalid_escape;
template <> inline constexpr errc error_code_v<grammar::nested_footnote> = errc::nested_footnote;
template <> inline constexpr errc error_code_v<grammar::empty_emphasis> = errc::empty_emphasis;
template <>
inline constexpr errc error_code_v<grammar::emphasis_close<'_'>> = errc::unclosed_emphasis;
template <>
inline constexpr errc error_code_v<grammar::emphasis_close<'*'>> = errc::unclosed_emphasis;
template <> inline constexpr errc error_code_v<grammar::id_close> = errc::unclosed_resource;
template <> inline constexpr errc error_code_v<grammar::target_close> = errc::unclosed_resource;
template <> inline constexpr errc error_code_v<grammar::code_close> = errc::unclosed_code_block;
template <> inline constexpr errc error_code_v<grammar::inline_math_close> = errc::unclosed_math;
template <> inline constexpr errc error_code_v<grammar::math_close> = errc::unclosed_math_block;
template <> inline constexpr errc error_code_v<grammar::aligned_close> = errc::unclosed_math_block;
template <> inline constexpr errc error_code_v<grammar::empty_math> = errc::empty_math;

template <typename Rule> struct control : pegtl::normal<Rule> {
	template <typename ParseInput, typename... States>
	[[noreturn]] static auto raise(ParseInput const& input, States&&... /*unused*/) -> void
	{
		throw syntax_failure(error_code_v<Rule>, input.current_position());
	}
};

using pegtl_input = pegtl::text_view_input<pegtl::ascii::scan::lf_crlf>;
using pegtl_node = pegtl::parse_tree::node_t<pegtl_input>;

auto parse_pegtl_tree(std::string_view source) -> result<std::unique_ptr<pegtl_node>>
{
	pegtl_input input{source};
	try {
		if (auto tree =
		        pegtl::parse_tree::parse<grammar::doc, selector, pegtl::nothing, control>(input);
		    tree) {
			return tree;
		}
		return std::unexpected(error{.code = errc::syntax, .line = 0, .column = 0});
	} catch (syntax_failure const& failure) {
		return std::unexpected(error{
			.code = failure.code,
			.line = failure.line,
			.column = failure.column,
		});
	} catch (pegtl::parse_error_base const&) {
		return std::unexpected(error{.code = errc::syntax, .line = 0, .column = 0});
	}
}

auto find_reference(ast::inlines const& nodes) -> std::optional<std::string_view>
{
	using found_key = std::optional<std::string_view>;
	auto const visitor = detail::overloaded{
		[](ast::reference const& item) -> found_key { return item.key; },
		[](ast::emphasis const& item) -> found_key { return find_reference(item.children); },
		[](ast::strong const& item) -> found_key { return find_reference(item.children); },
		[](auto const& /*leaf*/) -> found_key { return std::nullopt; },
	};
	for (auto const& node : nodes) {
		if (auto const found = std::visit(visitor, node)) {
			return found;
		}
	}
	return std::nullopt;
}

constexpr std::string_view blanks = " \t";

[[nodiscard]] auto is_blank(std::string_view text) noexcept -> bool
{
	return text.find_first_not_of(blanks) == std::string_view::npos;
}

[[nodiscard]] auto end_of(std::string_view text) noexcept -> std::string_view
{
	return std::string_view{text.end(), text.end()};
}

struct lowering_context {
	explicit lowering_context(std::string_view src)
		: source{src}
	{
	}

	std::string_view source;
	ast::resources defs;

	enum class ref_kind : std::uint8_t { link, image, footnote };
	struct pending_ref {
		ref_kind kind;
		std::string_view key;
	};
	std::vector<pending_ref> refs;

	auto add_link(std::string_view key, std::string_view target) -> result<void>
	{
		TRY(check_key(key));
		if (defs.links.contains(key)) {
			return std::unexpected(detail::make_error(errc::duplicate_definition, source, key));
		}
		defs.links.insert_or_assign(key, target);
		return {};
	}

	[[nodiscard]] auto split_width(std::string_view text) const -> result<ast::image_def>
	{
		constexpr std::size_t max_width = 100;
		constexpr std::size_t decimal = 10;
		ast::image_def const plain{.target = text, .width = std::nullopt};
		auto const end = text.find_last_not_of(blanks);
		auto const gap = end == std::string_view::npos ? end : text.find_last_of(blanks, end);
		auto const rest = gap == std::string_view::npos ? gap : text.find_last_not_of(blanks, gap);
		if (rest == std::string_view::npos) {
			return plain;
		}
		auto const word = text.substr(gap + 1, end - gap);
		auto const digits = word.substr(0, word.size() - 1);
		if (!word.ends_with('%') || digits.empty() ||
		    digits.find_first_not_of("0123456789") != std::string_view::npos) {
			return plain;
		}
		std::size_t value = 0;
		for (char const digit : digits) {
			value =
				std::min((value * decimal) + static_cast<std::size_t>(digit - '0'), max_width + 1);
		}
		if (value == 0 || value > max_width) {
			return std::unexpected(detail::make_error(errc::invalid_width, source, word));
		}
		return ast::image_def{.target = text.substr(0, rest + 1), .width = word};
	}

	auto add_image(std::string_view key, pegtl_node const& target) -> result<ast::image_def>
	{
		TRY(check_key(key));
		if (defs.images.contains(key)) {
			return std::unexpected(detail::make_error(errc::duplicate_definition, source, key));
		}
		auto const def = TRY(split_width(target.data));
		defs.images.insert_or_assign(key, def);
		return def;
	}

	auto add_footnote(std::string_view key, ast::inlines content) -> result<void>
	{
		if (defs.footnotes.contains(key)) {
			return std::unexpected(detail::make_error(errc::duplicate_definition, source, key));
		}
		defs.footnotes.insert_or_assign(key, std::move(content));
		return {};
	}

	[[nodiscard]] auto check_not_nested(ast::inlines const& content) const -> result<void>
	{
		if (auto const key = find_reference(content); key) {
			return std::unexpected(detail::make_error(errc::nested_footnote, source, *key));
		}
		return {};
	}

	[[nodiscard]] auto check_key(std::string_view key) const -> result<void>
	{
		if (is_blank(key)) {
			return std::unexpected(detail::make_error(errc::empty_key, source, end_of(key)));
		}
		return {};
	}

	auto add_ref(ref_kind kind, std::string_view key) -> result<void>
	{
		TRY(check_key(key));
		refs.push_back(pending_ref{.kind = kind, .key = key});
		return {};
	}

	[[nodiscard]] auto validate_refs() const -> result<void>
	{
		for (auto const& ref_item : refs) {
			bool found = false;
			switch (ref_item.kind) {
			case ref_kind::link:
				found = defs.links.contains(ref_item.key);
				break;
			case ref_kind::image:
				found = defs.images.contains(ref_item.key);
				break;
			case ref_kind::footnote:
				found = defs.footnotes.contains(ref_item.key);
				break;
			}
			if (!found) {
				return std::unexpected(
					detail::make_error(errc::undefined_reference, source, ref_item.key));
			}
		}
		return {};
	}
};

[[noreturn]] auto unhandled_node(pegtl_node const& node) -> void
{
	std::println(stderr, "jimd: internal error: no lowering for {}", node.type);
	std::abort();
}

auto lower_link(pegtl_node const& node, lowering_context& ctx) -> result<ast::link>
{
	auto const key = node.children.front()->data;
	std::optional<std::string_view> target;
	if (node.children.size() > 1 && node.children.at(1)->is_type<grammar::resource_target>()) {
		if (is_blank(key)) {
			return std::unexpected(detail::make_error(errc::empty_link, ctx.source, end_of(key)));
		}
		target = node.children.at(1)->data;
	} else {
		TRY(ctx.add_ref(lowering_context::ref_kind::link, key));
	}
	return ast::link{.key = key, .text = key, .target = target};
}

auto lower_image(pegtl_node const& node, lowering_context& ctx) -> result<ast::image>
{
	auto const key = node.children.front()->data;
	std::optional<std::string_view> target;
	std::optional<std::string_view> width;
	if (node.children.size() > 1 && node.children.at(1)->is_type<grammar::resource_target>()) {
		auto const def = TRY(ctx.split_width(node.children.at(1)->data));
		target = def.target;
		width = def.width;
	} else {
		TRY(ctx.add_ref(lowering_context::ref_kind::image, key));
	}
	return ast::image{.key = key, .alt = key, .target = target, .width = width};
}

auto lower_inlines(pegtl_node const& node, lowering_context& ctx) -> result<ast::inlines>;

auto lower_reference(pegtl_node const& node, lowering_context& ctx) -> result<ast::reference>
{
	auto const key = node.children.front()->data;
	std::optional<ast::inlines> content;
	if (node.children.size() > 1 && node.children.at(1)->is_type<grammar::footnote_target>()) {
		content = TRY(lower_inlines(*node.children.at(1), ctx));
		TRY(ctx.check_not_nested(*content));
	} else {
		TRY(ctx.add_ref(lowering_context::ref_kind::footnote, key));
	}
	return ast::reference{.key = key, .content = std::move(content)};
}

auto lower_inline(pegtl_node const& node, lowering_context& ctx) -> result<ast::inline_node>;

auto lower_inlines(pegtl_node const& node, lowering_context& ctx) -> result<ast::inlines>
{
	ast::inlines out;
	for (auto const& child : node.children) {
		out.push_back(TRY(lower_inline(*child, ctx)));
	}
	return out;
}

auto lower_inline(pegtl_node const& node, lowering_context& ctx) -> result<ast::inline_node>
{
	if (node.is_type<grammar::text>() || node.is_type<grammar::footnote_text>() ||
	    node.is_type<grammar::escape_char>()) {
		return ast::text{.value = node.data};
	}
	if (node.is_type<grammar::intra_newline>()) {
		return ast::soft_break{};
	}
	if (node.is_type<grammar::italic>()) {
		return ast::emphasis{.children = TRY(lower_inlines(node, ctx))};
	}
	if (node.is_type<grammar::bold>()) {
		return ast::strong{.children = TRY(lower_inlines(node, ctx))};
	}
	if (node.is_type<grammar::inline_code>()) {
		return ast::code_span{.value = node.data};
	}
	if (node.is_type<grammar::inline_math>()) {
		return ast::math{.tex = node.data};
	}
	if (node.is_type<grammar::link>()) {
		return TRY(lower_link(node, ctx));
	}
	if (node.is_type<grammar::image>()) {
		return TRY(lower_image(node, ctx));
	}
	if (node.is_type<grammar::reference>()) {
		return TRY(lower_reference(node, ctx));
	}
	unhandled_node(node);
}

auto lower_footnote_def(pegtl_node const& node, lowering_context& ctx) -> result<void>
{
	auto const key = node.children.front()->data;
	TRY(ctx.check_key(key));
	ast::inlines content;
	for (auto const& child : node.children | std::views::drop(1)) {
		content.push_back(TRY(lower_inline(*child, ctx)));
	}
	TRY(ctx.check_not_nested(content));
	return ctx.add_footnote(key, std::move(content));
}

auto lower_def(pegtl_node const& node, lowering_context& ctx) -> result<bool>
{
	if (node.is_type<grammar::link_def>()) {
		TRY(ctx.add_link(node.children.at(0)->data, node.children.at(1)->data));
		return true;
	}
	if (node.is_type<grammar::image_def>()) {
		TRY(ctx.add_image(node.children.at(0)->data, *node.children.at(1)));
		return true;
	}
	if (node.is_type<grammar::reference_def>()) {
		TRY(lower_footnote_def(node, ctx));
		return true;
	}
	return false;
}

auto lower_heading(pegtl_node const& node, lowering_context& ctx) -> result<ast::heading>
{
	ast::heading heading{.level = node.children.front()->data.size(), .children = {}};
	for (auto const& child : node.children | std::views::drop(1)) {
		heading.children.push_back(TRY(lower_inline(*child, ctx)));
	}
	return heading;
}

auto lower_code_block(pegtl_node const& node) -> ast::code_block
{
	auto const& body = *node.children.back();
	return ast::code_block{
		.info = node.children.size() > 1 ? node.children.front()->data : std::string_view{},
		.body = body.data,
	};
}

auto lower_list(pegtl_node const& node, lowering_context& ctx) -> result<ast::list>
{
	ast::list list_node;
	for (auto const& item : node.children) {
		list_node.items.push_back(TRY(lower_inlines(*item, ctx)));
	}
	return list_node;
}

auto split_rows(std::string_view body) -> std::vector<std::string_view>
{
	std::vector<std::string_view> lines;
	for (auto end = body.find('\n'); end != std::string_view::npos; end = body.find('\n')) {
		auto line = body.substr(0, end);
		if (line.ends_with('\r')) {
			line.remove_suffix(1);
		}
		lines.push_back(line);
		body.remove_prefix(end + 1);
	}
	auto const is_row = [](std::string_view line) -> bool { return !is_blank(line); };
	auto const first = std::ranges::find_if(lines, is_row);
	auto const last = std::ranges::find_if(lines | std::views::reverse, is_row);
	return {first, last.base()};
}

auto tex_code(std::string_view row) -> std::string_view
{
	auto pos = row.find_first_of("\\%");
	while (pos != std::string_view::npos && !row.substr(pos).starts_with('%')) {
		pos = row.find_first_of("\\%", pos + 2);
	}
	return row.substr(0, pos);
}

auto ends_in_row_break(std::string_view code) -> bool
{
	if (auto const last = code.find_last_not_of(blanks); last != std::string_view::npos) {
		code = code.substr(0, last + 1);
	}
	auto const other = code.find_last_not_of('\\');
	auto const run = other == std::string_view::npos ? code.size() : code.size() - 1 - other;
	return run >= 2;
}

auto environment_balance(std::string_view code) -> int
{
	constexpr std::string_view letters = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
	int balance = 0;
	for (auto pos = code.find('\\'); pos != std::string_view::npos; pos = code.find('\\', pos)) {
		++pos;
		auto const word = code.substr(pos, code.find_first_not_of(letters, pos) - pos);
		pos += std::max<std::size_t>(word.size(), 1);
		auto const brace = code.find_first_not_of(blanks, pos);
		if (brace == std::string_view::npos || !code.substr(brace).starts_with('{')) {
			continue;
		}
		if (word == "begin") {
			++balance;
		} else if (word == "end") {
			--balance;
		}
	}
	return balance;
}

auto check_row(std::string_view row, std::string_view source) -> result<void>
{
	auto const code = tex_code(row);
	if (ends_in_row_break(code)) {
		return std::unexpected(detail::make_error(errc::math_row_break, source, row));
	}
	if (environment_balance(code) != 0) {
		return std::unexpected(detail::make_error(errc::math_environment_spans_rows, source, row));
	}
	return {};
}

auto lower_aligned(pegtl_node const& node, lowering_context const& ctx) -> result<ast::aligned_math>
{
	ast::aligned_math aligned{.rows = split_rows(node.children.front()->data)};
	for (auto const row : aligned.rows) {
		TRY(check_row(row, ctx.source));
	}
	return aligned;
}

auto lower_block(pegtl_node const& node, lowering_context& ctx)
	-> result<std::optional<ast::block>>;

auto lower_quote(pegtl_node const& node, lowering_context& ctx) -> result<ast::quote>
{
	ast::quote quote_node;
	for (auto const& child : node.children) {
		if (auto block_item = TRY(lower_block(*child, ctx)); block_item) {
			quote_node.children.push_back(std::move(*block_item));
		}
	}
	return quote_node;
}

auto lower_fenced(pegtl_node const& node, lowering_context const& ctx) -> result<ast::block>
{
	if (node.is_type<grammar::block_code>()) {
		return lower_code_block(node);
	}
	if (node.is_type<grammar::block_math>()) {
		return ast::display_math{.tex = node.children.front()->data};
	}
	if (node.is_type<grammar::block_aligned>()) {
		return TRY(lower_aligned(node, ctx));
	}
	unhandled_node(node);
}

auto lower_block(pegtl_node const& node, lowering_context& ctx) -> result<std::optional<ast::block>>
{
	if (TRY(lower_def(node, ctx))) {
		return std::nullopt;
	}
	if (node.is_type<grammar::paragraph>()) {
		ast::paragraph para;
		for (auto const& child : node.children) {
			para.children.push_back(TRY(lower_inline(*child, ctx)));
		}
		return para;
	}
	if (node.is_type<grammar::header>()) {
		return TRY(lower_heading(node, ctx));
	}
	if (node.is_type<grammar::quote>()) {
		return TRY(lower_quote(node, ctx));
	}
	if (node.is_type<grammar::bullet_list>()) {
		return TRY(lower_list(node, ctx));
	}
	return TRY(lower_fenced(node, ctx));
}

auto add_metadata(ast::document& doc, pegtl_node const& node) -> result<void>
{
	auto const key = node.children.at(0)->data;
	if (doc.metadata.contains(key)) {
		return std::unexpected(detail::make_error(errc::duplicate_metadata, doc.source, key));
	}
	doc.metadata.insert_or_assign(key, node.children.at(1)->data);
	return {};
}

auto lower_document(pegtl_node const& root, std::string_view source) -> result<ast::document>
{
	ast::document doc;
	doc.source = source;
	lowering_context ctx{source};

	for (auto const& child : root.children.front()->children) {
		if (child->is_type<grammar::metadata_def>()) {
			TRY(add_metadata(doc, *child));
			continue;
		}
		if (auto block_item = TRY(lower_block(*child, ctx)); block_item) {
			doc.blocks.push_back(std::move(*block_item));
		}
	}
	TRY(ctx.validate_refs());

	doc.defs = std::move(ctx.defs);
	return doc;
}
}

auto parse(std::string_view source) -> result<ast::document>
{
	auto const tree = TRY(parse_pegtl_tree(source));
	return lower_document(*tree, source);
}
}
