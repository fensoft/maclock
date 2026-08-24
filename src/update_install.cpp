#ifdef MACLOCK_UPDATE_COMBINED_SOURCE
#ifndef MACLOCK_LOCAL
#define MACLOCK_UPDATE_INSTALL_FRAGMENT
#include "update_manifest.cpp"
#include "update_assets.cpp"
#include "update_ota.cpp"
#undef MACLOCK_UPDATE_INSTALL_FRAGMENT
#endif
#endif
