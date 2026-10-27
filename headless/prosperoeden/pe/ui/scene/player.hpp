// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "pe/ui/scene/model.hpp"
#include <functional>

namespace pe::ui::scene {
// No GPU handles, service pointers, scripts, clocks or filesystem paths cross this boundary.
// The host owns snapshots, navigation, timing and the bounded draw command vocabulary.
class Player {
public:
    using Query = std::function<Json(const std::string&)>;
    using Dispatch = std::function<void(const std::string&, const Json&)>;
    using Draw = std::function<void(const std::string&, const Json&)>;
    using Sound = std::function<void(const std::string&, bool)>;
    explicit Player(Model model) : model_(std::move(model)), screen_(model_.entry) { enter(); }
    const std::string& screen() const { return screen_; }
    const std::string& focus() const { return focus_; }
    const std::string& state() const { return state_; }
    std::size_t selected_index() const {
        const auto found = selection_.find(focus_); return found == selection_.end() ? 0 : found->second;
    }
    void update(float dt, bool reduced) {
        reduced_ = reduced;
        if (!std::isfinite(dt) || dt < 0) return;
        const float step = std::min(dt, 0.05f);
        elapsed_ += step; focus_time_ += step; state_time_ += step;
    }
    void press(const std::string& key, const Query& query, const Dispatch& dispatch, const Sound& sound) {
        snapshot(query); normalize_selection();
        const auto* current = find(focus_);
        if (key == "up" || key == "down" || key == "left" || key == "right") {
            bool moved = false;
            if (current && collection(*current)) {
                const auto& rows = source(*current); auto& index = selection_[focus_];
                const int columns = current->type == "list" ? 1 : current->properties.value("columns", 4);
                const long long delta = key == "left" ? -1 : key == "right" ? 1 : key == "up" ? -columns : columns;
                const auto target = static_cast<long long>(index) + delta;
                if (target >= 0 && target < static_cast<long long>(rows.size())) { index = static_cast<std::size_t>(target); moved = true; }
            }
            if (!moved && current && current->links.contains(key)) {
                const auto target = current->links.at(key).get<std::string>();
                const auto* next = find(target);
                if (next && focusable(*next) && visible(*next)) { focus_ = target; moved = true; }
            }
            if (!moved) {
                std::vector<const Node*> all; collect(screen_model().nodes, all);
                all.erase(std::remove_if(all.begin(), all.end(), [&](auto* n) { return !focusable(*n) || !visible(*n); }), all.end());
                if (!all.empty()) {
                    auto at = std::find_if(all.begin(), all.end(), [&](auto* n) { return n->id == focus_; });
                    const auto index = at == all.end() ? 0 : static_cast<std::size_t>(at - all.begin());
                    const auto next = (index + (key == "left" || key == "up" ? all.size() - 1 : 1)) % all.size();
                    if (all[next]->id != focus_) { focus_ = all[next]->id; moved = true; }
                }
            }
            if (moved) { focus_time_ = 0; remember_selection(); cue("focus", sound); }
            return;
        }
        normalize_selection();
        Json operation;
        if (key == "select" && current && collection(*current) && source(*current).empty()) return;
        if (key == "select" && current && !current->activate.is_null()) operation = current->activate;
        else if (screen_model().input.contains(key)) operation = screen_model().input.at(key);
        // Options is an unhideable recovery route, even for a theme without navigation.
        else if (key == "options") operation = {{"action", "launcher.picker"}};
        if (operation.is_null()) return;
        cue(key == "back" ? "back" : "select", sound);
        if (operation.contains("screen")) { screen_ = operation.at("screen"); enter(); }
        else if (operation.contains("state")) { state_ = operation.at("state"); state_time_ = 0; }
        else dispatch(operation.at("action"), resolve(operation.value("args", Json::object())));
    }
    void draw(const Query& query, const Draw& draw, const Sound& sound) {
        snapshot(query); normalize_selection();
        if (music_dirty_) { sound(screen_model().music, true); music_dirty_ = false; }
        commands_ = 0; text_bytes_ = 0;
        const float fade = reduced_ || screen_model().transition == 0 ? 1 : static_cast<float>(std::min(1.0, elapsed_ / screen_model().transition));
        emit(draw, "opacity", {{"value", fade}});
        render(screen_model().nodes, draw, false);
        emit(draw, "unopacity", Json::object());
    }
private:
    Model model_;
    std::string screen_, focus_, state_;
    std::map<std::string, std::size_t> selection_;
    std::map<std::string, std::string> handles_;
    Json data_ = Json::object();
    double elapsed_{}, focus_time_{}, state_time_{};
    bool reduced_{}, music_dirty_ = true;
    int commands_{};
    std::size_t text_bytes_{};
    const Screen& screen_model() const { return model_.screens.at(screen_); }
    static bool collection(const Node& n) { return n.type == "list" || n.type == "grid"; }
    static bool focusable(const Node& n) {
        return n.properties.value("focusable", n.type == "focus" || collection(n) || !n.activate.is_null());
    }
    bool visible(const Node& n) const {
        return n.properties.value("visible", true) && (!n.properties.contains("state") || n.properties.at("state") == state_);
    }
    void collect(const std::vector<Node>& nodes, std::vector<const Node*>& out) const {
        for (const auto& n : nodes) {
            if (!visible(n)) continue;
            out.push_back(&n); if (!collection(n)) collect(n.children, out);
        }
    }
    const Node* find(const std::string& id) const {
        std::vector<const Node*> all; collect(screen_model().nodes, all);
        const auto it = std::find_if(all.begin(), all.end(), [&](auto* n) { return n->id == id; });
        return it == all.end() ? nullptr : *it;
    }
    void enter() {
        elapsed_ = focus_time_ = state_time_ = 0; state_.clear(); focus_ = screen_model().initial_focus; music_dirty_ = true;
        if (focus_.empty()) {
            std::vector<const Node*> all; collect(screen_model().nodes, all);
            for (const auto* n : all) if (focusable(*n) && visible(*n)) { focus_ = n->id; break; }
        }
    }
    void snapshot(const Query& query) {
        data_ = Json::object();
        for (const auto* root : {"library", "home", "profiles", "status", "preferences"}) data_[root] = query(root);
    }
    Json value(const std::string& pointer) const {
        try { return data_.at(Json::json_pointer(pointer)); } catch (...) { return nullptr; }
    }
    const Json& source(const Node& n) const {
        static const Json empty = Json::array();
        try {
            const auto& rows = data_.at(Json::json_pointer(n.properties.at("source").get<std::string>()));
            if (!rows.is_array()) return empty;
            require(rows.size() <= 8192, "model collection exceeds limit");
            return rows;
        } catch (const Json::exception&) { return empty; }
    }
    void normalize_selection() {
        data_["selected"] = nullptr;
        std::vector<const Node*> all; collect(screen_model().nodes, all);
        const auto* focused = find(focus_);
        if (!focused || !focusable(*focused)) {
            focus_.clear();
            for (const auto* n : all) if (focusable(*n)) { focus_ = n->id; focus_time_ = 0; break; }
        }
        for (const auto* n : all) if (collection(*n)) {
            const auto& rows = source(*n); auto& index = selection_[n->id];
            const auto saved = handles_.find(n->id);
            if (saved != handles_.end()) {
                const auto it = std::find_if(rows.begin(), rows.end(), [&](const auto& row) { return row.is_object() && row.value("handle", "") == saved->second; });
                if (it != rows.end()) index = static_cast<std::size_t>(it - rows.begin());
            }
            index = rows.empty() ? 0 : std::min(index, rows.size() - 1);
            if (n->id == focus_ && !rows.empty()) data_["selected"] = rows[index];
        }
        remember_selection();
    }
    void remember_selection() {
        const auto* n = find(focus_);
        if (!n || !collection(*n)) return;
        const auto& rows = source(*n); const auto index = selection_[n->id];
        if (index < rows.size() && rows[index].is_object()) {
            const auto handle = rows[index].value("handle", "");
            if (!handle.empty()) handles_[n->id] = handle;
            data_["selected"] = rows[index];
        }
    }
    Json resolve(const Json& arg, int depth = 0) const {
        require(depth <= 8, "argument binding depth exceeded");
        if (arg.is_object()) {
            if (arg.size() == 1 && arg.contains("$bind")) { binding(arg.at("$bind")); return value(arg.at("$bind")); }
            Json out = Json::object(); for (auto it = arg.begin(); it != arg.end(); ++it) out[it.key()] = resolve(it.value(), depth + 1); return out;
        }
        if (arg.is_array()) { Json out = Json::array(); for (const auto& item : arg) out.push_back(resolve(item, depth + 1)); return out; }
        return arg;
    }
    void cue(const char* name, const Sound& sound) {
        if (model_.cues.contains(name)) sound(model_.cues.at(name), false);
    }
    void emit(const Draw& draw, const std::string& kind, const Json& args) {
        if (kind == "text") {
            text_bytes_ += args.value("text", std::string{}).size();
            require(text_bytes_ <= 32768, "frame text/glyph budget exceeded");
        }
        require(++commands_ <= 8192, "frame command budget exceeded"); draw(kind, args.is_null() ? Json::object() : args);
    }
    Json animated(const Node& n) const {
        Json p = n.properties;
        for (const auto& t : screen_model().tracks) {
            if (t.node != n.id || (t.trigger == "focus" && focus_ != n.id) ||
                (t.trigger.starts_with("state:") && t.trigger.substr(6) != state_)) continue;
            const double clock = t.trigger == "focus" ? focus_time_ : t.trigger.starts_with("state:") ? state_time_ : elapsed_;
            const float at = reduced_ ? 1 : static_cast<float>(t.loop ? std::fmod(clock, t.duration) / t.duration : std::min(1.0, clock / t.duration));
            auto right = std::upper_bound(t.keys.begin(), t.keys.end(), at, [](float v, auto k) { return v < k.first; });
            if (right == t.keys.end()) p[t.property] = t.keys.back().second;
            else if (right == t.keys.begin()) p[t.property] = right->second;
            else { const auto left = right - 1; const float f = (at - left->first) / (right->first - left->first); p[t.property] = left->second + (right->second - left->second) * f; }
        }
        return p;
    }
    void render(const std::vector<Node>& nodes, const Draw& draw, bool in_collection) {
        for (const auto& n : nodes) {
            if (!visible(n)) continue;
            Json p = animated(n);
            const float x = p.value("x", 0.0f), y = p.value("y", 0.0f), w = p.value("w", 0.0f), h = p.value("h", 0.0f);
            emit(draw, "opacity", {{"value", p.value("alpha", 1.0f)}});
            // Alpha belongs to the retained subtree stack, not also the primitive's tint.
            p.erase("alpha");
            emit(draw, "transform", {{"scale", p.value("scale", 1.0f)}, {"ox", x + w / 2}, {"oy", y + h / 2}});
            if (n.type == "group" || n.type == "focus") {
                if (n.type == "focus" && n.id == focus_) emit(draw, "focus", p);
                emit(draw, "transform", {{"dx", x}, {"dy", y}}); render(n.children, draw, in_collection); emit(draw, "untransform", {});
            } else if (n.type == "mask") {
                emit(draw, "clip", p); render(n.children, draw, in_collection); emit(draw, "unclip", {});
            } else if (collection(n)) {
                require(!in_collection, "nested collections are unsupported");
                const auto& rows = source(n); const auto selected = selection_[n.id];
                const int columns = n.type == "list" ? 1 : p.value("columns", 4), visible_rows = p.value("rows", 2);
                const float cw = p.value("cell_w", 250.0f), ch = p.value("cell_h", 300.0f), gap = p.value("gap", 16.0f);
                const auto first_row = selected / columns >= static_cast<std::size_t>(visible_rows) ? selected / columns - visible_rows + 1 : 0;
                const auto first = first_row * columns, end = std::min(rows.size(), first + columns * visible_rows);
                emit(draw, "clip", {{"x", x}, {"y", y}, {"w", w}, {"h", h}});
                if (rows.empty()) emit(draw, "text", {{"x", x}, {"y", y + 36}, {"text", p.value("empty", "No items")}, {"size", 28}});
                for (auto i = first; i < end; ++i) {
                    const float dx = x + float((i - first) % columns) * (cw + gap), dy = y + float((i - first) / columns) * (ch + gap);
                    emit(draw, "transform", {{"dx", dx}, {"dy", dy}}); data_["item"] = rows[i];
                    if (focus_ == n.id && i == selected) emit(draw, "focus", {{"x", -6}, {"y", -6}, {"w", cw + 12}, {"h", ch + 12}, {"radius", p.value("radius", 12.0f)}});
                    render(n.children, draw, true); emit(draw, "untransform", {});
                }
                data_.erase("item"); emit(draw, "unclip", {});
            } else if (n.type == "particles") {
                // Deterministic bounded host particles; calm mode removes all ambient motion.
                if (!reduced_) for (int i = 0; i < p.value("count", 24); ++i) {
                    const float px = x + std::fmod(float(i * 137 + 29), std::max(1.0f, w));
                    const float py = y + std::fmod(float(i * 83) + elapsed_ * p.value("speed", 12.0f), std::max(1.0f, h));
                    Json dot = p; dot["x"] = px; dot["y"] = py; dot["radius"] = p.value("radius", 2.0f); emit(draw, "circle", dot);
                }
            } else if (n.type == "nineslice") {
                const auto s = p.at("slice").get<std::vector<float>>();
                const auto uv = p.value("uv", std::vector<float>{0, 0, 1, 1});
                // Border extent is in virtual pixels; source cuts are normalized image fractions.
                const float border = p.value("border", 16.0f), bx = std::min(border, w / 2), by = std::min(border, h / 2);
                const float xs[]{x, x + bx, x + w - bx, x + w}, ys[]{y, y + by, y + h - by, y + h};
                const float us[]{uv[0], uv[0] + uv[2] * s[0], uv[0] + uv[2] * (1 - s[2]), uv[0] + uv[2]};
                const float vs[]{uv[1], uv[1] + uv[3] * s[1], uv[1] + uv[3] * (1 - s[3]), uv[1] + uv[3]};
                for (int yy = 0; yy < 3; ++yy) for (int xx = 0; xx < 3; ++xx) {
                    Json part = p; part["x"] = xs[xx]; part["y"] = ys[yy]; part["w"] = xs[xx + 1] - xs[xx]; part["h"] = ys[yy + 1] - ys[yy];
                    part["uv"] = {us[xx], vs[yy], us[xx + 1] - us[xx], vs[yy + 1] - vs[yy]}; part.erase("radius"); emit(draw, "image", part);
                }
            } else {
                if (p.contains("bind")) {
                    const auto bound = value(p.at("bind"));
                    if (n.type == "text") p["text"] = bound.is_string() ? bound.get<std::string>() : bound.is_primitive() && !bound.is_null() ? bound.dump() : "";
                    else if (n.type == "cover") p["game"] = bound.is_string() ? bound.get<std::string>() : "";
                }
                if (n.type == "text") { p["width"] = w; if (!p.contains("bind")) p["translate"] = true; }
                emit(draw, n.type, p);
            }
            emit(draw, "untransform", {}); emit(draw, "unopacity", {});
        }
    }
};
} // namespace pe::ui::scene
