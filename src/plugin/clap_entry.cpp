// CLAP Entry Point - Phase 0 Minimal Implementation
// Full implementation in Phase 2

#include <clap/clap.h>
#include <algorithm>
#include <cstring>

//------------------------------------------------------------------------------
// Plugin Descriptor
//------------------------------------------------------------------------------

static const char* s_features[] = {
    CLAP_PLUGIN_FEATURE_AUDIO_EFFECT,
    CLAP_PLUGIN_FEATURE_UTILITY,
    nullptr
};

static const clap_plugin_descriptor_t s_descriptor = {
    .clap_version = CLAP_VERSION,
    .id = "com.ninjam.clap-client",
    .name = "NINJAM",
    .vendor = "NINJAM",
    .url = "https://ninjam.com",
    .manual_url = "",
    .support_url = "",
    .version = "1.0.0",
    .description = "NINJAM online music collaboration",
    .features = s_features
};

//------------------------------------------------------------------------------
// Plugin Lifecycle (Minimal Stubs)
//------------------------------------------------------------------------------

static bool plugin_init(const clap_plugin_t* plugin) {
    return true;
}

static void plugin_destroy(const clap_plugin_t* plugin) {
    delete const_cast<clap_plugin_t*>(plugin);
}

static bool plugin_activate(const clap_plugin_t* plugin,
                            double sample_rate,
                            uint32_t min_frames,
                            uint32_t max_frames) {
    return true;
}

static void plugin_deactivate(const clap_plugin_t* plugin) {
}

static bool plugin_start_processing(const clap_plugin_t* plugin) {
    return true;
}

static void plugin_stop_processing(const clap_plugin_t* plugin) {
}

static void plugin_reset(const clap_plugin_t* plugin) {
}

static clap_process_status plugin_process(const clap_plugin_t* plugin,
                                          const clap_process_t* process) {
    // Pass-through for Phase 0
    if (process->audio_inputs_count == 0 || process->audio_outputs_count == 0)
        return CLAP_PROCESS_CONTINUE;

    const clap_audio_buffer_t& input = process->audio_inputs[0];
    const clap_audio_buffer_t& output = process->audio_outputs[0];
    if (!input.data32 || !output.data32)
        return CLAP_PROCESS_CONTINUE;

    const uint32_t frames = process->frames_count;
    const uint32_t channels = std::min(input.channel_count, output.channel_count);

    for (uint32_t c = 0; c < channels && c < 2; ++c) {
        if (input.data32[c] && output.data32[c]) {
            memcpy(output.data32[c],
                   input.data32[c],
                   frames * sizeof(float));
        }
    }
    return CLAP_PROCESS_CONTINUE;
}

static const void* plugin_get_extension(const clap_plugin_t* plugin,
                                        const char* id) {
    // No extensions in Phase 0
    return nullptr;
}

static void plugin_on_main_thread(const clap_plugin_t* plugin) {
}

//------------------------------------------------------------------------------
// Plugin Instance Template
//------------------------------------------------------------------------------

static const clap_plugin_t s_plugin_template = {
    .desc = &s_descriptor,
    .plugin_data = nullptr,
    .init = plugin_init,
    .destroy = plugin_destroy,
    .activate = plugin_activate,
    .deactivate = plugin_deactivate,
    .start_processing = plugin_start_processing,
    .stop_processing = plugin_stop_processing,
    .reset = plugin_reset,
    .process = plugin_process,
    .get_extension = plugin_get_extension,
    .on_main_thread = plugin_on_main_thread
};

//------------------------------------------------------------------------------
// Factory
//------------------------------------------------------------------------------

static uint32_t factory_get_plugin_count(const clap_plugin_factory_t* factory) {
    return 1;
}

static const clap_plugin_descriptor_t* factory_get_plugin_descriptor(
    const clap_plugin_factory_t* factory, uint32_t index) {
    return index == 0 ? &s_descriptor : nullptr;
}

static const clap_plugin_t* factory_create_plugin(
    const clap_plugin_factory_t* factory,
    const clap_host_t* host,
    const char* plugin_id) {

    if (!clap_version_is_compatible(host->clap_version))
        return nullptr;

    if (strcmp(plugin_id, s_descriptor.id) != 0)
        return nullptr;

    // Allocate plugin instance
    clap_plugin_t* plugin = new clap_plugin_t(s_plugin_template);
    plugin->plugin_data = nullptr;  // Will hold NinjamPlugin* in Phase 2

    return plugin;
}

static const clap_plugin_factory_t s_factory = {
    .get_plugin_count = factory_get_plugin_count,
    .get_plugin_descriptor = factory_get_plugin_descriptor,
    .create_plugin = factory_create_plugin
};

//------------------------------------------------------------------------------
// Entry Point
//------------------------------------------------------------------------------

static bool entry_init(const char* path) {
    return true;
}

static void entry_deinit(void) {
}

static const void* entry_get_factory(const char* factory_id) {
    if (strcmp(factory_id, CLAP_PLUGIN_FACTORY_ID) == 0)
        return &s_factory;
    return nullptr;
}

extern "C" CLAP_EXPORT const clap_plugin_entry_t clap_entry = {
    .clap_version = CLAP_VERSION,
    .init = entry_init,
    .deinit = entry_deinit,
    .get_factory = entry_get_factory
};
