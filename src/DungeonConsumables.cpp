/*
 * mod-dungeon-consumables
 *
 * Real players get a few Delver's Draughts in their bags when they enter a dungeon,
 * and lose whatever is left when they leave. Each one restores 50% of maximum health and mana
 * over 10 seconds, so the same potion is as useful at level 15 as at 80. The percentages come
 * from the potion's spell, Gift of the Water Spirit; see the SQL.
 *
 * The potion is its own item, so leaving never touches potions the player brought along.
 *
 * Leaving and coming back to the same run (a corpse run, a trip out to repair) gives back what
 * the player had when they left rather than a fresh bundle, so zoning out can't be used to refill.
 * A different run, or the same dungeon after a reset, gets a fresh bundle.
 *
 * Released under GNU GPL v2 or (at your option) any later version.
 */

#include "Chat.h"
#include "Config.h"
#include "Log.h"
#include "Map.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "WorldSession.h"

#include <mutex>
#include <type_traits>
#include <unordered_map>
#include <utility>

namespace
{
    // Must match the SQL. An unused Blizzard placeholder ("NPC Equip 32967") rather than a new
    // entry, because the client only shows an icon for entries in its own Item.dbc.
    constexpr uint32 ITEM_DELVERS_DRAUGHT = 32967;

    struct Config
    {
        bool enabled = true;
        uint32 potions = 5;
        bool includeRaids = false;
        bool announce = true;
    };

    Config config;

    // The run a player last had potions for.
    struct Run
    {
        uint32 mapId = 0;
        uint32 instanceId = 0;
        bool holding = false;    // the potions are in their bags right now
        uint32 leftovers = 0;    // how many they had when they last left this run
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

    // Removes every Delver's Draught the player has, bank included, and returns how many.
    uint32 TakeAll(Player* player)
    {
        uint32 count = player->GetItemCount(ITEM_DELVERS_DRAUGHT, true);
        if (count)
            player->DestroyItemCount(ITEM_DELVERS_DRAUGHT, count, true);

        return count;
    }

    // Same as Player::AddItem, but tells the player when their bags are full.
    void Give(Player* player, uint32 count, bool fresh)
    {
        if (!count)
            return;

        uint32 noSpaceForCount = 0;
        ItemPosCountVec dest;
        InventoryResult msg = player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, ITEM_DELVERS_DRAUGHT,
            count, &noSpaceForCount);
        uint32 given = msg == EQUIP_ERR_OK ? count : count - noSpaceForCount;

        if (given && !dest.empty())
            if (Item* item = player->StoreNewItem(dest, ITEM_DELVERS_DRAUGHT, true))
                player->SendNewItem(item, given, true, false);

        ChatHandler chat(player->GetSession());
        if (given < count)
            chat.PSendSysMessage("Your bags are full: {} Delver's Draught(s) didn't fit.", count - given);

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

        // Logged back in inside the run they already have potions for.
        if (sameRun && run.holding)
            return;

        // Done even while the module is disabled, so turning it off can't strand potions in bags.
        // Also cleans up after a crash or restart, when the potions outlive the record of them.
        bool const wasHolding = run.holding;
        uint32 taken = TakeAll(player);
        if (wasHolding)
        {
            run.leftovers = taken;
            taken = 0;
            run.holding = false;
        }

        if (!inRun || !config.enabled)
            return;

        if (sameRun)
            Give(player, run.leftovers, false); // back after a corpse run or a trip out
        else if (taken)
            Give(player, taken, false);         // potions but no record: the server restarted with them inside
        else
            Give(player, config.potions, true);

        run.mapId = mapId;
        run.instanceId = instanceId;
        run.holding = true;
        run.leftovers = 0;
    }
}

class DungeonConsumablesWorldScript : public WorldScript
{
public:
    DungeonConsumablesWorldScript() : WorldScript("DungeonConsumablesWorldScript") { }

    void OnAfterConfigLoad(bool /*reload*/) override
    {
        config.enabled      = sConfigMgr->GetOption<bool>("DungeonConsumables.Enable", true);
        config.potions      = sConfigMgr->GetOption<uint32>("DungeonConsumables.Potions", 5);
        config.includeRaids = sConfigMgr->GetOption<bool>("DungeonConsumables.IncludeRaids", false);
        config.announce     = sConfigMgr->GetOption<bool>("DungeonConsumables.Announce", true);
    }

    // Items are loaded by now.
    void OnStartup() override
    {
        if (!sObjectMgr->GetItemTemplate(ITEM_DELVERS_DRAUGHT))
            LOG_ERROR("module", "mod-dungeon-consumables: item {} (Delver's Draught) is missing from "
                "item_template. Apply the module's SQL.", ITEM_DELVERS_DRAUGHT);
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
