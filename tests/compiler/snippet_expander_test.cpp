#include <catch2/catch_test_macros.hpp>

#include <string>
#include <string_view>

#include "whitehole/compiler/snippet_expander.hpp"

using namespace whitehole;
using namespace whitehole::compiler;

namespace {

SnippetBindings testBindings() {
    SnippetBindings bindings;
    bindings.time = "T";
    bindings.input = [](std::string_view port, std::string_view coordinate) -> Result<std::string> {
        if (port == "bad") {
            return Error{ErrorCode::NotFound, "kein Port bad"};
        }
        return "in_" + std::string{port} + "(" + std::string{coordinate} + ")";
    };
    bindings.parameter = [](std::string_view key) -> Result<std::string> {
        return "P." + std::string{key};
    };
    return bindings;
}

std::string expand(std::string_view body) {
    return expandSnippet(body, testBindings()).value();
}

}  // namespace

TEST_CASE("expandSnippet: Text ohne Platzhalter bleibt unverändert") {
    const std::string text = "let a = vec3<f32>(1.0, 2.0, 3.0);\nreturn vec4<f32>(a, 1.0);";
    CHECK(expand(text) == text);
    CHECK(expand("") == "");
}

TEST_CASE("expandSnippet: @time und @param") {
    CHECK(expand("x = @time * @param(speed);") == "x = T * P.speed;");
    CHECK(expand("@param( spaced )") == "P.spaced");
}

TEST_CASE("expandSnippet: @in mit einfacher und verschachtelter Koordinate") {
    CHECK(expand("@in(a, p)") == "in_a(p)");
    CHECK(expand("@in(a, p + vec3<f32>(1.0, 0.0, 0.0))") == "in_a(p + vec3<f32>(1.0, 0.0, 0.0))");
    CHECK(expand("@in( src ,  vec3<f32>(r.x, r.y, p.z) )") == "in_src(vec3<f32>(r.x, r.y, p.z))");
}

TEST_CASE("expandSnippet: Platzhalter dürfen in der Koordinate stehen") {
    CHECK(expand("@in(a, p + vec3<f32>(@param(shift), 0.0, @time))") ==
          "in_a(p + vec3<f32>(P.shift, 0.0, T))");
    CHECK(expand("@in(a, @in(b, p))") == "in_a(in_b(p))");
}

TEST_CASE("expandSnippet: mehrere Platzhalter in einem Ausdruck") {
    CHECK(expand("mix(@in(a, p), @in(b, p), @param(amount))") ==
          "mix(in_a(p), in_b(p), P.amount)");
}

TEST_CASE("expandSnippet: fehlerhafte Platzhalter ergeben InvalidArgument") {
    for (const char* broken : {"@unknown(x)", "@", "@in", "@param", "@in(a, p", "@param(x",
                               "@in(a)", "@in(, p)", "@param()", "@param(   )"}) {
        const auto result = expandSnippet(broken, testBindings());
        INFO(broken);
        REQUIRE_FALSE(result.ok());
        CHECK(result.error().code == ErrorCode::InvalidArgument);
    }
}

TEST_CASE("expandSnippet: Fehler der Bindings werden unverändert weitergegeben") {
    const auto result = expandSnippet("return @in(bad, p);", testBindings());
    REQUIRE_FALSE(result.ok());
    CHECK(result.error().code == ErrorCode::NotFound);
    CHECK(result.error().message == "kein Port bad");
}
