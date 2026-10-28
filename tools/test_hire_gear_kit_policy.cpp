// Standalone test for the `.bot gear` kit refit rules: the kit key a live
// hire's talents resolve to, and the row a slot takes under an item-level cap.
//
// Build and run (from the repo root):
//   g++ -std=c++17 -Wall -Wextra tools/test_hire_gear_kit_policy.cpp -o /tmp/test_hire_gear_kit
//   /tmp/test_hire_gear_kit

#include "../runtime/policy/hire/HireGearKitPolicy.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace TortoiseBots;

static int checks = 0;
#define CHECK(cond) do { \
    ++checks; \
    if (!(cond)) { \
        std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        std::exit(1); \
    } \
} while (0)

static bool IsHireGearSpecKey(uint8_t classId, char const* key)
{
    uint32_t count = 0;
    HireSpecOptions(classId, count);
    for (uint32_t i = 0; i < count; ++i)
        if (std::strcmp(HireGearSpecKey(classId, static_cast<int>(i)), key) == 0)
            return true;
    return false;
}

// Every talent spec, played as tank or not, resolves to a key the hire-gear
// table is generated for; otherwise the refit would find no rows.
static void TestKeysExistForEverySpec()
{
    struct { PlayerTalentSpec first, last; uint8_t classId; } const classes[] = {
        { PlayerTalentSpec::TALENT_SPEC_WARRIOR_ARMS, PlayerTalentSpec::TALENT_SPEC_WARRIOR_PROTECTION, kHireClassWarrior },
        { PlayerTalentSpec::TALENT_SPEC_PALADIN_HOLY, PlayerTalentSpec::TALENT_SPEC_PALADIN_RETRIBUTION, kHireClassPaladin },
        { PlayerTalentSpec::TALENT_SPEC_HUNTER_BEAST_MASTERY, PlayerTalentSpec::TALENT_SPEC_HUNTER_SURVIVAL, kHireClassHunter },
        { PlayerTalentSpec::TALENT_SPEC_ROGUE_ASSASSINATION, PlayerTalentSpec::TALENT_SPEC_ROGUE_SUBTLETY, kHireClassRogue },
        { PlayerTalentSpec::TALENT_SPEC_PRIEST_DISCIPLINE, PlayerTalentSpec::TALENT_SPEC_PRIEST_SHADOW, kHireClassPriest },
        { PlayerTalentSpec::TALENT_SPEC_SHAMAN_ELEMENTAL, PlayerTalentSpec::TALENT_SPEC_SHAMAN_RESTORATION, kHireClassShaman },
        { PlayerTalentSpec::TALENT_SPEC_MAGE_ARCANE, PlayerTalentSpec::TALENT_SPEC_MAGE_FROST, kHireClassMage },
        { PlayerTalentSpec::TALENT_SPEC_WARLOCK_AFFLICTION, PlayerTalentSpec::TALENT_SPEC_WARLOCK_DESTRUCTION, kHireClassWarlock },
        { PlayerTalentSpec::TALENT_SPEC_DRUID_BALANCE, PlayerTalentSpec::TALENT_SPEC_DRUID_RESTORATION, kHireClassDruid },
    };
    for (auto const& c : classes)
        for (uint8_t s = static_cast<uint8_t>(c.first); s <= static_cast<uint8_t>(c.last); ++s)
            for (bool tank : { false, true })
            {
                char const* key = HireGearKeyForTalentSpec(static_cast<PlayerTalentSpec>(s), tank);
                CHECK(key && IsHireGearSpecKey(c.classId, key));
            }
    CHECK(HireGearKeyForTalentSpec(PlayerTalentSpec::TALENT_SPEC_INVALID, false) == nullptr);
}

// The role played splits the two shared trees.
static void TestRoleSplitsSharedTrees()
{
    CHECK(std::strcmp(HireGearKeyForTalentSpec(PlayerTalentSpec::TALENT_SPEC_DRUID_FERAL, true), "bear") == 0);
    CHECK(std::strcmp(HireGearKeyForTalentSpec(PlayerTalentSpec::TALENT_SPEC_DRUID_FERAL, false), "cat") == 0);
    CHECK(std::strcmp(HireGearKeyForTalentSpec(PlayerTalentSpec::TALENT_SPEC_SHAMAN_ENHANCEMENT, true), "tank") == 0);
    CHECK(std::strcmp(HireGearKeyForTalentSpec(PlayerTalentSpec::TALENT_SPEC_SHAMAN_ENHANCEMENT, false), "enhancement") == 0);
    CHECK(std::strcmp(HireGearKeyForTalentSpec(PlayerTalentSpec::TALENT_SPEC_WARRIOR_PROTECTION, false), "protection") == 0);
}

