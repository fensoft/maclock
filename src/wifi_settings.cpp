#include "wifi_mode_internal.h"

// Settings storage remains serialized through the service state lock. The
// implementation is textually retained by the Wi-Fi coordinator during this
// compatibility-preserving split.
