#pragma once

// Update-service fragments are textually combined to preserve private worker
// state, lock ownership, and OTA lifecycle ordering.
#define MACLOCK_UPDATE_INTERNAL 1
