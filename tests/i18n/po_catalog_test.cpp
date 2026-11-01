// po_catalog_test.cpp: Tests des Katalogleser (wh_i18n) und der mitgelieferten Übersetzungen.
//
// Warum es den Test gibt: Die drei Kataloge (en, fr, es) entstehen aus dem Quelltext und per Hand. Dieser Test
// fällt auf, wenn eine Datei nicht mehr lesbar ist, eine Übersetzung fehlt oder ein Platzhalter (%1) verloren
// geht, und das schon beim normalen Testlauf, ohne Qt.
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <string>

#include "whitehole/i18n/language.hpp"
#include "whitehole/i18n/po_catalog.hpp"

using namespace whitehole;
using whitehole::i18n::PoCatalog;

namespace {

std::string readFile(const std::filesystem::path& path) {
    std::ifstream in{path, std::ios::binary};
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

std::filesystem::path i18nDir() {
    return std::filesystem::path{__FILE__}.parent_path().parent_path().parent_path() / "apps" / "whitehole_studio" / "i18n";
}

}  // namespace

TEST_CASE("PoCatalog: einfache Einträge, Kopf und Kommentare", "[i18n]") {
    const auto catalog = PoCatalog::parse(R"PO(msgid ""
msgstr ""
"Language: en\n"

# Kommentar
#: datei.cpp:12
msgid "&Datei"
msgstr "&File"

msgid "Speichern"
msgstr "Save"
)PO");
    REQUIRE(catalog.ok());
    REQUIRE(catalog.value().find("&Datei") != nullptr);
    CHECK(*catalog.value().find("&Datei") == "&File");
    CHECK(*catalog.value().find("Speichern") == "Save");
    CHECK(catalog.value().find("Gibt es nicht") == nullptr);
    CHECK(catalog.value().sources().size() == 2);  // der Kopf (msgid "") zählt nicht
    CHECK(catalog.value().translatedCount() == 2);
}

TEST_CASE("PoCatalog: mehrzeilige Texte und Escapes", "[i18n]") {
    const auto catalog = PoCatalog::parse(R"PO(msgid ""
"Erste Zeile\n"
"Zweite \"Zeile\"\t\\"
msgstr ""
"First line\n"
"Second \"line\""
)PO");
    REQUIRE(catalog.ok());
    const std::string* found = catalog.value().find("Erste Zeile\nZweite \"Zeile\"\t\\");
    REQUIRE(found != nullptr);
    CHECK(*found == "First line\nSecond \"line\"");
}

TEST_CASE("PoCatalog: leere und unscharfe (fuzzy) Einträge gelten als nicht übersetzt", "[i18n]") {
    const auto catalog = PoCatalog::parse(R"PO(msgid "Eins"
msgstr ""

#, fuzzy
msgid "Zwei"
msgstr "Two (unsicher)"

msgid "Drei"
msgstr "Three"

#~ msgid "Alt"
#~ msgstr "Old"
)PO");
    REQUIRE(catalog.ok());
    CHECK(catalog.value().find("Eins") == nullptr);
    CHECK(catalog.value().find("Zwei") == nullptr);
    CHECK(catalog.value().find("Drei") != nullptr);
    CHECK(catalog.value().untranslated().size() == 2);
    CHECK(catalog.value().sources().size() == 3);
    CHECK(catalog.value().find("Alt") == nullptr);
}

TEST_CASE("PoCatalog: msgctxt wird übergangen, Datei ohne Schlusszeile", "[i18n]") {
    const auto catalog = PoCatalog::parse("msgctxt \"Menü\"\nmsgid \"Offen\"\nmsgstr \"Open\"");
    REQUIRE(catalog.ok());
    REQUIRE(catalog.value().find("Offen") != nullptr);
    CHECK(*catalog.value().find("Offen") == "Open");
}

TEST_CASE("PoCatalog: Fehler nennen die Zeile", "[i18n]") {
    CHECK_FALSE(PoCatalog::parse("msgstr \"x\"\n").ok());
    CHECK_FALSE(PoCatalog::parse("msgid \"a\"\n").ok());
    CHECK_FALSE(PoCatalog::parse("msgid \"a\nmsgstr \"b\"\n").ok());
    CHECK_FALSE(PoCatalog::parse("msgid \"a\\q\"\nmsgstr \"b\"\n").ok());
    CHECK_FALSE(PoCatalog::parse("\"lose Zeile\"\n").ok());
    CHECK_FALSE(PoCatalog::parse("msgid \"a\"\nmsgid_plural \"b\"\nmsgstr[0] \"c\"\n").ok());
    CHECK_FALSE(PoCatalog::parse("blabla\n").ok());
    const auto bad = PoCatalog::parse("msgid \"ok\"\nmsgstr \"ok\"\n\nmsgstr \"x\"\n");
    REQUIRE_FALSE(bad.ok());
    CHECK(bad.error().message.find("Zeile 4") != std::string::npos);
}

TEST_CASE("PoCatalog: beliebiger Müll stürzt nicht ab", "[i18n]") {
    std::mt19937 rng{12345};
    const std::string alphabet = "msgidtrcxp \"\\\n#,:~[]0123456789nt%äö";
    for (int round = 0; round < 3000; ++round) {
        std::string text;
        const std::size_t length = rng() % 120;
        for (std::size_t i = 0; i < length; ++i) {
            text.push_back(alphabet[rng() % alphabet.size()]);
        }
        const auto parsed = PoCatalog::parse(text);  // ok oder Fehler, aber nie ein Absturz
        if (parsed.ok()) {
            (void)parsed.value().find("msgid");
        }
    }
    SUCCEED();
}

TEST_CASE("placeholdersMatch: gleiche Platzhalter, auch in anderer Reihenfolge", "[i18n]") {
    CHECK(i18n::placeholdersMatch("%1 von %2", "%2 of %1"));
    CHECK(i18n::placeholdersMatch("Kein Platzhalter", "No placeholder"));
    CHECK_FALSE(i18n::placeholdersMatch("%1 von %2", "%1 of"));
    CHECK_FALSE(i18n::placeholdersMatch("%1", "%1 %1"));
    CHECK_FALSE(i18n::placeholdersMatch("Wert %1", "Valeur"));
    CHECK(i18n::placeholdersMatch("100 %", "100 %"));       // Prozentzeichen ohne Ziffer ist kein Platzhalter
    CHECK(i18n::placeholdersMatch("%n Dateien", "%n files"));
}

TEST_CASE("escapePo und parse sind zueinander invers", "[i18n]") {
    const std::string text = "Zeile\n\"zitiert\"\t\\Ende";
    const auto catalog = PoCatalog::parse("msgid \"" + i18n::escapePo(text) + "\"\nmsgstr \"x\"\n");
    REQUIRE(catalog.ok());
    CHECK(catalog.value().find(text) != nullptr);
}

TEST_CASE("Sprachen: Gebietsschema wird auf eine bekannte Sprache abgebildet", "[i18n]") {
    CHECK(i18n::languageFromLocale("de_DE.UTF-8") == "de");
    CHECK(i18n::languageFromLocale("fr_CA") == "fr");
    CHECK(i18n::languageFromLocale("es-MX") == "es");
    CHECK(i18n::languageFromLocale("ES") == "es");
    CHECK(i18n::languageFromLocale("en_US") == "en");
    CHECK(i18n::languageFromLocale("C") == "en");
    CHECK(i18n::languageFromLocale("") == "en");
    CHECK(i18n::languageFromLocale("ja_JP") == "en");
    CHECK(i18n::isKnownLanguage("fr"));
    CHECK_FALSE(i18n::isKnownLanguage("it"));
    CHECK(i18n::kLanguages.size() == 4);
}

TEST_CASE("Kataloge: en, fr und es sind vollständig und haben dieselben Platzhalter", "[i18n]") {
    const auto pot = PoCatalog::parse(readFile(i18nDir() / "messages.pot"));
    REQUIRE(pot.ok());
    REQUIRE(pot.value().sources().size() > 300);
    for (const char* lang : {"en", "fr", "es"}) {
        INFO(lang);
        const auto catalog = PoCatalog::parse(readFile(i18nDir() / (std::string{lang} + ".po")));
        REQUIRE(catalog.ok());
        CHECK(catalog.value().untranslated().empty());
        CHECK(catalog.value().sources().size() == pot.value().sources().size());
        std::size_t differing = 0;
        for (const std::string& source : pot.value().sources()) {
            const std::string* translation = catalog.value().find(source);
            INFO(source);
            REQUIRE(translation != nullptr);
            CHECK(i18n::placeholdersMatch(source, *translation));
            // Die Beschleuniger (&) dürfen nicht verloren gehen oder sich verdoppeln: gleiche Zahl oder keine.
            const auto ampersands = [](const std::string& s) { return std::count(s.begin(), s.end(), '&'); };
            if (ampersands(source) == 1) {
                CHECK(ampersands(*translation) == 1);
            }
            differing += (*translation != source) ? 1U : 0U;
        }
        // Die allermeisten Texte unterscheiden sich vom Deutschen (Rest: Eigennamen, Zahlen, Symbole).
        CHECK(differing > pot.value().sources().size() * 8 / 10);
    }
}
