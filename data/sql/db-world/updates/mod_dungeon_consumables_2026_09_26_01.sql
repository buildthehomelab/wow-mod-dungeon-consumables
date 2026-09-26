-- mod-dungeon-consumables: Delver's Draught has to be a Potion for its cooldown to start.
--
-- mod_dungeon_consumables_2026_09_26_00.sql set its subclass to 8 (Other) to match the client's
-- Item.dbc. That broke the potion cooldown. Drinking a potion in combat leaves the whole potion
-- category (4) waiting on the client until the server says combat is over, and the server only
-- sends that for items it thinks are potions: consumables of subclass 1 (Potion). As Other, the
-- server started the cooldown straight away and never sent the end-of-combat signal, so every
-- potion in the player's bags stayed stuck waiting until they zoned.
--
-- The icon comes from Item.dbc's display, not its subclass, so the icon is unaffected.
--
-- Idempotent: safe to run again.

UPDATE `item_template` SET `subclass` = 1 WHERE `entry` = 32967; -- Potion
