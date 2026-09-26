/*
 * mod-dungeon-consumables
 *
 * Real players get a handful of level-appropriate healing and mana potions in their bags when
 * they enter a dungeon, and lose whatever is left when they leave. The potions are copies of the
 * normal ones with their own entries (see the SQL), so leaving never touches potions the player
 * brought along.
 *
 * Leaving and coming back to the same run (a corpse run, a trip out to repair) gives back what
 * the player had when they left rather than a fresh bundle, so zoning out can't be used to refill.
 * A different run, or the same dungeon after a reset, gets a fresh bundle.
 *
 * Released under GNU GPL v2 or (at your option) any later version.
 */

#include "Chat.h"
#include "Config.h"
#include "DBCStores.h"
#include "Log.h"
#include "Map.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "WorldSession.h"

#include <iterator>
#include <mutex>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{
    enum class Kind : uint8
    {
        Healing,
        Mana
    };

    // Copied from mod-individual-progression's ProgressionState rather than included, so this
    // module builds without it. IP stores a player's state as rewarded quests 66000 + state.
    constexpr uint32 IP_PROGRESSION_QUEST_BASE = 66000;
    constexpr uint8 IP_STATE_MAX = 18;
    constexpr uint8 IP_STATE_PRE_TBC = 8;       // Outland open
    constexpr uint8 IP_STATE_TBC_TIER_5 = 13;   // Northrend open

    struct Tier
    {
        uint32 entry;    // the dungeon copy
        uint32 source;   // the potion it copies, for log messages
        Kind kind;
        uint8 ipState;   // IP state needed on top of the item's RequiredLevel
    };

    // Must match the SQL. Within a kind, lowest level first.
    constexpr uint32 ENTRY_BASE = 9500100;

    Tier const Tiers[] =
    {
        { ENTRY_BASE + 0,  118,   Kind::Healing, 0 },                    // Minor
        { ENTRY_BASE + 1,  858,   Kind::Healing, 0 },                    // Lesser
        { ENTRY_BASE + 2,  929,   Kind::Healing, 0 },                    // Healing
        { ENTRY_BASE + 3,  1710,  Kind::Healing, 0 },                    // Greater
        { ENTRY_BASE + 4,  3928,  Kind::Healing, 0 },                    // Superior
        { ENTRY_BASE + 5,  13446, Kind::Healing, 0 },                    // Major
        { ENTRY_BASE + 6,  22829, Kind::Healing, IP_STATE_PRE_TBC },     // Super
        { ENTRY_BASE + 7,  33447, Kind::Healing, IP_STATE_TBC_TIER_5 },  // Runic
        { ENTRY_BASE + 8,  2455,  Kind::Mana,    0 },                    // Minor
        { ENTRY_BASE + 9,  3385,  Kind::Mana,    0 },                    // Lesser
        { ENTRY_BASE + 10, 3827,  Kind::Mana,    0 },                    // Mana
        { ENTRY_BASE + 11, 6149,  Kind::Mana,    0 },                    // Greater
        { ENTRY_BASE + 12, 13443, Kind::Mana,    0 },                    // Superior
        { ENTRY_BASE + 13, 13444, Kind::Mana,    0 },                    // Major
        { ENTRY_BASE + 14, 22832, Kind::Mana,    IP_STATE_PRE_TBC },     // Super
        { ENTRY_BASE + 15, 33448, Kind::Mana,    IP_STATE_TBC_TIER_5 },  // Runic
    };

    struct Config
    {
        bool enabled = true;
        uint32 healingPotions = 5;
        uint32 manaPotions = 5;
        bool includeRaids = false;
        bool respectIndividualProgression = true;
        bool announce = true;

        // mod-individual-progression's own switch. Defaults to off so that without IP installed
        // nobody is held back to vanilla potions.
        bool ipEnabled = false;
    };

    Config config;

    using ItemCounts = std::vector<std::pair<uint32, uint32>>; // entry, count

    // The run a player last had supplies for.
    struct Run
    {
        uint32 mapId = 0;
        uint32 instanceId = 0;
        bool holding = false;    // the potions are in their bags right now
        ItemCounts leftovers;    // what they had when they last left this run
    };

    std::mutex runsLock;
    std::unordered_map<ObjectGuid::LowType, Run> runs;

    // The playerbots fork adds WorldSession::IsBot(); stock AzerothCore doesn't have it. Looking for
    // it at compile time lets the module build on both.
    template <typename Session, typename = void>
    struct HasIsBot : std::false_type { };

    template <typename Session>
    struct HasIsBot<Session, std::void_t<decltype(std::declval<Session&>().IsBot())>> : std::true_type { };

    template <typename Session>
    bool IsBotSession(Session* session)
    {
        if constexpr (HasIsBot<Session>::value)
            return session->IsBot();
        else
            return false;
    }

    // Bots already drink their own potions through playerbots.
    bool IsRealPlayer(Player* player)
    {
        WorldSession* session = player ? player->GetSession() : nullptr;
        return session && !IsBotSession(session);
    }

    bool IsRunMap(Map const* map)
    {
        return map && (map->IsNonRaidDungeon() || (config.includeRaids && map->IsRaid()));
    }

    uint8 GetProgressionState(Player* player)
    {
        if (!config.respectIndividualProgression || !config.ipEnabled)
            return IP_STATE_MAX;

        // Same walk as IndividualProgression::GetPlayerProgressionFromQuests.
        uint8 state = 0;
        for (uint8 i = 1; i <= IP_STATE_MAX; ++i)
            if (player->GetQuestStatus(IP_PROGRESSION_QUEST_BASE + i) == QUEST_STATUS_REWARDED)
                state = i;

        return state;
    }

    bool UsesMana(Player* player)
    {
        ChrClassesEntry const* classEntry = sChrClassesStore.LookupEntry(player->getClass());
        return classEntry && classEntry->powerType == POWER_MANA;
    }

    // The best potion of a kind the player can use: the highest RequiredLevel at or below their
    // level that their progression state allows. 0 if none (e.g. a level 3 caster and mana).
    uint32 PickTier(Kind kind, uint8 level, uint8 state)
    {
        uint32 best = 0;
        for (Tier const& tier : Tiers)
        {
            if (tier.kind != kind || tier.ipState > state)
                continue;

            ItemTemplate const* proto = sObjectMgr->GetItemTemplate(tier.entry);
            if (!proto || proto->RequiredLevel > level)
                continue;

            best = tier.entry;
        }

        return best;
    }

    ItemCounts BuildBundle(Player* player)
    {
        ItemCounts bundle;
        uint8 const level = player->GetLevel();
        uint8 const state = GetProgressionState(player);

        if (config.healingPotions)
            if (uint32 entry = PickTier(Kind::Healing, level, state))
                bundle.emplace_back(entry, config.healingPotions);

        if (config.manaPotions && UsesMana(player))
            if (uint32 entry = PickTier(Kind::Mana, level, state))
                bundle.emplace_back(entry, config.manaPotions);

        return bundle;
    }

    // Removes every dungeon potion the player has, bank included, and returns what was taken.
    ItemCounts TakeAll(Player* player)
    {
        ItemCounts taken;
        for (Tier const& tier : Tiers)
        {
            uint32 count = player->GetItemCount(tier.entry, true);
            if (!count)
                continue;

            player->DestroyItemCount(tier.entry, count, true);
            taken.emplace_back(tier.entry, count);
        }

        return taken;
    }

    // Same as Player::AddItem, but reports how many actually fit.
    uint32 Give(Player* player, uint32 entry, uint32 count)
    {
        uint32 noSpaceForCount = 0;
        ItemPosCountVec dest;
        InventoryResult msg = player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, entry, count, &noSpaceForCount);
        if (msg != EQUIP_ERR_OK)
            count -= noSpaceForCount;

        if (!count || dest.empty())
            return 0;

        if (Item* item = player->StoreNewItem(dest, entry, true))
            player->SendNewItem(item, count, true, false);

        return count;
    }

    void GiveAll(Player* player, ItemCounts const& items, bool fresh)
    {
        uint32 missing = 0;
        for (auto const& [entry, count] : items)
            missing += count - Give(player, entry, count);

        ChatHandler chat(player->GetSession());
        if (missing)
            chat.PSendSysMessage("Your bags are full: {} dungeon potion(s) didn't fit.", missing);

        if (fresh && config.announce)
            chat.SendSysMessage("You receive dungeon supplies. They disappear when you leave the dungeon.");
    }

    // Called after every map change, login included (the player is added to their map on login).
    void OnEnterMap(Player* player)
    {
        Map* map = player->GetMap();
        if (!map)
            return;

        bool const inRun = IsRunMap(map);
        uint32 const mapId = map->GetId();
        uint32 const instanceId = map->GetInstanceId();

        std::lock_guard<std::mutex> guard(runsLock);
        Run& run = runs[player->GetGUID().GetCounter()];

        bool const sameRun = inRun && run.mapId == mapId && run.instanceId == instanceId;

        // Logged back in inside the run they already have supplies for.
        if (sameRun && run.holding)
            return;

        // Done even while the module is disabled, so turning it off can't strand potions in bags.
        // Also cleans up after a crash or restart, when the potions outlive the record of them.
        bool const wasHolding = run.holding;
        ItemCounts taken = TakeAll(player);
        if (wasHolding)
        {
            run.leftovers = std::move(taken);
            taken.clear();
            run.holding = false;
        }

        if (!inRun || !config.enabled)
            return;

        if (sameRun)
        {
            // Back after a corpse run or a trip out: hand back what they left with.
            GiveAll(player, run.leftovers, false);
        }
        else if (!taken.empty())
        {
            // Potions but no record of the run: the server restarted while they were inside.
            // Give back what they had instead of a fresh bundle.
            GiveAll(player, taken, false);
        }
        else
        {
            GiveAll(player, BuildBundle(player), true);
        }

        run.mapId = mapId;
        run.instanceId = instanceId;
        run.holding = true;
        run.leftovers.clear();
    }

    void CheckItemTemplates()
    {
        uint32 missing = 0;
        for (Tier const& tier : Tiers)
        {
            if (sObjectMgr->GetItemTemplate(tier.entry))
                continue;

            LOG_ERROR("module", "mod-dungeon-consumables: item {} (copy of {}) is missing from item_template. "
                "Apply the module's SQL.", tier.entry, tier.source);
            ++missing;
        }

        if (!missing)
            LOG_INFO("module", "mod-dungeon-consumables: {} dungeon potions loaded.", std::size(Tiers));
    }
}

