#pragma once

enum class LocalHalSelfTestMode
{
    Write,
    Verify
};

bool maclock_local_run_self_test(
    LocalHalSelfTestMode mode);
