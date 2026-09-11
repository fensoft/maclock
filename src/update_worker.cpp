#ifdef MACLOCK_UPDATE_COMBINED_SOURCE
void update_worker(void *context)
{
    auto *state =
        static_cast<UpdateService::State *>(context);
    WorkerAction action = WorkerAction::None;
    portENTER_CRITICAL(&state->mux);
    action = state->requested_action;
    state->requested_action = WorkerAction::None;
    portEXIT_CRITICAL(&state->mux);

    if (action == WorkerAction::Check)
        perform_check(*state);
    else if (action == WorkerAction::Install)
    {
#ifdef MACLOCK_LOCAL
        set_error(
            *state,
            "Firmware installation is unavailable in the simulator");
#else
        perform_install(*state);
#endif
    }
    else if (action == WorkerAction::RefreshAssets)
    {
#ifdef MACLOCK_LOCAL
        set_error(
            *state,
            "Asset refresh is unavailable in the simulator");
#else
        perform_asset_refresh(*state);
#endif
    }

    portENTER_CRITICAL(&state->mux);
    state->worker = nullptr;
    portEXIT_CRITICAL(&state->mux);
    vTaskDelete(nullptr);
}

bool start_worker(
    UpdateService::State &state, WorkerAction action)
{
    static constexpr uint32_t kWorkerStackSize =
        16U * 1024U;

    portENTER_CRITICAL(&state.mux);
    if (state.worker || state.snapshot.busy)
    {
        portEXIT_CRITICAL(&state.mux);
        return false;
    }
    state.requested_action = action;
    state.worker = nullptr;
    const BaseType_t created = xTaskCreatePinnedToCore(
        update_worker, "MaclockUpdate", kWorkerStackSize,
        &state, 1, &state.worker, 0);
    portEXIT_CRITICAL(&state.mux);

    if (created != pdPASS)
    {
        set_error(
            state,
            "Not enough internal memory to check for updates");
    }
    return created == pdPASS;
}
} // namespace
#endif
