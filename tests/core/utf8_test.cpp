// utf8_test.cpp: decodeUtf8 zerlegt Text in Codepunkte und übersteht kaputte Eingaben.
#include <catch2/catch_test_macros.hpp>

#include <string>

#include "whitehole/core/utf8.hpp"

using whitehole::decodeUtf8;
using whitehole::kReplacementCodePoint;
using Points = std::vector<std::uint32_t>;

TEST_CASE("decodeUtf8: ASCII, Umlaute, Mehrbytezeichen", "[core][utf8]") {
    CHECK(decodeUtf8("") == Points{});
    CHECK(decodeUtf8("Hi!") == Points{72, 105, 33});
    CHECK(decodeUtf8("\xC3\xA4\xC3\xB6\xC3\xBC\xC3\x9F") == Points{0xE4, 0xF6, 0xFC, 0xDF});  // ä ö ü ß
    CHECK(decodeUtf8("\xE2\x82\xAC") == Points{0x20AC});                                    // €
    CHECK(decodeUtf8("\xF0\x9F\x98\x80") == Points{0x1F600});                                // Emoji (4 Bytes)
}

TEST_CASE("decodeUtf8: kaputte Folgen werden zu U+FFFD, der Rest bleibt erhalten", "[core][utf8]") {
    CHECK(decodeUtf8("a\x80z") == Points{'a', kReplacementCodePoint, 'z'});         // Folgebyte allein
    CHECK(decodeUtf8("\xC3") == Points{kReplacementCodePoint});                       // abgebrochen am Ende
    CHECK(decodeUtf8("\xC3z") == Points{kReplacementCodePoint, 'z'});                 // Folgebyte fehlt
    CHECK(decodeUtf8("\xC0\x80") == Points{kReplacementCodePoint, kReplacementCodePoint});  // Überlänge
    CHECK(decodeUtf8("\xED\xA0\x80") == Points{kReplacementCodePoint, kReplacementCodePoint, kReplacementCodePoint});  // Ersatzpaar
    CHECK(decodeUtf8("\xF5\x80\x80\x80") ==
          Points{kReplacementCodePoint, kReplacementCodePoint, kReplacementCodePoint, kReplacementCodePoint});  // über U+10FFFF
}

TEST_CASE("decodeUtf8: die Höchstzahl schneidet ab", "[core][utf8]") {
    CHECK(decodeUtf8("abcdef", 3) == Points{'a', 'b', 'c'});
    CHECK(decodeUtf8("\xC3\xA4\xC3\xA4\xC3\xA4", 2) == Points{0xE4, 0xE4});
    CHECK(decodeUtf8("abc", 0).empty());
}
