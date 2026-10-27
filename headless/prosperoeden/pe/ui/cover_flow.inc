// ProsperoEden - Cover Flow stage, using the library's live selection and cover cache.
// SPDX-License-Identifier: GPL-3.0-or-later

void Launcher::draw_cover_flow(Canvas &c, const Rect &stage)
{
    auto &list = c.list;
    const Theme &t = th();
    const int count = static_cast<int>(games_.size());
    if (count == 0) return;
    const float position = look().reduce_motion ? static_cast<float>(library_.selected) : cover_flow_.position;
    int visible = 0;
    const auto order = cover_flow::order(count, position, visible);
    const auto outline = [&](const gfx::PerspectiveQuad &q, Color color, float thickness) {
        list.line(q.left_x, q.left_top, q.right_x, q.right_top, thickness, color);
        list.line(q.right_x, q.right_top, q.right_x, q.right_bottom, thickness, color);
        list.line(q.right_x, q.right_bottom, q.left_x, q.left_bottom, thickness, color);
        list.line(q.left_x, q.left_bottom, q.left_x, q.left_top, thickness, color);
    };

    list.push_clip(stage);
    // Load the selected cover first: neighbours never starve the controller's current choice.
    c.textures.cover(games_[static_cast<std::size_t>(library_.selected)].cover,
                     std::min(stage.w * 0.34f, stage.h * 0.52f));
    for (int n = 0; n < visible; ++n)
    {
        const int i = order[static_cast<std::size_t>(n)];
        const Game &game = games_[static_cast<std::size_t>(i)];
        const float offset = static_cast<float>(i) - position;
        const auto card = cover_flow::project(offset, stage);
        const bool selected = i == library_.selected;
        const Cover picture = c.textures.cover(game.cover, card.bounds.h);
        // Physical yaw, camera distance and perspective-correct UVs; not scaled flat rectangles.
        const float shade = selected || look().high_contrast ? 1.0f :
                            std::max(0.48f, 0.86f - std::fabs(offset) * 0.08f);
        list.perspective_image(0, card.quad, t.panel);
        if (picture.texture != 0)
            list.perspective_image(picture.texture, card.quad, {shade, shade, shade, game.remote ? 0.65f : 1.0f});
        const bool active = screen_ == Screen::library ? !rail_focus_ : home_on_grid_;
        outline(card.quad, selected && active ? t.accent : t.rule,
                selected ? (look().high_contrast ? 6.0f : 4.0f) : 1.0f);
        if (selected && picture.texture == 0)
        {
            const float cx = card.bounds.x + card.bounds.w * 0.5f;
            const float cy = card.bounds.y + card.bounds.h * 0.5f;
            icon(c, Icon::library, cx, cy - 12.0f, 68.0f, t.meta);
            text_shrink(c, picture.missing ? tr(kCoverUnavailable) : tr("Reading..."), cx,
                        cy + 58.0f, t.size_small, t.copy, card.bounds.w - 24.0f, Align::center);
        }
    }
    list.pop_clip();

    // Names and actions are never tilted or dimmed: they remain readable at 720p and with large text.
    const Game &game = games_[static_cast<std::size_t>(library_.selected)];
    const float cx = stage.x + stage.w * 0.5f;
    const float name_top = stage.y + stage.h * 0.75f;
    const float line = look().large_text ? 42.0f : 36.0f;
    {
        const Heavy heavy(c);
        text_block(c, game.name, stage.x + 28.0f, baseline(name_top, line, t.size_heading),
                   t.size_heading, line, t.title, stage.w - 56.0f, 2);
    }
    Color tone = t.meta;
    const std::string state = game.remote ? remote_state(game, &tone) : game.format + "  /  " + game.size;
    text_shrink(c, state, stage.x + 28.0f, baseline(name_top + line * 2.0f + 8.0f, 32.0f, t.size_small),
                t.size_small, tone, stage.w - 56.0f);
    // Persistent directional affordance, including the first/last bound (no wrapping).
    draw_pad(c, Pad::leftright, cx - 84.0f, stage.y + stage.h - 24.0f, 28.0f);
    text_shrink(c, list_position(library_.selected + 1, count), cx + 10.0f,
                baseline(stage.y + stage.h - 40.0f, 32.0f, t.size_small), t.size_small, t.copy, 160.0f);
}
