#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <string>
#include <vector>

#include "libslic3r/Preset.hpp"
#include "libslic3r/PrintConfig.hpp"
#include "slic3r/Utils/SnapmakerPrinterAgent.hpp"

using namespace Slic3r;

namespace {

struct FilamentTestCollection : public PresetCollection
{
    FilamentTestCollection()
        : PresetCollection(Preset::TYPE_FILAMENT, Preset::filament_options(),
                           static_cast<const PrintRegionConfig &>(FullPrintConfig::defaults()))
    {}
};

struct FilamentSpec
{
    const char *name;
    const char *filament_id;
    const char *vendor;
    const char *filament_type;
    const char *colour;
};

// Bambu keeps every PLA variant at filament_type "PLA" with the variant only in the name, while
// Goofoo puts the variant in filament_type as well.
const std::vector<FilamentSpec> library{
    {"Bambu PLA Basic @System",  "GFA00",    "Bambu Lab", "PLA",      "#FFFFFF"},
    {"Bambu PLA Matte @System",  "GFA01",    "Bambu Lab", "PLA",      "#000000"},
    {"Bambu PLA Silk @System",   "GFA05",    "Bambu Lab", "PLA",      "#F4A925"},
    {"Bambu PLA Metal @System",  "OFrKLeE3", "Bambu Lab", "PLA",      "#C0C0C0"},
    {"Bambu PLA-CF @System",     "GFA50",    "Bambu Lab", "PLA-CF",   "#000000"},
    {"Bambu PETG HF @System",    "GFG02",    "Bambu Lab", "PETG",     "#FFFFFF"},
    {"Goofoo PLA @System",       "OFoq0Cmt", "Goofoo",    "PLA",      "#FFFFFF"},
    {"Goofoo PLA Silk @System",  "OFpta2gc", "Goofoo",    "PLA Silk", "#000000"},
    {"Goofoo PETG @System",      "OFHdIbiu", "Goofoo",    "PETG",     "#FFFFFF"},
    {"Goofoo PETG HF @System",   "OF5bjxyx", "Goofoo",    "PETG HF",  "#000000"},
    {"Generic PLA @System",      "GFL99",    "Generic",   "PLA",      "#FFFFFF"},
};

// Load the specs, then stamp the fields the matcher reads in a second pass: load_preset() keeps
// the collection sorted, so a reference taken while loading can be left dangling.
std::string match(const std::string &vendor, const std::string &type, const std::string &sub_type,
                  const std::string &color_rgba)
{
    FilamentTestCollection filaments;
    for (const FilamentSpec &spec : library) {
        DynamicPrintConfig config(filaments.default_preset().config);
        config.option<ConfigOptionStrings>("filament_type", true)->values           = {spec.filament_type};
        config.option<ConfigOptionStrings>("filament_vendor", true)->values         = {spec.vendor};
        config.option<ConfigOptionStrings>("default_filament_colour", true)->values = {spec.colour};
        filaments.load_preset(std::string(), spec.name, config, /*select=*/false);
    }
    for (auto it = filaments.begin(); it != filaments.end(); ++it) {
        const auto spec = std::find_if(library.begin(), library.end(),
                                       [&](const FilamentSpec &s) { return it->name == s.name; });
        if (spec == library.end())
            continue;
        it->filament_id   = spec->filament_id;
        it->is_system     = true;
        it->is_default    = false;
        it->is_visible    = true;
        it->is_compatible = true;
    }
    return SnapmakerPrinterAgent::match_filament_preset(filaments, vendor, type, sub_type, color_rgba);
}

} // namespace

TEST_CASE("A slot with a subtype matches the vendor preset named for it", "[Snapmaker][FilamentSync]")
{
    // A white matte spool must not take the white Basic preset on color alone.
    CHECK(match("Bambu Lab", "PLA", "Matte", "FFFFFFFF") == "GFA01");
    CHECK(match("Bambu Lab", "PLA", "Silk", "FFFFFFFF") == "GFA05");
}

TEST_CASE("A subtype the slicer does not recognize still selects the preset named for it", "[Snapmaker][FilamentSync]")
{
    CHECK(match("Bambu Lab", "PLA", "Basic", "000000FF") == "GFA00");
}

TEST_CASE("A subtype only matches whole words of a preset name", "[Snapmaker][FilamentSync]")
{
    CHECK(match("Bambu Lab", "PLA", "Metal", "FFFFFFFF") == "OFrKLeE3");
    CHECK(match("Bambu Lab", "PLA", "Meta", "FFFFFFFF") == "GFA00");
}

TEST_CASE("A preset type carrying the subtype matches regardless of case", "[Snapmaker][FilamentSync]")
{
    CHECK(match("Goofoo", "PLA", "Silk", "FFFFFFFF") == "OFpta2gc");
    CHECK(match("Goofoo", "PETG", "HF", "FFFFFFFF") == "OF5bjxyx");
}

TEST_CASE("A preset declaring the combined type beats one only named for the subtype", "[Snapmaker][FilamentSync]")
{
    CHECK(match("Bambu Lab", "PLA", "CF", "FFFFFFFF") == "GFA50");
}

TEST_CASE("A slot without a subtype matches the vendor preset closest in color", "[Snapmaker][FilamentSync]")
{
    CHECK(match("Bambu Lab", "PLA", "", "000000FF") == "GFA01");
    CHECK(match("Bambu Lab", "PLA", "NONE", "FFFFFFFF") == "GFA00");
}

TEST_CASE("A slot's vendor matches presets regardless of case", "[Snapmaker][FilamentSync]")
{
    CHECK(match("BAMBU LAB", "PLA", "Matte", "FFFFFFFF") == "GFA01");
}

TEST_CASE("A slot only matches presets of its own vendor and material", "[Snapmaker][FilamentSync]")
{
    CHECK(match("Bambu Lab", "PETG", "Matte", "000000FF") == "GFG02");
    CHECK(match("Bambu Lab", "ABS", "", "FFFFFFFF").empty());
    CHECK(match("Sunlu", "PLA", "Matte", "FFFFFFFF").empty());
}