namespace
{
// Core equipment slot numbers (EQUIPMENT_SLOT_HEAD, EQUIPMENT_SLOT_WAIST,
// EQUIPMENT_SLOT_OFFHAND, EQUIPMENT_SLOT_END).
constexpr uint8_t kSlotHead = 0;
constexpr uint8_t kSlotOffhand = 16;
constexpr uint8_t kSlotWaist = 5;
constexpr uint8_t kSlotEnd = 19;

// Fixture kits: a head piece per tier, whose item id doubles as its item level
// so a cap can be placed between two kits.
constexpr uint32_t kHeadIlvlBase = 60;
constexpr uint32_t kHeadIlvlPerTier = 5;
constexpr uint32_t kShieldItem = 70;
constexpr uint32_t kPreRaidOffhandItem = 61;
constexpr uint8_t kTierPreRaid = 1;
constexpr uint8_t kTierMoltenCore = 3;
constexpr uint8_t kTierAhnQiraj = 5;
constexpr uint8_t kTierNaxxramas = 6;
constexpr uint8_t kTierKarazhan = 7;

uint32_t HeadIlvl(uint8_t tier) { return kHeadIlvlBase + kHeadIlvlPerTier * tier; }

HireGearRow Row(uint8_t tier, uint8_t slot, uint32_t item)
{
    HireGearRow r;
    r.classId = kHireClassWarrior;
    std::strcpy(r.spec, "arms");
    r.tier = tier;
    r.slot = slot;
    r.itemId = item;
    return r;
}
} // namespace

static void TestCapChoice()
{
    std::vector<HireGearRow> rows;
    for (uint8_t t = kTierPreRaid; t <= kHireGearTierMax; ++t)
        rows.push_back(Row(t, kSlotHead, HeadIlvl(t)));
    rows.push_back(Row(kTierNaxxramas, kSlotOffhand, 0));            // two-hander kit: offhand cleared
    rows.push_back(Row(kTierAhnQiraj, kSlotOffhand, kShieldItem));
    rows.push_back(Row(kTierMoltenCore, kSlotOffhand, 0));
    rows.push_back(Row(kTierPreRaid, kSlotOffhand, kPreRaidOffhandItem));
    auto ilvl = [](uint32_t item) { return item; };
    HireGearRow const* out = nullptr;
    auto choose = [&](uint8_t tier, uint8_t slot, uint32_t cap) {
        return ChooseKitSlotRow(rows.data(), rows.size(), kHireClassWarrior, "arms", 0, tier, slot, cap, ilvl, out);
    };

    // No cap, or a cap the kit meets: the kit's own piece.
    CHECK(choose(kTierNaxxramas, kSlotHead, 0) == KitSlotChoice::Equip && out->itemId == HeadIlvl(kTierNaxxramas));
    CHECK(choose(kTierNaxxramas, kSlotHead, HeadIlvl(kTierNaxxramas)) == KitSlotChoice::Equip && out->itemId == HeadIlvl(kTierNaxxramas));
    // A cap under the kit: the highest lower kit that fits.
    CHECK(choose(kTierNaxxramas, kSlotHead, HeadIlvl(kTierMoltenCore) + 1) == KitSlotChoice::Equip && out->itemId == HeadIlvl(kTierMoltenCore));
    // Even the pre-raid kit is over the cap: the slot empties.
    CHECK(choose(kTierNaxxramas, kSlotHead, HeadIlvl(kTierPreRaid) - 1) == KitSlotChoice::Nothing);
    // The kit itself wants the slot empty, cap or not.
    CHECK(choose(kTierNaxxramas, kSlotOffhand, 0) == KitSlotChoice::Clear);
    // Walking down skips a lower kit's cleared slot and lands on a worn piece.
    CHECK(choose(kTierAhnQiraj, kSlotOffhand, kShieldItem - 1) == KitSlotChoice::Equip && out->itemId == kPreRaidOffhandItem);
    // Slots the kit does not list are left alone.
    CHECK(choose(kTierKarazhan, kSlotOffhand, 0) == KitSlotChoice::NoRow);
    CHECK(choose(kTierNaxxramas, kSlotWaist, 0) == KitSlotChoice::NoRow);
}

