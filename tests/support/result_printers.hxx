#pragma once

#include <expected>
#include <ostream>
#include <type_traits>

#include <gtest/gtest.h>

#include <blog/error.hxx>
#include <jimd/position.hxx>
#include <jimd/result.hxx>

namespace jimd
{
/**
 * PrintTo() - Print a position as ``line:column``.
 * @pos: The position to print.
 * @stream: The stream to print to.
 */
auto PrintTo(position const& pos, std::ostream* stream) -> void;
/**
 * PrintTo() - Print a jimd error as its description and position.
 * @err: The error to print.
 * @stream: The stream to print to.
 */
auto PrintTo(error const& err, std::ostream* stream) -> void;
/**
 * PrintTo() - Print the error held by @err.
 * @err: The wrapped error to print.
 * @stream: The stream to print to.
 */
auto PrintTo(std::unexpected<error> const& err, std::ostream* stream) -> void;

/**
 * PrintTo() - Print a result as its error, ``success`` or its value.
 * @res: The result to print.
 * @stream: The stream to print to.
 */
template <typename T> auto PrintTo(result<T> const& res, std::ostream* stream) -> void
{
	if (!res) {
		PrintTo(res.error(), stream);
	} else if constexpr (std::is_void_v<T>) {
		*stream << "success";
	} else {
		*stream << ::testing::PrintToString(*res);
	}
}
}

namespace blog
{
/**
 * PrintTo() - Print a blog error as format_error() does for path ``<input>``.
 * @err: The error to print.
 * @stream: The stream to print to.
 */
auto PrintTo(error const& err, std::ostream* stream) -> void;
}
