-- mod-dungeon-consumables: the Dungeon Rejuvenation Potion.
--
-- A copy of the Minor Rejuvenation Potion (2456) with its own entry, so the module can find and
-- remove exactly these when a player leaves a dungeon without touching potions the player bought
-- or crafted. It keeps the original's icon and its potion cooldown (category 4, 1 minute, shared
-- with every other potion).
--
-- Its spell is swapped for Gift of the Water Spirit (30874): 5% of maximum health and mana every
-- second for 10 seconds. Blizzard only ever gave that spell to an NPC, and no item uses it, so
-- the percentages come straight from the game data with no script, the client's tooltip already
-- describes it correctly, and nothing players can get elsewhere is affected. The spell's own
-- 5-minute cooldown doesn't apply: the item's cooldown fields replace it.
--
-- It's Bind on Pickup, sells for nothing and has no level requirement.
--
-- The entry must match ITEM_DUNGEON_REJUVENATION_POTION in src/DungeonConsumables.cpp.
--
-- Idempotent: safe to run again. The range delete also clears the per-level potions (9500101 to
-- 9500115) from the module's first version.

SET @ENTRY := 9500100;

DELETE FROM `item_template` WHERE `entry` BETWEEN @ENTRY AND @ENTRY + 15;

DROP TEMPORARY TABLE IF EXISTS `tmp_dungeon_rejuvenation_potion`;
CREATE TEMPORARY TABLE `tmp_dungeon_rejuvenation_potion` LIKE `item_template`;

INSERT INTO `tmp_dungeon_rejuvenation_potion` SELECT * FROM `item_template` WHERE `entry` = 2456;

UPDATE `tmp_dungeon_rejuvenation_potion`
SET `entry`                   = @ENTRY,
    `name`                    = 'Dungeon Rejuvenation Potion',
    `description`             = 'Disappears when you leave the dungeon.',
    `RequiredLevel`           = 0,
    `ItemLevel`               = 1,
    `bonding`                 = 1, -- Bind on Pickup
    `BuyPrice`                = 0,
    `SellPrice`               = 0,
    `spellid_1`               = 30874, -- Gift of the Water Spirit
    `spelltrigger_1`          = 0,     -- on use
    `spellcharges_1`          = -1,    -- used up
    `spellcooldown_1`         = 0,
    `spellcategory_1`         = 4,     -- potions
    `spellcategorycooldown_1` = 60000,
    `VerifiedBuild`           = 0;

INSERT INTO `item_template` SELECT * FROM `tmp_dungeon_rejuvenation_potion`;

DROP TEMPORARY TABLE `tmp_dungeon_rejuvenation_potion`;

-- An earlier version bound a script to Minor Rejuvenation Potion's spell; it's no longer used.
DELETE FROM `spell_script_names` WHERE `ScriptName` = 'spell_dungeon_rejuvenation_potion';
