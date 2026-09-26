-- mod-dungeon-consumables: the dungeon-only potions.
--
-- Each one is a copy of a normal healing or mana potion (same spell, same icon, same shared
-- potion cooldown) with its own entry, so the module can find and remove exactly these when a
-- player leaves the dungeon without touching potions the player bought or crafted. The copies
-- are Bind on Pickup and can't be sold, so they can't be traded or vendored for gold either.
--
-- The entries must match ENTRY_BASE and the Tiers table in src/DungeonConsumables.cpp.
--
-- Copied from the live rows rather than written out, so they pick up whatever the server
-- already has for the originals (mod-individual-progression edits some item_template rows).
--
-- Idempotent: safe to run again.

SET @BASE := 9500100;

DELETE FROM `item_template` WHERE `entry` BETWEEN @BASE AND @BASE + 15;

DROP TEMPORARY TABLE IF EXISTS `tmp_dungeon_consumable_map`;
CREATE TEMPORARY TABLE `tmp_dungeon_consumable_map` (
    `source` INT UNSIGNED NOT NULL PRIMARY KEY,
    `entry`  INT UNSIGNED NOT NULL
);

INSERT INTO `tmp_dungeon_consumable_map` (`source`, `entry`) VALUES
(118,   @BASE + 0),  -- Minor Healing Potion      (level 1)
(858,   @BASE + 1),  -- Lesser Healing Potion     (level 3)
(929,   @BASE + 2),  -- Healing Potion            (level 12)
(1710,  @BASE + 3),  -- Greater Healing Potion    (level 21)
(3928,  @BASE + 4),  -- Superior Healing Potion   (level 35)
(13446, @BASE + 5),  -- Major Healing Potion      (level 45)
(22829, @BASE + 6),  -- Super Healing Potion      (level 55)
(33447, @BASE + 7),  -- Runic Healing Potion      (level 70)
(2455,  @BASE + 8),  -- Minor Mana Potion         (level 5)
(3385,  @BASE + 9),  -- Lesser Mana Potion        (level 14)
(3827,  @BASE + 10), -- Mana Potion               (level 22)
(6149,  @BASE + 11), -- Greater Mana Potion       (level 31)
(13443, @BASE + 12), -- Superior Mana Potion      (level 41)
(13444, @BASE + 13), -- Major Mana Potion         (level 49)
(22832, @BASE + 14), -- Super Mana Potion         (level 55)
(33448, @BASE + 15); -- Runic Mana Potion         (level 70)

DROP TEMPORARY TABLE IF EXISTS `tmp_dungeon_consumable_items`;
CREATE TEMPORARY TABLE `tmp_dungeon_consumable_items` LIKE `item_template`;

INSERT INTO `tmp_dungeon_consumable_items`
SELECT `it`.* FROM `item_template` `it`
JOIN `tmp_dungeon_consumable_map` `m` ON `m`.`source` = `it`.`entry`;

UPDATE `tmp_dungeon_consumable_items` `t`
SET `t`.`entry` = (SELECT `m`.`entry` FROM `tmp_dungeon_consumable_map` `m` WHERE `m`.`source` = `t`.`entry`);

UPDATE `tmp_dungeon_consumable_items`
SET `name`          = CONCAT('Dungeon ', `name`),
    `description`   = 'Dungeon supplies. Disappears when you leave the dungeon.',
    `bonding`       = 1, -- Bind on Pickup
    `BuyPrice`      = 0,
    `SellPrice`     = 0,
    `VerifiedBuild` = 0;

INSERT INTO `item_template` SELECT * FROM `tmp_dungeon_consumable_items`;

DROP TEMPORARY TABLE `tmp_dungeon_consumable_items`;
DROP TEMPORARY TABLE `tmp_dungeon_consumable_map`;
