# json

Minimal pull-style JSON reader for small documents (API responses). No DOM is
built; values are copied on demand into caller-provided buffers.

| | |
|---|---|
| Target / option | `pico_toolset_json` / `PICO_TOOLSET_BUILD_JSON` (ON) |
| Links | nothing beyond the C++ standard library |
| Example | `libs/json/example/json_example.cpp` |
| Presets | none (no board wiring) |
| Reference consumer | PicoADSB (adsb.lol `/v2/point` and `/route` responses) |

## API

`pico_toolset::JsonReader(const char* data, size_t len)`:

- `beginObject()` / `beginArray()` -- call at `{` / `[`.
- `nextObjectMember(key)` / `nextArrayElement()` -- loop conditions; they
  consume the closing `}` / `]` and return false at the end.
- `parseString(out)` / `parseNumber(out)` -- at a member value position. A JSON
  `null` is consumed and reported as "no value" (returns false) *without*
  invalidating the reader.
- `skipValue()` -- consumes the current value, nested containers included.
- `valid()` -- false once a structural error was seen.

## Constraints

- `\uXXXX` escapes are skipped, not decoded: fine for ASCII fields only.
- `parseNumber` reads at most 39 characters of a number token.
- A type mismatch (e.g. `parseNumber` on a string such as ADS-B's
  `"alt_baro":"ground"`) invalidates the reader: call `skipValue()` for fields
  that can hold either type.
- Strings and numbers are parsed with `std::string`/`strtod`, so it needs the
  heap; fine for the Pico W, not for a no-malloc setup.
