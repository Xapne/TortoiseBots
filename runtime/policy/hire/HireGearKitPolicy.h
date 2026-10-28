#pragma once

// Refitting a hired companion to a gear kit after the hire (`.bot gear`): the
// kit key for the talents it plays, and the row a slot takes under an
// item-level cap. Pure policy over the hire-gear rows, unit-tested standalone
// (tools/test_hire_gear_kit_policy.cpp).

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "../../../ai/playerbot/PlayerTalentSpec.h"
#include "HireGearPolicy.h"

namespace TortoiseBots
{

// The hire-gear kit key for the talents a live bot plays. Feral druids and
// enhancement shamans own two kits; the role the bot fills in its group picks
// one. Null when the talents name no hireable spec.
inline char const* HireGearKeyForTalentSpec(PlayerTalentSpec spec, bool tank)
{
    switch (spec)
    {
        case PlayerTalentSpec::TALENT_SPEC_WARRIOR_ARMS: return "arms";
        case PlayerTalentSpec::TALENT_SPEC_WARRIOR_FURY: return "fury";
        case PlayerTalentSpec::TALENT_SPEC_WARRIOR_PROTECTION: return "protection";
        case PlayerTalentSpec::TALENT_SPEC_PALADIN_HOLY: return "holy";
        case PlayerTalentSpec::TALENT_SPEC_PALADIN_PROTECTION: return "protection";
        case PlayerTalentSpec::TALENT_SPEC_PALADIN_RETRIBUTION: return "retribution";
        case PlayerTalentSpec::TALENT_SPEC_HUNTER_BEAST_MASTERY: return "beastmastery";
        case PlayerTalentSpec::TALENT_SPEC_HUNTER_MARKSMANSHIP: return "marksmanship";
        case PlayerTalentSpec::TALENT_SPEC_HUNTER_SURVIVAL: return "survival";
        case PlayerTalentSpec::TALENT_SPEC_ROGUE_ASSASSINATION: return "assassination";
        case PlayerTalentSpec::TALENT_SPEC_ROGUE_COMBAT: return "combat";
        case PlayerTalentSpec::TALENT_SPEC_ROGUE_SUBTLETY: return "subtlety";
        case PlayerTalentSpec::TALENT_SPEC_PRIEST_DISCIPLINE: return "discipline";
        case PlayerTalentSpec::TALENT_SPEC_PRIEST_HOLY: return "holy";
        case PlayerTalentSpec::TALENT_SPEC_PRIEST_SHADOW: return "shadow";
        case PlayerTalentSpec::TALENT_SPEC_SHAMAN_ELEMENTAL: return "elemental";
        case PlayerTalentSpec::TALENT_SPEC_SHAMAN_ENHANCEMENT: return tank ? "tank" : "enhancement";
        case PlayerTalentSpec::TALENT_SPEC_SHAMAN_RESTORATION: return "restoration";
        case PlayerTalentSpec::TALENT_SPEC_MAGE_ARCANE: return "arcane";
        case PlayerTalentSpec::TALENT_SPEC_MAGE_FIRE: return "fire";
        case PlayerTalentSpec::TALENT_SPEC_MAGE_FROST: return "frost";
        case PlayerTalentSpec::TALENT_SPEC_WARLOCK_AFFLICTION: return "affliction";
        case PlayerTalentSpec::TALENT_SPEC_WARLOCK_DEMONOLOGY: return "demonology";
        case PlayerTalentSpec::TALENT_SPEC_WARLOCK_DESTRUCTION: return "destruction";
        case PlayerTalentSpec::TALENT_SPEC_DRUID_BALANCE: return "balance";
        case PlayerTalentSpec::TALENT_SPEC_DRUID_FERAL: return tank ? "bear" : "cat";
        case PlayerTalentSpec::TALENT_SPEC_DRUID_RESTORATION: return "restoration";
        case PlayerTalentSpec::TALENT_SPEC_INVALID: return nullptr;
    }
    return nullptr;
}

enum class KitSlotChoice : uint8_t
{
    NoRow,   // the kit says nothing about this slot: leave it alone
    Clear,   // the kit wants the slot empty (offhand of a two-hander kit)
    Equip,   // out is the row to wear
    Nothing, // every kit at or below the tier is over the cap: empty the slot
};

// The slot's row from the requested kit, or from the highest kit below it
// whose piece is at or under the cap. A cap of 0 is no cap. itemLevelOf maps
// an item id to its item level.
template <typename ItemLevelOf>
inline KitSlotChoice ChooseKitSlotRow(HireGearRow const* rows, size_t count, uint8_t classId, char const* spec,
    uint8_t race, uint8_t tier, uint8_t slot, uint32_t ilvlCap, ItemLevelOf const& itemLevelOf, HireGearRow const*& out)
{
    out = nullptr;
    HireGearRow const* row = nullptr;
    if (!LookupHireGearRow(rows, count, classId, spec, race, tier, slot, row))
        return KitSlotChoice::NoRow;
    if (row->itemId == 0)
        return KitSlotChoice::Clear;
    for (uint8_t t = tier; t > kHireGearTierBase; --t)
    {
        if (t != tier && (!LookupHireGearRow(rows, count, classId, spec, race, t, slot, row) || row->itemId == 0))
            continue;
        if (ilvlCap == 0 || itemLevelOf(row->itemId) <= ilvlCap)
        {
            out = row;
            return KitSlotChoice::Equip;
        }
    }
    return KitSlotChoice::Nothing;
}

// The `ilvl=<n>` word of `.bot gear`, already lower-cased. False when the
// word is not one; the cap must be a whole number above zero.
inline bool ParseKitIlvlWord(char const* word, uint32_t& ilvlOut)
{
    static char const kPrefix[] = "ilvl=";
    size_t const prefixLen = sizeof(kPrefix) - 1;
    if (std::strncmp(word, kPrefix, prefixLen) != 0)
        return false;
    char const* digits = word + prefixLen;
    char* end = nullptr;
    unsigned long const value = std::strtoul(digits, &end, 10);
    if (end == digits || *end != '\0' || value == 0 || value > UINT32_MAX)
        return false;
    ilvlOut = static_cast<uint32_t>(value);
    return true;
}

} // namespace TortoiseBots
