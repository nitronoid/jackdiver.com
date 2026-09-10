#include <support/dump.hxx>

#include <cstddef>
#include <flat_map>
#include <format>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

#include <jimd/ast.hxx>
#include <jimd/detail/overloaded.hxx>

namespace support
{
namespace
{
class printer {
public:
	explicit printer(std::size_t depth = 0) noexcept
		: m_depth(depth)
	{
	}

	auto print_document(jimd::ast::document const& doc) -> void
	{
		print_line("document");
		++m_depth;
		if (!doc.metadata.empty()) {
			print_metadata(doc.metadata);
		}
		if (!doc.defs.links.empty() || !doc.defs.images.empty() || !doc.defs.footnotes.empty()) {
			print_resources(doc.defs);
		}
		for (auto const& block : doc.blocks) {
			print_block(block);
		}
		--m_depth;
	}

	auto print_metadata(std::flat_map<std::string_view, std::string_view> const& metadata) -> void
	{
		print_line("metadata");
		++m_depth;
		for (auto const& [key, value] : metadata) {
			print_line(std::format("{:?}: {:?}", key, value));
		}
		--m_depth;
	}

	auto print_resources(jimd::ast::resources const& defs) -> void
	{
		print_line("defs");
		++m_depth;
		for (auto const& [key, target] : defs.links) {
			print_line(std::format("link {:?}: {:?}", key, target));
		}
		for (auto const& [key, def] : defs.images) {
			print_line(std::format("image {:?}: {:?}{}", key, def.target, width_suffix(def.width)));
		}
		for (auto const& [key, content] : defs.footnotes) {
			print_line(std::format("footnote {:?}:", key));
			print_children(content);
		}
		--m_depth;
	}

	auto print_block(jimd::ast::block const& block) -> void
	{
		namespace ast = jimd::ast;
		auto const visitor = jimd::detail::overloaded{
			[this](ast::paragraph const& node) -> void {
				print_line("paragraph");
				print_children(node.children);
			},
			[this](ast::heading const& node) -> void {
				print_line(std::format("heading level={}", node.level));
				print_children(node.children);
			},
			[this](ast::quote const& node) -> void {
				print_line("quote");
				++m_depth;
				for (auto const& child : node.children) {
					print_block(child);
				}
				--m_depth;
			},
			[this](ast::code_block const& node) -> void {
				if (node.info.empty()) {
					print_line(std::format("code_block {:?}", node.body));
				} else {
					print_line(std::format("code_block info={:?} {:?}", node.info, node.body));
				}
			},
			[this](ast::list const& node) -> void {
				print_line("list");
				++m_depth;
				for (auto const& item : node.items) {
					print_line("item");
					print_children(item);
				}
				--m_depth;
			},
			[this](ast::display_math const& node) -> void {
				print_line(std::format("display_math {:?}", node.tex));
			},
			[this](ast::aligned_math const& node) -> void {
				print_line("aligned_math");
				++m_depth;
				for (auto const row : node.rows) {
					print_line(std::format("row {:?}", row));
				}
				--m_depth;
			},
		};
		std::visit(visitor, block);
	}

	auto print_inline(jimd::ast::inline_node const& node) -> void
	{
		namespace ast = jimd::ast;
		auto const visitor = jimd::detail::overloaded{
			[this](ast::text const& item) -> void {
				print_line(std::format("text {:?}", item.value));
			},
			[this](ast::soft_break const& /*item*/) -> void { print_line("soft_break"); },
			[this](ast::code_span const& item) -> void {
				print_line(std::format("code_span {:?}", item.value));
			},
			[this](ast::emphasis const& item) -> void {
				print_line("emphasis");
				print_children(item.children);
			},
			[this](ast::strong const& item) -> void {
				print_line("strong");
				print_children(item.children);
			},
			[this](ast::link const& item) -> void {
				if (item.target.has_value()) {
					print_line(std::format("link key={:?} text={:?} target={:?}", item.key,
					                       item.text, *item.target));
				} else {
					print_line(std::format("link key={:?} text={:?}", item.key, item.text));
				}
			},
			[this](ast::image const& item) -> void {
				if (item.target.has_value()) {
					print_line(std::format("image key={:?} alt={:?} target={:?}{}", item.key,
					                       item.alt, *item.target, width_suffix(item.width)));
				} else {
					print_line(std::format("image key={:?} alt={:?}", item.key, item.alt));
				}
			},
			[this](ast::reference const& item) -> void {
				print_line(std::format("reference key={:?}", item.key));
				if (item.content.has_value()) {
					print_children(*item.content);
				}
			},
			[this](ast::math const& item) -> void {
				print_line(std::format("math {:?}", item.tex));
			},
		};
		std::visit(visitor, node);
	}

	[[nodiscard]] auto str() const& -> std::string const&
	{
		return m_buffer;
	}

	[[nodiscard]] auto str() && noexcept -> std::string
	{
		return std::move(m_buffer);
	}

private:
	[[nodiscard]] static auto width_suffix(std::optional<std::string_view> width) -> std::string
	{
		return width ? std::format(" width={:?}", *width) : std::string{};
	}

	[[nodiscard]] auto indent() const -> std::string
	{
		std::string result(m_depth * 2, ' ');
		return result;
	}

	auto print_line(std::string_view text) -> void
	{
		std::format_to(std::back_inserter(m_buffer), "{}{}\n", indent(), text);
	}

	auto print_children(jimd::ast::inlines const& children) -> void
	{
		++m_depth;
		for (auto const& child : children) {
			print_inline(child);
		}
		--m_depth;
	}

	std::size_t m_depth{0};
	std::string m_buffer;
};
}

auto dump(jimd::ast::document const& doc) -> std::string
{
	printer out{};
	out.print_document(doc);
	return std::move(out).str();
}

auto dump(jimd::ast::block const& block, std::size_t depth) -> std::string
{
	printer out{depth};
	out.print_block(block);
	return std::move(out).str();
}

auto dump(jimd::ast::inline_node const& node, std::size_t depth) -> std::string
{
	printer out{depth};
	out.print_inline(node);
	return std::move(out).str();
}

auto dump(jimd::ast::resources const& defs, std::size_t depth) -> std::string
{
	printer out{depth};
	out.print_resources(defs);
	return std::move(out).str();
}
}
