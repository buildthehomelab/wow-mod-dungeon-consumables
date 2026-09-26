# Dungeon Consumables

An [AzerothCore](https://www.azerothcore.org/) (WotLK 3.3.5a) module that hands players a few
potions when they walk into a dungeon, to make runs a little easier.

- **On entry**, real players get healing potions matched to their level, and mana potions if
  their class uses mana (5 of each by default).
- **On leaving**, whatever is left disappears.
- **Coming back to the same run** (a corpse run, a trip out to repair) gives back what you had
  when you left, not a fresh bundle, so zoning out doesn't refill you. A different dungeon, or
  the same one after a reset, gets a fresh bundle.
- **Playerbots are skipped**, because they already handle their own potions.

## Your own potions are safe

The handouts are separate items: "Dungeon Healing Potion", "Dungeon Super Mana Potion" and so on,
entries 9500100 to 9500115. The module only ever removes those entries, so potions you bought or
crafted are never touched and don't stack with them. Both kinds share the normal potion cooldown.

The dungeon potions are Bind on Pickup and sell for nothing, so they can't be traded, mailed or
vendored.

## Which potion you get

The best one your level allows:

| Level | Healing | Mana |
|------:|---------|------|
| 1 | Minor | |
| 3 | Lesser | |
| 5 | | Minor |
| 12 | Healing | |
| 14 | | Lesser |
| 21 | Greater | |
| 22 | | Mana |
| 31 | | Greater |
| 35 | Superior | |
| 41 | | Superior |
| 45 | Major | |
| 49 | | Major |
| 55 | Super | Super |
| 70 | Runic | Runic |

With [mod-individual-progression](https://github.com/ZhengPeiRu21/mod-individual-progression)
the potions also follow your era: Super needs Outland unlocked (state 8), and Runic needs
Northrend (state 13). Turn that off with `DungeonConsumables.RespectIndividualProgression = 0`.

## Install

```bash
cd azerothcore-wotlk/modules
git clone https://github.com/buildthehomelab/wow-mod-dungeon-consumables.git mod-dungeon-consumables
```

Clone into `mod-dungeon-consumables` exactly, because AzerothCore derives the loader symbol from
the folder name. Then re-run CMake, rebuild, and copy `conf/mod_dungeon_consumables.conf.dist`
to your config folder as `mod_dungeon_consumables.conf`.

The SQL in `data/sql/db-world/updates` creates the 16 dungeon potions and is applied by the
worldserver's updater. It copies the live rows of the real potions, so any changes your server
has made to them carry over. At startup the worldserver logs
`mod-dungeon-consumables: 16 dungeon potions loaded.`, or an error naming any item that's
missing.

## Config

See `conf/mod_dungeon_consumables.conf.dist`: potion counts, raids on or off,
individual-progression gating, and the chat notice.

## Notes

- The module remembers runs in memory. After a worldserver restart, a player still inside a
  dungeon keeps the potions they had.
- If your bags are full on entry, you're told how many potions didn't fit. They aren't given
  later.
