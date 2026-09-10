#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
#include <string_view>
#include <gtest/gtest.h>

#include <blog/error.hxx>
#include <blog/file.hxx>
#include <support/result_printers.hxx>

using namespace std::literals;

namespace
{
auto testdata(std::string_view name) -> std::filesystem::path
{
	// NOLINTBEGIN(concurrency-mt-unsafe)
	char const* const srcdir = std::getenv("TEST_SRCDIR");
	char const* const workspace = std::getenv("TEST_WORKSPACE");
	// NOLINTEND(concurrency-mt-unsafe)
	if (srcdir == nullptr || workspace == nullptr) {
		return {};
	}
	return std::filesystem::path{srcdir} / workspace / "tests" / "blog" / "testdata" / name;
}
}

TEST(read_file, reads_the_bytes_as_they_are)
{
	auto const content = blog::read_file(testdata("bad_view_box.svg"));
	ASSERT_TRUE(content.has_value());
	EXPECT_EQ(content.value(),
	          "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 600\">\n</svg>\n");
}

TEST(read_file, files_around_the_size_of_the_buffer)
{
	auto const path = std::filesystem::path{::testing::TempDir()} / "read_file_sizes.txt";
	constexpr std::size_t buffer = BUFSIZ;
	for (auto const size : {buffer - 1, buffer, buffer + 1, 2 * buffer, 100000 * std::size_t{1}}) {
		std::string const data(size, 'x');
		{
			std::ofstream file{path, std::ios::binary};
			file.write(data.data(), static_cast<std::streamsize>(data.size()));
			ASSERT_TRUE(file.good());
		}
		auto const content = blog::read_file(path);
		ASSERT_TRUE(content.has_value()) << size;
		EXPECT_EQ(content.value(), data) << size;
	}
	std::filesystem::remove(path);
}

TEST(read_file, an_empty_file)
{
	auto const path = std::filesystem::path{::testing::TempDir()} / "read_file_empty.txt";
	{
		std::ofstream file{path, std::ios::binary};
		ASSERT_TRUE(file.good());
	}
	auto const content = blog::read_file(path);
	ASSERT_TRUE(content.has_value());
	EXPECT_TRUE(content.value().empty());
	std::filesystem::remove(path);
}

TEST(read_file, a_missing_file_has_no_detail)
{
	auto const content = blog::read_file(testdata("nothing_here.svg"));
	ASSERT_FALSE(content.has_value());
	EXPECT_EQ(content.error(), blog::error{blog::errc::file_not_found});
}

TEST(read_file, a_directory_cannot_be_read)
{
	auto const content = blog::read_file(testdata(""));
	ASSERT_FALSE(content.has_value());
	EXPECT_EQ(content.error(), blog::error{blog::errc::read_failed});
}
