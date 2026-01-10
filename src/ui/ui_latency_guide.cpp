/*
    NINJAM CLAP Plugin - ui_latency_guide.cpp
    Visual latency guide widget
*/

#include "ui_latency_guide.h"
#include "plugin/ninjam_plugin.h"
#include "imgui.h"
#include <cmath>

namespace {

void push_transient(UiState& state, float offset) {
    state.latency_history[state.latency_history_index] = offset;
    state.latency_history_index =
        (state.latency_history_index + 1) % UiState::kLatencyHistorySize;
    if (state.latency_history_count < UiState::kLatencyHistorySize) {
        state.latency_history_count++;
    }
}

bool compute_stats(const UiState& state, float& mean, float& stddev) {
    if (state.latency_history_count <= 0) {
        mean = 0.0f;
        stddev = 0.0f;
        return false;
    }
    double sum = 0.0;
    for (int i = 0; i < state.latency_history_count; ++i) {
        sum += state.latency_history[i];
    }
    mean = static_cast<float>(sum / state.latency_history_count);
    double var_sum = 0.0;
    for (int i = 0; i < state.latency_history_count; ++i) {
        const double d = state.latency_history[i] - mean;
        var_sum += d * d;
    }
    stddev = static_cast<float>(std::sqrt(var_sum / state.latency_history_count));
    return true;
}

int effective_bpi(const ninjam::NinjamPlugin* plugin) {
    if (!plugin) return 4;
    int bpi = plugin->ui_snapshot.bpi.load(std::memory_order_relaxed);
    if (bpi <= 0) {
        bpi = plugin->ui_state.bpi;
    }
    return (bpi > 0) ? bpi : 4;
}

} // namespace

