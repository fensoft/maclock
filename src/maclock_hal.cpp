#include "maclock_hal.h"

#include <stdlib.h>

namespace
{
MaclockHal *active_hal = nullptr;
}

void maclock_install_hal(MaclockHal &hal)
{
    active_hal = &hal;
}

MaclockHal &maclock_hal()
{
    if (!active_hal)
        abort();
    return *active_hal;
}

bool maclock_hal_installed()
{
    return active_hal != nullptr;
}
