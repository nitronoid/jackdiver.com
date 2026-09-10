#pragma once

namespace jimd::detail
{
/**
 * struct overloaded - Combines callables into one overload set, to give to std::visit().
 *
 * Name the node types that need work and let a ``[](auto const&)`` catch-all take the rest.
 * The trade-off is that a catch-all also takes a node type added later. A container node, one
 * with children, would then fall into the default silently instead of failing to compile, so
 * the tests have to catch it.
 */
template <typename... Fns> struct overloaded : Fns... {
	using Fns::operator()...;
};
}