void ui_render_latency_guide(ninjam::NinjamPlugin* plugin) {
    if (!plugin) return;

    auto& state = plugin->ui_state;
    if (!ImGui::CollapsingHeader("Timing Guide##panel",
                                 ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    ImGui::Indent();

    ImGui::SetNextItemWidth(200.0f);
    ImGui::SliderFloat("Threshold##latency", &state.transient_threshold,
                       0.01f, 0.5f, "%.2f");

    if (plugin->ui_snapshot.transient_detected.load(std::memory_order_acquire)) {
        const float offset =
            plugin->ui_snapshot.last_transient_beat_offset.load(
                std::memory_order_relaxed);
        push_transient(state, offset);
        plugin->ui_snapshot.transient_detected.store(
            false, std::memory_order_release);
    }

    const float height = 60.0f;
    const ImVec2 start = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    const ImVec2 end = ImVec2(start.x + width, start.y + height);

    auto* draw_list = ImGui::GetWindowDrawList();
    const ImU32 line_col = ImGui::GetColorU32(ImGuiCol_Border);
    draw_list->AddRect(start, end, line_col);

    const int bpi = effective_bpi(plugin);
    const float beat_width = (bpi > 0) ? (width / bpi) : width;
    const float center_x = start.x + width * 0.5f;

    if (bpi > 0 && width > 4.0f) {
        for (int i = 0; i < bpi; ++i) {
            const float x = start.x + width * (static_cast<float>(i) + 0.5f) /
                                           static_cast<float>(bpi);
            draw_list->AddLine(ImVec2(x, start.y + 4.0f),
                               ImVec2(x, end.y - 4.0f), line_col);
        }
    }

    const ImU32 label_col = ImGui::GetColorU32(ImGuiCol_TextDisabled);
    draw_list->AddText(ImVec2(start.x + 4.0f, start.y + 4.0f),
                       label_col, "Early");
    const char* right_label = "Late";
    const ImVec2 right_size = ImGui::CalcTextSize(right_label);
    draw_list->AddText(ImVec2(end.x - right_size.x - 4.0f, start.y + 4.0f),
                       label_col, right_label);

    const char* center_label = "On beat";
    const ImVec2 center_size = ImGui::CalcTextSize(center_label);
    const float center_label_y = end.y - center_size.y - 4.0f;
    draw_list->AddText(ImVec2(center_x - center_size.x * 0.5f, center_label_y),
                       label_col, center_label);

    const float bpm_for_ticks = plugin->ui_snapshot.bpm.load(
        std::memory_order_relaxed);
    if (bpm_for_ticks > 0.0f && beat_width > 0.0f) {
        const float ms_per_beat = 60000.0f / bpm_for_ticks;
        const float tick_ms = 5.0f;
        const float beat_offset = tick_ms / ms_per_beat;
        const float tick_x = beat_offset * beat_width;
        const float tick_y = center_label_y - 14.0f;
        if (center_x - tick_x > start.x + 4.0f) {
            const char* left_tick = "-5ms";
            const ImVec2 left_size = ImGui::CalcTextSize(left_tick);
            draw_list->AddText(ImVec2(center_x - tick_x - left_size.x * 0.5f, tick_y),
                               label_col, left_tick);
        }
        if (center_x + tick_x < end.x - 4.0f) {
            const char* right_tick = "+5ms";
            const ImVec2 right_size = ImGui::CalcTextSize(right_tick);
            draw_list->AddText(ImVec2(center_x + tick_x - right_size.x * 0.5f, tick_y),
                               label_col, right_tick);
        }
    }

    float mean = 0.0f;
    float stddev = 0.0f;
    const bool have_stats = compute_stats(state, mean, stddev);

    const ImU32 dot_col = ImGui::GetColorU32(ImGuiCol_PlotHistogram);
    const float mid_y = (start.y + end.y) * 0.5f;
    for (int i = 0; i < state.latency_history_count; ++i) {
        const float x = center_x + state.latency_history[i] * beat_width;
        if (x >= start.x && x <= end.x) {
            draw_list->AddCircleFilled(ImVec2(x, mid_y), 3.0f, dot_col);
        }
    }

    if (have_stats) {
        const ImU32 mean_col = ImGui::GetColorU32(ImGuiCol_Text);
        const float mean_x = center_x + mean * beat_width;
        if (mean_x >= start.x && mean_x <= end.x) {
            draw_list->AddLine(ImVec2(mean_x, start.y + 2.0f),
                               ImVec2(mean_x, end.y - 2.0f), mean_col, 2.0f);
        }

        const float bpm = plugin->ui_snapshot.bpm.load(std::memory_order_relaxed);
        if (bpm > 0.0f) {
            const float ms_per_beat = 60000.0f / bpm;
            const float mean_ms = mean * ms_per_beat;
            const float abs_ms = std::fabs(mean_ms);
            ImVec4 color = ImVec4(0.2f, 0.8f, 0.2f, 1.0f);
            const char* label = "On beat";
            if (abs_ms > 15.0f) {
                color = ImVec4(0.9f, 0.2f, 0.2f, 1.0f);
                label = (mean_ms > 0.0f) ? "Late" : "Early";
            } else if (abs_ms > 5.0f) {
                color = ImVec4(0.9f, 0.8f, 0.2f, 1.0f);
                label = (mean_ms > 0.0f) ? "Slightly late" : "Slightly early";
            }

            ImGui::Spacing();
            ImGui::Text("Avg offset: %+0.1f ms (std: %0.1f ms)",
                        mean_ms, stddev * ms_per_beat);
            ImGui::SameLine();
            ImGui::TextColored(color, "%s", label);
        }
    }

    ImGui::Dummy(ImVec2(width, height));

    if (have_stats) {
        const float consistency = 1.0f - std::min(stddev / 0.1f, 1.0f);
        ImGui::SetNextItemWidth(200.0f);
        ImGui::ProgressBar(consistency, ImVec2(200.0f, 0.0f), "Consistency");
    }

    ImGui::Unindent();
}
