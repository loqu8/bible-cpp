# Data Sources & Licensing

## License Summary

| Component | License | Source |
|-----------|---------|--------|
| bible-cpp code | MIT | This repository |
| Strong's Hebrew Lexicon (TBESH) | CC BY 4.0 | STEPBible-Data, Tyndale House Cambridge |
| Strong's Greek Lexicon (TBESG) | CC BY 4.0 | STEPBible-Data, Tyndale House Cambridge |
| KJV Bible text | Public Domain | Published 1611 |
| CUV Bible text | Public Domain | Published 1919 |
| Strong's Concordance (original) | Public Domain | James Strong, 1890 |

## STEPBible-Data (CC BY 4.0)

Source: https://github.com/STEPBible/STEPBible-Data

The TBESG and TBESH lexicon files provide the Strong's concordance data used
by bible-cpp. These are maintained by Tyndale House, Cambridge and released
under Creative Commons Attribution 4.0 International.

### Attribution (required by CC BY 4.0)

When distributing bible-cpp with STEPBible data, include this attribution:

> Strong's lexicon data from "Tyndale House, Cambridge"
> (www.TyndaleHouse.com) and "STEP Bible" (www.STEPBible.org).
> Source: https://github.com/STEPBible/STEPBible-Data
> Licensed under CC BY 4.0.

### TBESG Format (Greek Lexicon)

Tab-separated, 8 columns:
1. **eStrong** — Extended Strong's number (e.g., G0026)
2. **dStrong** — Disambiguated Strong's (e.g., G0026 =)
3. **uStrong** — Unified Strong's (e.g., G0026)
4. **Greek** — Greek word (e.g., ἀγάπη)
5. **Transliteration** — (e.g., agape)
6. **Morph** — Morphological code (e.g., G:N-F)
7. **Gloss** — One-word English gloss (e.g., love)
8. **Meaning** — Full lexical definition

### TBESH Format (Hebrew Lexicon)

Same 8-column format with Hebrew instead of Greek.

### Three Numbering Systems

- **eStrong**: Standard numbers compatible with OpenScriptures/NASB. H0001-H8674 (Hebrew), G0001-G5624 (Greek). Extensions for words subdivided by BDB.
- **dStrong**: Disambiguated — unique per person/place. E.g., H2264A vs H2264B for different individuals named Herod.
- **uStrong**: Unified — single number per individual across all name variants.

For a standard concordance, **eStrong** suffices. Use dStrong when you need "which Herod?" disambiguation.

## Bible SuperSearch

Source: https://biblesupersearch.com

Bible SuperSearch provides KJV and CUV texts with inline Strong's markers
in SQLite format. The Bible text itself is public domain. The software is GPL,
but the data (verse text) is explicitly stated as public domain on their site.

### Strong's Marker Format

Bible SuperSearch uses `{H####}` and `{G####}` inline markers:

```
{H7225}In the beginning {H430}God {H1254}created {H853} {H8064}the heaven
{H853}and {H776}the earth
```

The marker appears before the word it tags. The parser in `strongs_parser.cpp`
handles this format.

## Public Domain Bible Texts

### KJV (King James Version)

Published 1611. In the public domain worldwide (except UK Crown Copyright,
which does not apply to the US or most other jurisdictions). The most
widely-used English Bible translation and the standard reference for
Strong's concordance numbering.

### CUV (Chinese Union Version / 和合本)

Published 1919 by the Bible Society. In the public domain (>100 years old).
Available in simplified (和合本简体) and traditional (和合本繁體) character
variants. The standard Chinese Bible translation used by most Protestant
Chinese churches.

## Future Data Sources

- **TAGNT/TAHOT** (Greek/Hebrew source texts) — CC BY 4.0 from STEPBible.
  Would enable displaying original language text alongside translations.
- **TIPNR** (Proper Names) — CC BY 4.0 from STEPBible.
  Every proper noun with disambiguated individuals and genealogies.
- **Cross-references** — OpenBible.info (~340K pairs, CC BY).
- **RCUV/NCV** — Modern Chinese translations (copyright, would need licensing).
