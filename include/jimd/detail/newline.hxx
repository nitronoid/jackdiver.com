#pragma once

#include <string>
#include <string_view>

namespace jimd::detail
{
/**
 * crlf_to_lf() - Turn each CR LF pair into a LF.
 * @text: The text to copy.
 *
 * A lone CR or LF is kept as it is.
 *
 * Return: The text with LF line ends.
 */
[[nodiscard]] inline auto crlf_to_lf(std::string_view text) -> std::string
{
	std::string out;
	for (auto pos = text.find("\r\n"); pos != std::string_view::npos; pos = text.find("\r\n")) {
		out += text.substr(0, pos);
		out += '\n';
		text.remove_prefix(pos + 2);
	}
	out += text;
	return out;
}
}
