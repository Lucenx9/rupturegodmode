# Rupture God Mode specification

## Goal

Provide a StarRupture ModLoader client plugin that keeps the local player's survival resources protected while God Mode is enabled.

## Required behavior

- God Mode starts enabled when the plugin loads. A manual toggle persists across world travel until the game process exits.
- The player can toggle it without opening a custom menu; the default key is `F8` and is configurable through ModLoader.
- Health, energy, shield, oxygen, hydration, calories, med-tool charge, and grenade charge are maintained at their maximum values.
- Toxicity, radiation, heat, drain, corrosion, and infection are maintained at their minimum values.
- Temperature is frozen at the safe value captured for the current pawn.
- StarRupture's native immortality mode prevents lethal damage paths, including falls and solar storms.
- The game's native unlimited/restricted cheats are enabled for health, survival vitals, ammunition, weapon heat, and hostile environmental values.
- Native immortality is toggled exactly once per controller on enable and disable, so respawning cannot accidentally turn it off.
- Disabling the mode removes all native cheats installed by this plugin and stops maintaining attributes.
- A new pawn after respawn receives God Mode automatically.
- In standalone mode, the local player receives full authoritative protection.
- On a listen server, the host applies authoritative protection to every connected player by default.
- On a remote multiplayer client, the plugin maintains the local predicted attributes. Installing the plugin on the host provides authoritative protection as well.
- Plugin shutdown and world teardown remove native effects when a local player still exists.

## Configuration

```ini
[General]
Enabled=1

[GodMode]
EnabledAtStart=1
ToggleKey=F8

[Multiplayer]
ProtectAllPlayersWhenHosting=1
```

## Compatibility

- ModLoader plugin interface: 60
- StarRupture Game SDK: commit `2c7d138506e42011d1ccf4ee044339c154b45b90` (game build CL121391)
