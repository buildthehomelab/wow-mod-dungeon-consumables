# Dungeon Consumables

An [AzerothCore](https://www.azerothcore.org/) (WotLK 3.3.5a) module that hands players a few
potions when they walk into a dungeon, to make runs a little easier.

- **On entry**, real players get 5 **Delver's Draughts**. Each one restores 50% of
  maximum health and mana over 10 seconds. Because the amounts are percentages, the same potion
  works at level 15 and at 80. Classes without mana just get the heal.
- **On leaving**, whatever is left disappears.
- **Coming back to the same run** (a corpse run, a trip out to repair) gives back what you had
  when you left, not a fresh bundle, so zoning out doesn't refill you. A different dungeon, or
  the same one after a reset, gets a fresh bundle.
- **Playerbots are skipped**, because they already handle their own potions.

The number of potions is configurable.

## Your own potions are safe

Delver's Draught is its own item (entry 9500100), and the module only ever removes that entry.
Potions you bought or crafted are never touched and don't stack with it. It shares the normal
potion cooldown.

It's Bind on Pickup and sells for nothing, so it can't be traded, mailed or vendored.

## How it works

Delver's Draught casts **Gift of the Water Spirit** (spell 30874): 5% of maximum health and mana
every second for 10 seconds. Blizzard only ever gave that spell to an NPC, and no item uses it.
So:

- **No server script is needed.** The percentages are Blizzard's own spell data.
- **The tooltip is correct.** The client shows "Use: Regenerates 50% of your total health and mana
  over 10 sec."
- **Nothing players can get elsewhere is affected**, because no other item or ability casts it.

It uses the normal 1-minute potion cooldown, shared with all other potions. The spell's own
5-minute cooldown doesn't apply, because the item's cooldown fields replace it.

Things to know, all from the spell's own data, which the client enforces too:

- Druids have to leave bear, cat or other forms to drink it.
- Drinking it triggers the 1.5-second global cooldown, which normal potions don't.
- It's a Magic buff, so an enemy that dispels can remove it.
- The amount is fixed at 50% over 10 seconds. Changing it would need a client patch to keep the
  tooltip right.

## Install

```bash
cd azerothcore-wotlk/modules
git clone https://github.com/buildthehomelab/wow-mod-dungeon-consumables.git mod-dungeon-consumables
```

Clone into `mod-dungeon-consumables` exactly, because AzerothCore derives the loader symbol from
the folder name. Then re-run CMake, rebuild, and copy `conf/mod_dungeon_consumables.conf.dist`
to your config folder as `mod_dungeon_consumables.conf`.

The SQL in `data/sql/db-world/updates` creates the potion, and the worldserver's updater applies
it. If the item is missing at startup, the worldserver logs an error saying so.

## Config

See `conf/mod_dungeon_consumables.conf.dist`: potion count, raids on or off, and the chat
notice.

## Notes

- The module remembers runs in memory. After a worldserver restart, a player still inside a
  dungeon keeps the potions they had.
- If your bags are full on entry, you're told how many potions didn't fit. They aren't given
  later.