class DungeonConsumablesWorldScript : public WorldScript
{
public:
    DungeonConsumablesWorldScript() : WorldScript("DungeonConsumablesWorldScript") { }

    void OnAfterConfigLoad(bool /*reload*/) override
    {
        config.enabled                      = sConfigMgr->GetOption<bool>("DungeonConsumables.Enable", true);
        config.healingPotions               = sConfigMgr->GetOption<uint32>("DungeonConsumables.HealingPotions", 5);
        config.manaPotions                  = sConfigMgr->GetOption<uint32>("DungeonConsumables.ManaPotions", 5);
        config.includeRaids                 = sConfigMgr->GetOption<bool>("DungeonConsumables.IncludeRaids", false);
        config.respectIndividualProgression = sConfigMgr->GetOption<bool>("DungeonConsumables.RespectIndividualProgression", true);
        config.announce                     = sConfigMgr->GetOption<bool>("DungeonConsumables.Announce", true);

        config.ipEnabled = sConfigMgr->GetOption<bool>("IndividualProgression.Enable", false, false);
    }

    // Items are loaded by now.
    void OnStartup() override
    {
        CheckItemTemplates();
    }
};

class DungeonConsumablesPlayerScript : public PlayerScript
{
public:
    DungeonConsumablesPlayerScript() : PlayerScript("DungeonConsumablesPlayerScript") { }

    void OnPlayerMapChanged(Player* player) override
    {
        if (!IsRealPlayer(player))
            return;

        OnEnterMap(player);
    }
};

void AddDungeonConsumablesScripts()
{
    new DungeonConsumablesWorldScript();
    new DungeonConsumablesPlayerScript();
}
