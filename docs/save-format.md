# Save file format (Marvel's Avengers PC, Steam 2.8.2)

Applies to `save1_N.dat`, `savebk_N.dat` and `profile1.dat` in `Documents\Marvel's Avengers\<SteamID64>\`.

## File
```
file  = RC4( zlib(plain) ) || CRC64( zlib(plain) )      CRC stored as 8 bytes, little-endian
key   = first 16 bytes of SHA-1( 25 E2 E3 53 51 54 76 9D || SteamID64 as 8 bytes little-endian )
CRC64 = ECMA-182 polynomial 0x42F0E1EBA9EA3693 table; update c = T[(c ^ b) & 0xFF] ^ (c >> 8);
        init 0; result inverted
```
The SteamID64 is the save folder's name, so a save only opens under the account that made it. Re-encrypting
the same plain data with another SteamID moves a save to another account (Save Tool "Import").

## Plain data
- Saves are a 16 MB (0x1000000) memory pool; `profile1.dat` is 0x7000 bytes.
- `0x1AF498`: u32 bytes used by the block area, which starts at `0x1AF49C`.
- Each block: `+8` u32 = type (low byte) | size in dwords (bits 8..31), i.e. size in bytes =
  `(u32 >> 6) & 0x3FFFFFC`. Blocks are packed one after another.
- Block type 4 holds `{u32 key, u32 value}` pairs on 8-byte steps. The pair with key **0x7C766BB1** is a
  story stage: `0x49481C6F` at the start of a new game, `0x06B870E3` with the war table and all heroes
  unlocked. Changing only that value unlocks both (tested in game).
