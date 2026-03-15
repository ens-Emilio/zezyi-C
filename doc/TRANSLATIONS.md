# Translations (i18n)

The UI translation system is a small table-driven layer:

- Keys: `include/i18n.h`
- Strings: `src/i18n.c`

Supported languages:

- English (`en`)
- Portuguese (Brazil) (`ptbr`)
- Spanish (`es`)

## Adding a language

1. Add a new entry in the `Language` enum (`include/config.h`).
2. Add a new column to the translation tables in `src/i18n.c`.
3. Update config parsing/serialization (`src/gameconfig.c`) to accept/save the new code.
4. Add a label in `i18n_language_label()`.

Note: the built-in bitmap font is optimized for uppercase ASCII, so avoid accented characters in UI strings unless the font is expanded.

