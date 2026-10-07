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

Delver's Draught is its own item (entry 32967), and the module only ever removes that entry.
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

### Why entry 32967

The 3.3.5 client takes a bag item's icon from its own Item.dbc, by entry, so a brand-new item
entry shows a question mark unless players install a client patch. Entry 32967 is "NPC Equip
32967", a placeholder Blizzard never gave players: nothing drops, sells, creates or rewards it,
and no NPC equips it. The client lists it as a consumable with the Summon Water Elemental icon,
which suits a potion that casts Gift of the Water Spirit. The module's SQL rewrites that row.

The row keeps subclass Potion, even though the client's Item.dbc says Other. A potion drunk in
combat starts its cooldown only when combat ends, and the server sends that signal to the client
only for items of subclass Potion. As Other, every potion got stuck waiting for a signal that never
came, until the player zoned.

Things to know, all from the spell's own data, which the client enforces too:

- Druids have to leave bear, cat or other forms to drink it.
- Drinking it triggers the 1.5-second global cooldown, which normal potions don't.
- It's a Magic buff, so an enemy that dispels can remove it.
- The amount is fixed at 50% over 10 seconds. Changing it would need a client patch to keep the
  tooltip right.

## Requirements

- [AzerothCore](https://www.azerothcore.org/) wotlk (master) and a WoW 3.3.5a (12340) client.
- No client patch. The potion reuses item entry 32967, which the 3.3.5 client already lists as a
  consumable with its own icon.
- [mod-playerbots](https://github.com/mod-playerbots/mod-playerbots) is optional. Bots are detected
  with `WorldSession::IsHeadless()` (or `IsBot()` on older playerbots cores) and skipped.

## Installation

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

## Troubleshooting

- **The worldserver logs an error about the potion at startup:** the module's SQL in
  `data/sql/db-world/updates` hasn't been applied, so item 32967 is still the placeholder. Start
  the worldserver again and let the updater run.
- **Some potions never arrived:** your bags were full on entry. You're told how many didn't fit,
  and they aren't given later.
- **A druid can't drink it:** the spell can't be cast in Cat, Bear or other forms. Leave the form first.
- **The module doesn't build or load:** the folder must be named `mod-dungeon-consumables`.

## Credits

Author: [buildthehomelab](https://github.com/buildthehomelab)

## License

MIT, see [LICENSE](LICENSE).
