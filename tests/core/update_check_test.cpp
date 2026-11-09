// update_check_test.cpp: Tests zu core/update_check.hpp (Versionen vergleichen, GitHub-Antwort lesen).
//
// Warum es diese Datei gibt: Die Update-Benachrichtigung darf bei seltsamen Antworten nie etwas Falsches
// anzeigen oder öffnen. Die Fälle hier sind die, die im Netz vorkommen: gleiche Feldnamen in verschachtelten
// Objekten, Anführungszeichen im Text, Vorabversionen, fremde Links, kaputtes JSON.
#include <catch2/catch_test_macros.hpp>

#include <string>

#include "whitehole/core/update_check.hpp"

using whitehole::core::parseLatestRelease;
using whitehole::core::parseVersion;
using whitehole::core::Version;

TEST_CASE("Versionsnummern lesen", "[update]") {
    CHECK(parseVersion("0.65.0") == Version{0, 65, 0});
    CHECK(parseVersion("v0.65.1") == Version{0, 65, 1});
    CHECK(parseVersion("V1.2.3") == Version{1, 2, 3});
    CHECK(parseVersion("0.64.0-1") == Version{0, 64, 0});  // Paketnummer des .deb
    CHECK(parseVersion("1.0") == Version{1, 0, 0});
    CHECK(parseVersion("2") == Version{2, 0, 0});
    CHECK_FALSE(parseVersion("").has_value());
    CHECK_FALSE(parseVersion("v").has_value());
    CHECK_FALSE(parseVersion("abc").has_value());
    CHECK_FALSE(parseVersion("1.2.x").has_value());
    CHECK_FALSE(parseVersion("1.2.3x").has_value());
    CHECK_FALSE(parseVersion("1..2").has_value());
    CHECK_FALSE(parseVersion("99999999999.0.0").has_value());
}

TEST_CASE("Versionen vergleichen", "[update]") {
    CHECK(Version{0, 65, 0} > Version{0, 64, 9});
    CHECK(Version{0, 10, 0} > Version{0, 9, 0});  // numerisch, nicht als Text
    CHECK(Version{1, 0, 0} > Version{0, 99, 99});
    CHECK(Version{0, 64, 0} == Version{0, 64, 0});
    CHECK(whitehole::core::toString(Version{0, 65, 0}) == "0.65.0");
}

namespace {

// Gekürzt nach der echten Antwort: "author" und "assets" enthalten eigene html_url-Felder.
const std::string kRelease = R"({
  "url": "https://api.github.com/repos/bsteinmann/WhiteHoleFX/releases/1",
  "html_url": "https://github.com/bsteinmann/WhiteHoleFX/releases/tag/v0.65.0",
  "id": 1,
  "author": {"login": "x", "html_url": "https://github.com/x", "tag_name": "v9.9.9"},
  "tag_name": "v0.65.0",
  "draft": false,
  "prerelease": false,
  "body": "Text mit \"Anführungszeichen\", {Klammern} und \\ Schrägstrich",
  "assets": [{"name": "a.zip", "html_url": "https://evil.example/", "uploader": {"tag_name": "v8.8.8"}}]
})";

}  // namespace

TEST_CASE("Neuestes Release lesen", "[update]") {
    const auto release = parseLatestRelease(kRelease);
    REQUIRE(release.has_value());
    CHECK(release->tag == "v0.65.0");
    CHECK(release->version == Version{0, 65, 0});
    CHECK(release->url == "https://github.com/bsteinmann/WhiteHoleFX/releases/tag/v0.65.0");
}

TEST_CASE("Entwürfe und Vorabversionen werden verworfen", "[update]") {
    auto withFlag = [](const char* flag) {
        return std::string{R"({"tag_name":"v1.0.0","html_url":"https://github.com/a/b",)"} + flag + "}";
    };
    CHECK(parseLatestRelease(withFlag(R"("draft":false,"prerelease":false)")).has_value());
    CHECK_FALSE(parseLatestRelease(withFlag(R"("draft":true)")).has_value());
    CHECK_FALSE(parseLatestRelease(withFlag(R"("prerelease":true)")).has_value());
}

TEST_CASE("Ungültige oder gefährliche Antworten ergeben nichts", "[update]") {
    CHECK_FALSE(parseLatestRelease("").has_value());
    CHECK_FALSE(parseLatestRelease("[]").has_value());
    CHECK_FALSE(parseLatestRelease("Not Found").has_value());
    CHECK_FALSE(parseLatestRelease(R"({"message":"Not Found"})").has_value());
    CHECK_FALSE(parseLatestRelease(R"({"tag_name":"v1.0.0"})").has_value());                                       // ohne Link
    CHECK_FALSE(parseLatestRelease(R"({"tag_name":"v1.0.0","html_url":"https://evil.example/x"})").has_value());  // fremder Host
    CHECK_FALSE(parseLatestRelease(R"({"tag_name":"v1.0.0","html_url":"http://github.com/a"})").has_value());      // kein https
    CHECK_FALSE(parseLatestRelease(R"({"tag_name":"latest","html_url":"https://github.com/a"})").has_value());     // keine Version
    CHECK_FALSE(parseLatestRelease(R"({"tag_name":"v1.0.0","html_url":"https://github.com/a)").has_value());       // abgeschnitten
    CHECK_FALSE(parseLatestRelease(R"({"tag_name" "v1.0.0"})").has_value());                                       // Doppelpunkt fehlt
}

TEST_CASE("Felder in verschachtelten Objekten zählen nicht", "[update]") {
    // tag_name und html_url stehen nur im Unterobjekt: nichts auf der obersten Ebene.
    CHECK_FALSE(parseLatestRelease(R"({"x":{"tag_name":"v1.0.0","html_url":"https://github.com/a"}})").has_value());
}
