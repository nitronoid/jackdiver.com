#include <cstdint>
#include <string>
#include <gtest/gtest.h>

#include <quickjs.h>

namespace
{
constexpr int answer = 42;

class engine {
public:
	engine()
		: rt_{JS_NewRuntime()}
		, ctx_{JS_NewContext(rt_)}
	{
	}
	engine(engine const&) = delete;
	auto operator=(engine const&) -> engine& = delete;
	~engine()
	{
		JS_FreeContext(ctx_);
		JS_FreeRuntime(rt_);
	}

	[[nodiscard]] auto context() const -> JSContext*
	{
		return ctx_;
	}

	[[nodiscard]] auto eval(std::string const& source) const -> JSValue
	{
		return JS_Eval(ctx_, source.data(), source.size(), "<test>", 0);
	}

private:
	JSRuntime* rt_;
	JSContext* ctx_;
};
}

TEST(quickjs, evaluates_arithmetic)
{
	engine const qjs;
	JSValue const value = qjs.eval("6 * 7");
	ASSERT_FALSE(JS_IsException(value));

	std::int32_t number = 0;
	ASSERT_EQ(JS_ToInt32(qjs.context(), &number, value), 0);
	EXPECT_EQ(number, answer);
	JS_FreeValue(qjs.context(), value);
}

TEST(quickjs, reports_a_syntax_error_as_an_exception)
{
	engine const qjs;
	JSValue const value = qjs.eval("6 *");
	EXPECT_TRUE(JS_IsException(value));

	JSValue const exception = JS_GetException(qjs.context());
	JSValue const name = JS_GetPropertyStr(qjs.context(), exception, "name");
	char const* const text = JS_ToCString(qjs.context(), name);
	ASSERT_NE(text, nullptr);
	EXPECT_STREQ(text, "SyntaxError");
	JS_FreeCString(qjs.context(), text);
	JS_FreeValue(qjs.context(), name);
	JS_FreeValue(qjs.context(), exception);
}
