The dashboard fonts are generated from the original pixel patterns in
`scripts/generate_fonts.py`. They do not contain the Inconsolata font used by
an earlier prototype. Sizes use 1×, 2×, and 3× integer scaling of the same glyphs.

Run `python3 scripts/generate_fonts.py` from the project root to regenerate
`dashboard_fonts.h`. No external font package is needed.
