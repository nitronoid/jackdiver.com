#include <string_view>
#include <utility>

#include <jimd/result.hxx>

namespace jimd
{
auto describe(errc code) noexcept -> std::string_view
{
	switch (code) {
	case errc::syntax:
		return "syntax error";
	case errc::unclosed_quote:
		return "unclosed quote";
	case errc::unclosed_code_block:
		return "unclosed code block";
	case errc::unclosed_emphasis:
		return "unclosed emphasis";
	case errc::empty_emphasis:
		return "empty emphasis";
	case errc::unclosed_resource:
		return "unclosed resource";
	case errc::invalid_escape:
		return "invalid escape";
	case errc::duplicate_definition:
		return "duplicate definition";
	case errc::undefined_reference:
		return "undefined reference";
	case errc::empty_key:
		return "empty key";
	case errc::empty_link:
		return "empty link";
	case errc::nested_footnote:
		return "nested footnote";
	case errc::duplicate_metadata:
		return "duplicate metadata";
	case errc::unclosed_math:
		return "unclosed maths";
	case errc::unclosed_math_block:
		return "unclosed maths block";
	case errc::empty_math:
		return "empty maths";
	case errc::math_row_break:
		return "maths row ends in a row break";
	case errc::math_environment_spans_rows:
		return "maths environment spans rows";
	case errc::invalid_width:
		return "invalid image width";
	case errc::invalid_target:
		return "invalid figure target";
	case errc::unrendered_math:
		return "unrendered maths";
	case errc::unknown_image_size:
		return "unknown image size";
	}
	std::unreachable();
}
}