// The shipped table (tools/hire_gear/data/hire_gear.csv: class,spec,race,tier,
// slot,item_id,...). Empty when the test runs from elsewhere than the repo.
static std::vector<HireGearRow> LoadShippedRows()
{
    std::vector<HireGearRow> rows;
    std::ifstream in;
    for (char const* path : { "tools/hire_gear/data/hire_gear.csv", "../tools/hire_gear/data/hire_gear.csv", "hire_gear/data/hire_gear.csv" })
    {
        in.open(path);
        if (in.is_open())
            break;
    }
    std::string line;
    std::getline(in, line); // header
    while (std::getline(in, line))
    {
        std::istringstream fields(line);
        std::string field[6];
        for (std::string& f : field)
            std::getline(fields, f, ',');
        HireGearRow r;
        r.classId = static_cast<uint8_t>(std::atoi(field[0].c_str()));
        std::strncpy(r.spec, field[1].c_str(), sizeof(r.spec) - 1);
        r.race = static_cast<uint8_t>(std::atoi(field[2].c_str()));
        r.tier = static_cast<uint8_t>(std::atoi(field[3].c_str()));
        r.slot = static_cast<uint8_t>(std::atoi(field[4].c_str()));
        r.itemId = static_cast<uint32_t>(std::atol(field[5].c_str()));
        rows.push_back(r);
    }
    return rows;
}

// On the shipped rows: every kit key the command can produce has a kit at
// every tier, and without a cap the refit picks exactly the kit's own row.
static void TestShippedKits()
{
    std::vector<HireGearRow> const rows = LoadShippedRows();
    if (rows.empty())
    {
        std::printf("test_hire_gear_kit_policy: shipped rows not found, skipping that section\n");
        return;
    }
    auto noItemLevel = [](uint32_t) { return 0u; };
    for (uint8_t s = static_cast<uint8_t>(PlayerTalentSpec::TALENT_SPEC_WARRIOR_ARMS); s <= static_cast<uint8_t>(PlayerTalentSpec::TALENT_SPEC_DRUID_RESTORATION); ++s)
    {
        for (bool tank : { false, true })
        {
            char const* key = HireGearKeyForTalentSpec(static_cast<PlayerTalentSpec>(s), tank);
            if (!key)
                continue;
            uint8_t classId = 0;
            for (HireGearRow const& r : rows)
                if (std::strcmp(r.spec, key) == 0 && IsHireGearSpecKey(r.classId, key))
                    classId = r.classId;
            CHECK(classId != 0);
            for (uint8_t tier = kHireGearTierBase + 1; tier <= kHireGearTierMax; ++tier)
                for (uint8_t slot = 0; slot < kSlotEnd; ++slot)
                {
                    HireGearRow const* own = nullptr;
                    HireGearRow const* chosen = nullptr;
                    bool const has = LookupHireGearRow(rows.data(), rows.size(), classId, key, 0, tier, slot, own);
                    KitSlotChoice const choice = ChooseKitSlotRow(rows.data(), rows.size(), classId, key, 0, tier, slot, 0, noItemLevel, chosen);
                    CHECK(has == (choice != KitSlotChoice::NoRow));
                    if (has && own->itemId)
                        CHECK(choice == KitSlotChoice::Equip && chosen == own);
                    if (slot == kSlotHead)
                        CHECK(has && own->itemId); // every kit at least has a head piece
                }
        }
    }
}

static void TestIlvlWord()
{
    uint32_t v = 0;
    CHECK(ParseKitIlvlWord("ilvl=66", v) && v == 66);
    CHECK(ParseKitIlvlWord("ilvl=1", v) && v == 1);
    CHECK(!ParseKitIlvlWord("ilvl=", v));
    CHECK(!ParseKitIlvlWord("ilvl=0", v));
    CHECK(!ParseKitIlvlWord("ilvl=66x", v));
    CHECK(!ParseKitIlvlWord("ilvl=-5", v));
    CHECK(!ParseKitIlvlWord("66", v));
    CHECK(!ParseKitIlvlWord("t6", v));
}

int main()
{
    TestKeysExistForEverySpec();
    TestRoleSplitsSharedTrees();
    TestCapChoice();
    TestShippedKits();
    TestIlvlWord();
    std::printf("test_hire_gear_kit_policy: %d checks passed\n", checks);
    return 0;
}
