#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <gtest/gtest.h>

#include <fixture.h>

namespace
{
// BC_VERSION in deps/thirdparty/quickjs/quickjs-amalgam.c, the first byte of any bytecode. It
// changes with the vendored QuickJS, which is when this test should fail.
constexpr std::uint8_t bytecode_version = 28;

auto contains(std::span<std::uint8_t const> bytes, std::string_view text) -> bool
{
	auto const same = [](std::uint8_t byte, char letter) -> bool {
		return byte == static_cast<std::uint8_t>(letter);
	};
	return !std::ranges::search(bytes, text, same).empty();
}
}

TEST(js_bytecode, has_one_script_for_each_source_in_order)
{
	auto const scripts = fixture();
	ASSERT_EQ(scripts.size(), std::size_t{2});
	EXPECT_EQ(scripts.at(0).data(), static_cast<std::uint8_t const*>(fixture_first));
	EXPECT_EQ(scripts.at(0).size(), fixture_first_size);
	EXPECT_EQ(scripts.at(1).data(), static_cast<std::uint8_t const*>(fixture_second));
	EXPECT_EQ(scripts.at(1).size(), fixture_second_size);
}

TEST(js_bytecode, starts_with_the_bytecode_version)
{
	for (auto const script : fixture()) {
		ASSERT_FALSE(script.empty());
		EXPECT_EQ(script.front(), bytecode_version);
	}
}

TEST(js_bytecode, keeps_the_debug_information)
{
	auto const scripts = fixture();
	EXPECT_TRUE(contains(scripts.at(0), "testdata/first.js"));
	EXPECT_TRUE(contains(scripts.at(1), "testdata/second.js"));
}
