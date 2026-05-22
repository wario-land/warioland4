// Title screen data version configuration
//
// Controls which set of tilemap/logo data is compiled into the title screens.
//
// Build options (set via Makefile DATA_VERSION=... or define in CFLAGS):
//
//   DATA_VERSION=ida (default)
//     Uses data from the standard build.
//     Contains tilemaps with English logo text.
//
//   DATA_VERSION=alt
//     Uses alternate title screen tile layouts.
//     The "WARIO LAND 4" logo has different tile positions and values.

#ifndef TITLE_CONFIG_H
#define TITLE_CONFIG_H

#if !defined(DATA_VERSION_IDA) && !defined(DATA_VERSION_ALT)
#define DATA_VERSION_IDA
#endif

#endif // TITLE_CONFIG_H
