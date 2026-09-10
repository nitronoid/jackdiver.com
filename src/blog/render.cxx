#include <blog/render.hxx>

#include <expected>
#include <filesystem>
#include <flat_map>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

#include <blog/error.hxx>
#include <blog/image.hxx>
#include <highlight/hljs.hxx>
#include <jimd/ast.hxx>
#include <jimd/html.hxx>
#include <jimd/position.hxx>
#include <jimd/result.hxx>
#include <math/temml.hxx>

namespace blog
{
namespace
{
auto engine_mode(jimd::html::math_mode style) -> math::mode
{
	switch (style) {
	case jimd::html::math_mode::inline_math:
		return math::mode::inline_math;
	case jimd::html::math_mode::display:
		return math::mode::display;
	}
	std::unreachable();
}

auto invalid_math(jimd::ast::document const& doc, jimd::html::math_request const& request,
                  math::error const& failure) -> error
{
	auto const where = failure.offset ? request.locate(*failure.offset) : request.origin;
	return error{errc::invalid_math, jimd::position_of(doc.source, where), failure.message};
}

auto invalid_code(jimd::ast::document const& doc, jimd::html::code_request const& request,
                  highlight::error const& failure) -> error
{
	return error{errc::invalid_code, jimd::position_of(doc.source, request.language),
	             failure.message};
}

auto figure_error(jimd::ast::document const& doc, jimd::html::svg_figure const& figure,
                  std::variant<jimd::errc, errc> code, std::string_view reason) -> error
{
	auto detail = std::string{figure.target};
	if (!reason.empty()) {
		detail += ": ";
		detail += reason;
	}
	return error{code, jimd::position_of(doc.source, figure.key), std::move(detail)};
}

auto figure_ratios(jimd::ast::document const& doc, render_context const& context)
	-> std::expected<std::flat_map<std::string, std::string, std::less<>>, error>
{
	std::flat_map<std::string, std::string, std::less<>> ratios;
	for (auto const& figure : jimd::html::svg_figures(doc)) {
		if (ratios.contains(figure.target)) {
			continue;
		}
		if (auto const problem = local_path_problem(figure.target)) {
			return std::unexpected(figure_error(doc, figure, errc::image_not_local, *problem));
		}

		auto ratio =
			svg_ratio(context.static_root / std::filesystem::path{figure.target.substr(1)});
		if (!ratio) {
			return std::unexpected(
				figure_error(doc, figure, ratio.error().code, ratio.error().detail));
		}
		ratios.emplace(std::string{figure.target}, *std::move(ratio));
	}
	return ratios;
}
}

auto render_fragment(jimd::ast::document const& doc, render_context const& context)
	-> std::expected<std::string, error>
{
	jimd::html::math_table table;
	for (auto const& request : jimd::html::math_requests(doc)) {
		if (table.contains(
				std::pair<jimd::html::math_mode, std::string_view>{request.mode, request.tex})) {
			continue;
		}
		auto mathml = context.engine.render(request.tex, engine_mode(request.mode));
		if (!mathml) {
			return std::unexpected(invalid_math(doc, request, mathml.error()));
		}
		table.emplace(std::pair{request.mode, request.tex}, *std::move(mathml));
	}

	jimd::html::code_table code;
	for (auto const& request : jimd::html::code_requests(doc)) {
		if (code.contains(
				std::pair<std::string_view, std::string_view>{request.language, request.body})) {
			continue;
		}
		auto highlighted = context.highlighter.render(request.language, request.body);
		if (!highlighted) {
			return std::unexpected(invalid_code(doc, request, highlighted.error()));
		}
		code.emplace(std::pair{std::string{request.language}, request.body},
		             *std::move(highlighted));
	}

	auto const ratios = figure_ratios(doc, context);
	if (!ratios) {
		return std::unexpected(ratios.error());
	}

	auto fragment = jimd::html::render(
		doc,
		jimd::html::render_options{.math = &table, .code = &code, .image_ratios = &ratios.value()});
	if (!fragment) {
		return std::unexpected(fragment.error());
	}
	return *std::move(fragment);
}
}
