-- mod-dungeon-consumables: Delver's Draught, the dungeon potion.
--
-- Entry 32967 is "NPC Equip 32967", a placeholder Blizzard never gave players: nothing drops,
-- sells, creates or rewards it, and no NPC equips it. It's reused rather than a new entry because
-- the 3.3.5 client takes a bag item's icon from its own Item.dbc, by entry. A brand-new entry isn't
-- in Item.dbc and shows a question mark; 32967 is, as a consumable with the Summon Water Elemental
-- icon, which suits a potion that casts Gift of the Water Spirit. The display, subclass and
-- material below match the client's Item.dbc row so every part of the UI agrees.
--
-- The rest is copied from the Minor Rejuvenation Potion (2456), including its potion cooldown
-- (category 4, 1 minute, shared with every other potion).
--
-- Its spell is Gift of the Water Spirit (30874): 5% of maximum health and mana every second for
-- 10 seconds. Blizzard only ever gave that spell to an NPC, and no item uses it, so the
-- percentages come straight from the game data with no script, the client's tooltip already
-- describes it correctly, and nothing players can get elsewhere is affected. The spell's own
-- 5-minute cooldown doesn't apply: the item's cooldown fields replace it.
--
-- It's Bind on Pickup, sells for nothing and has no level requirement.
--
-- The entry must match ITEM_DELVERS_DRAUGHT in src/DungeonConsumables.cpp.
--
-- Idempotent: safe to run again. Also removes what earlier versions of the module created: the
-- custom entries 9500100 to 9500115 (including the question-mark Delver's Draught from
-- mod_dungeon_consumables_2026_09_25_00.sql) and a spell script binding. Players still holding
-- that old one lose it at their next login, when the core deletes items whose template is gone.

SET @ENTRY := 32967;

DELETE FROM `item_template` WHERE `entry` BETWEEN 9500100 AND 9500115;
DELETE FROM `spell_script_names` WHERE `ScriptName` = 'spell_dungeon_rejuvenation_potion';

DROP TEMPORARY TABLE IF EXISTS `tmp_delvers_draught`;
CREATE TEMPORARY TABLE `tmp_delvers_draught` LIKE `item_template`;

INSERT INTO `tmp_delvers_draught` SELECT * FROM `item_template` WHERE `entry` = 2456;

UPDATE `tmp_delvers_draught`
SET `entry`                   = @ENTRY,
    `subclass`                = 8,     -- Other, as in the client's Item.dbc
    `SoundOverrideSubclass`   = 0,
    `displayid`               = 45809, -- Summon Water Elemental icon, as in the client's Item.dbc
    `Material`                = 0,
    `name`                    = 'Delver''s Draught',
    `description`             = 'Disappears when you leave the dungeon.',
    `RequiredLevel`           = 0,
    `ItemLevel`               = 1,
    `bonding`                 = 1,     -- Bind on Pickup
    `BuyPrice`                = 0,
    `SellPrice`               = 0,
    `spellid_1`               = 30874, -- Gift of the Water Spirit
    `spelltrigger_1`          = 0,     -- on use
    `spellcharges_1`          = -1,    -- used up
    `spellcooldown_1`         = 0,
    `spellcategory_1`         = 4,     -- potions
    `spellcategorycooldown_1` = 60000,
    `VerifiedBuild`           = 0;

DELETE FROM `item_template` WHERE `entry` = @ENTRY;
INSERT INTO `item_template` SELECT * FROM `tmp_delvers_draught`;

DROP TEMPORARY TABLE `tmp_delvers_draught`;
