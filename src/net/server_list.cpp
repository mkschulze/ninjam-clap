/*
    JamWide Plugin - server_list.cpp
    Public server list fetcher implementation
*/

#include "server_list.h"
#include "wdl/jsonparse.h"

#include <cstdlib>
#include <cstring>
#include <vector>

namespace jamwide {

namespace {

int parse_int(const char* value, int fallback) {
    if (!value || !value[0]) {
        return fallback;
    }
    char* end = nullptr;
    long parsed = std::strtol(value, &end, 10);
    if (end == value) {
        return fallback;
    }
    return static_cast<int>(parsed);
}

const char* get_string_or_empty(const wdl_json_element* elem,
                                const char* name) {
    if (!elem) return "";
    const char* value = elem->get_string_by_name(name, true);
    return value ? value : "";
}

const wdl_json_element* get_list_root(const wdl_json_element* root) {
    if (!root) return nullptr;
    if (root->is_array()) return root;
    if (root->is_object()) {
        const wdl_json_element* servers = root->get_item_by_name("servers");
        if (servers && servers->is_array()) return servers;
    }
    return nullptr;
}

int count_array_items(const wdl_json_element* elem) {
    if (!elem || !elem->is_array()) {
        return 0;
    }
    int count = 0;
    while (elem->enum_item(count)) {
        ++count;
    }
    return count;
}

int parse_user_count(const wdl_json_element* item) {
    if (!item) return 0;

    const char* count_value = item->get_string_by_name("user_count", true);
    if (count_value && count_value[0]) {
        return parse_int(count_value, 0);
    }

    const char* users_value = item->get_string_by_name("users", true);
    if (users_value && users_value[0]) {
        return parse_int(users_value, 0);
    }

    const wdl_json_element* users_elem = item->get_item_by_name("users");
    if (users_elem && users_elem->is_array()) {
        return count_array_items(users_elem);
    }

    return 0;
}

} // namespace

ServerListFetcher::ServerListFetcher() = default;

void ServerListFetcher::reset_state() {
    buffer_.clear();
    reply_code_ = 0;
    active_ = false;
}

void ServerListFetcher::request(const std::string& url) {
    reset_state();
    url_ = url;
    if (url_.empty()) {
        return;
    }
    http_.addheader("User-Agent: JamWide");
    http_.addheader("Accept: text/plain, application/json");
    http_.connect(url_.c_str());
    active_ = true;
}

bool ServerListFetcher::in_flight() const {
    return active_;
}

bool ServerListFetcher::poll(ServerListResult& result) {
    if (!active_) {
        return false;
    }

    int st = http_.run();
    if (st < 0) {
        result.servers.clear();
        result.error = http_.geterrorstr();
        reset_state();
        return true;
    }

    if (http_.get_status() >= 2) {
        int available = 0;
        while ((available = http_.bytes_available()) > 0) {
            char buf[4096];
            if (available > static_cast<int>(sizeof(buf))) {
                available = static_cast<int>(sizeof(buf));
            }
            const int read = http_.get_bytes(buf, available);
            if (read > 0) {
                buffer_.append(buf, static_cast<size_t>(read));
            } else {
                break;
            }
        }
    }

    if (st == 1) {
        reply_code_ = http_.getreplycode();
        if (reply_code_ < 200 || reply_code_ >= 300) {
            result.servers.clear();
            result.error = http_.getreply();
            reset_state();
            return true;
        }

        result.servers.clear();
        result.error.clear();
        const bool ok = parse_response(buffer_, result);
        if (!ok && result.error.empty()) {
            result.error = "Failed to parse server list";
        }
        reset_state();
        return true;
    }

    return false;
}

bool ServerListFetcher::parse_response(const std::string& data,
                                       ServerListResult& result) {
    // Auto-detect format: plain text starts with "SERVER", JSON starts with '{' or '['
    if (!data.empty()) {
        size_t start = 0;
        while (start < data.size() && (data[start] == ' ' || data[start] == '\n' || data[start] == '\r')) {
            ++start;
        }
        if (start < data.size() && data.substr(start, 6) == "SERVER") {
            return parse_ninjam_format(data, result);
        }
    }
    return parse_json_format(data, result);
}

// Parse ninjam.com plain-text format:
// SERVER "host:port" "BPM/BPI" "users/max:name1,name2,..."
bool ServerListFetcher::parse_ninjam_format(const std::string& data,
                                            ServerListResult& result) {
    result.servers.clear();
    result.error.clear();

    // Helper to extract quoted string
    auto extract_quoted = [](const char*& p) -> std::string {
        while (*p && *p != '"') ++p;
        if (*p != '"') return "";
        ++p; // skip opening quote
        const char* start = p;
        while (*p && *p != '"') ++p;
        std::string s(start, p);
        if (*p == '"') ++p; // skip closing quote
        return s;
    };

    const char* p = data.c_str();
    while (*p) {
        // Find start of line
        while (*p && (*p == ' ' || *p == '\t')) ++p;

        // Check for SERVER keyword
        if (std::strncmp(p, "SERVER", 6) == 0) {
            p += 6;

            // Extract 3 quoted strings
            std::string host_port = extract_quoted(p);
            std::string tempo = extract_quoted(p);
            std::string users_info = extract_quoted(p);

            if (!host_port.empty()) {
                ServerListEntry entry;

                // Parse host:port
                size_t colon = host_port.rfind(':');
                if (colon != std::string::npos) {
                    entry.host = host_port.substr(0, colon);
                    entry.port = parse_int(host_port.c_str() + colon + 1, 2049);
                } else {
                    entry.host = host_port;
                    entry.port = 2049;
                }
                entry.name = entry.host;

                // Parse tempo: "BPM/BPI" or "lobby"
                if (tempo == "lobby" || tempo == "Lobby") {
                    entry.is_lobby = true;
                    entry.bpm = 0;
                    entry.bpi = 0;
                } else {
                    size_t slash = tempo.find('/');
                    if (slash != std::string::npos) {
                        // Format could be "110 BPM/16" or "110/16"
                        std::string bpm_part = tempo.substr(0, slash);
                        // Remove " BPM" suffix if present
                        size_t bpm_pos = bpm_part.find(" BPM");
                        if (bpm_pos != std::string::npos) {
                            bpm_part = bpm_part.substr(0, bpm_pos);
                        }
                        entry.bpm = parse_int(bpm_part.c_str(), 0);
                        entry.bpi = parse_int(tempo.c_str() + slash + 1, 0);
                    }
                }

                // Parse users: "current/max:name1,name2,..."
                size_t user_colon = users_info.find(':');
                if (user_colon != std::string::npos) {
                    std::string counts = users_info.substr(0, user_colon);
                    entry.user_list = users_info.substr(user_colon + 1);

                    // Remove "(empty)" placeholder
                    if (entry.user_list == "(empty)") {
                        entry.user_list.clear();
                    }

                    size_t slash = counts.find('/');
                    if (slash != std::string::npos) {
                        entry.users = parse_int(counts.c_str(), 0);
                        entry.max_users = parse_int(counts.c_str() + slash + 1, 0);
                    }
                }

                result.servers.push_back(std::move(entry));
            }
        }
        // Skip to next line
        while (*p && *p != '\n') ++p;
        if (*p == '\n') ++p;
    }

    return true;
}

bool ServerListFetcher::parse_json_format(const std::string& data,
                                          ServerListResult& result) {
    wdl_json_parser parser;
    wdl_json_element* root = parser.parse(data.c_str(),
                                          static_cast<int>(data.size()));
    if (!root) {
        result.error = parser.m_err ? parser.m_err : "JSON parse error";
        return false;
    }

    const wdl_json_element* list = get_list_root(root);
    if (!list) {
        result.error = "Server list not found";
        delete root;
        return false;
    }

    for (int i = 0; ; ++i) {
        const wdl_json_element* item = list->enum_item(i);
        if (!item) break;
        if (!item->is_object()) continue;

        ServerListEntry entry;
        entry.name = get_string_or_empty(item, "name");
        entry.host = get_string_or_empty(item, "host");
        if (entry.host.empty()) {
            entry.host = get_string_or_empty(item, "ip");
        }
        if (entry.host.empty()) {
            entry.host = get_string_or_empty(item, "address");
        }

        const char* port_value = item->get_string_by_name("port", true);
        entry.port = parse_int(port_value, 0);
        if (entry.port == 0) {
            const char* port_alt = item->get_string_by_name("portnum", true);
            entry.port = parse_int(port_alt, 0);
        }

        entry.users = parse_user_count(item);

        entry.topic = get_string_or_empty(item, "topic");

        if (!entry.host.empty()) {
            result.servers.push_back(std::move(entry));
        }
    }

    delete root;
    return true;
}

} // namespace jamwide
