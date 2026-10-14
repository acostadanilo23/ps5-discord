#include "concepts/concepts.hpp"

#include "ps5discord/config.hpp"
#include "ps5discord/discord/service.hpp"
#include "ui/fonts.hpp"

#include <algorithm>
#include <array>
#include <memory>
#include <string>

namespace hui::concepts {
namespace {

using gfx::Color;
using gfx::Rect;

const Color kBg = Color::rgb(0x1e1f22);
const Color kPanel = Color::rgb(0x2b2d31);
const Color kPanel2 = Color::rgb(0x313338);
const Color kMuted = Color::rgb(0x949ba4);
const Color kText = Color::rgb(0xf2f3f5);
const Color kAccent = Color::rgb(0x5865f2);
const Color kDanger = Color::rgb(0xed4245);

constexpr float kTop = 112.0f;
constexpr float kBottom = 990.0f;
constexpr float kGuildX = 42.0f;
constexpr float kGuildW = 300.0f;
constexpr float kChannelX = 356.0f;
constexpr float kChannelW = 400.0f;
constexpr float kChatX = 770.0f;
constexpr float kChatW = 1108.0f;

constexpr const char* kTechniques[] = {
    "Three-column controller-first chat layout",
    "Background network service with polling and rate-limit handling",
    "Built-in controller keyboard for sending messages",
};

class DiscordDesign final : public app::Concept {
public:
    explicit DiscordDesign(app::Context& context) : context_(context) {
        reload_auth();
    }

    ~DiscordDesign() override {
        if (service_) service_->stop();
    }

    const app::ConceptInfo& info() const override {
        static const app::ConceptInfo info{
            "ps5discord", "PS5 Discord", "Personal Discord text client",
            "overlay/src/concepts/ps5discord.cpp", audio::SoundSet::glass, kAccent, kTechniques,
        };
        return info;
    }

    void enter() override {
        if (service_ && !service_->running()) service_->start();
    }

    void update(const InputFrame& input, float, app::Feedback& feedback) override {
        if (!service_) {
            if (input.is_pressed(Action::north)) reload_auth();
            return;
        }

        snapshot_ = service_->snapshot();
        clamp_focus();

        if (composing_) {
            update_keyboard(input, feedback);
            return;
        }

        if (input.is_pressed(Action::north)) {
            service_->refresh_guilds();
            feedback.play(audio::Cue::select);
        }

        if (input.nav == Direction::left && pane_ > 0) {
            --pane_;
            feedback.play(audio::Cue::focus);
        } else if (input.nav == Direction::right && pane_ < 2) {
            ++pane_;
            feedback.play(audio::Cue::focus);
        } else if (input.nav == Direction::up || input.nav == Direction::down) {
            int delta = input.nav == Direction::down ? 1 : -1;
            if (pane_ == 0 && !snapshot_.guilds.empty()) {
                guild_focus_ = std::clamp(guild_focus_ + delta, 0, static_cast<int>(snapshot_.guilds.size()) - 1);
            } else if (pane_ == 1 && !snapshot_.channels.empty()) {
                channel_focus_ = std::clamp(channel_focus_ + delta, 0, static_cast<int>(snapshot_.channels.size()) - 1);
            } else if (pane_ == 2 && !snapshot_.messages.empty()) {
                message_scroll_ = std::clamp(message_scroll_ + delta, 0, std::max(0, static_cast<int>(snapshot_.messages.size()) - 1));
            }
            feedback.play(audio::Cue::focus);
        }

        if (input.is_pressed(Action::confirm)) {
            if (pane_ == 0 && guild_focus_ >= 0 && guild_focus_ < static_cast<int>(snapshot_.guilds.size())) {
                service_->select_guild(snapshot_.guilds[static_cast<std::size_t>(guild_focus_)].id);
                pane_ = 1;
                channel_focus_ = 0;
                feedback.play(audio::Cue::select);
            } else if (pane_ == 1 && channel_focus_ >= 0 && channel_focus_ < static_cast<int>(snapshot_.channels.size())) {
                service_->select_channel(snapshot_.channels[static_cast<std::size_t>(channel_focus_)].id);
                pane_ = 2;
                message_scroll_ = 0;
                feedback.play(audio::Cue::select);
            }
        }

        if (pane_ == 2 && !snapshot_.selected_channel_id.empty() && input.is_pressed(Action::west)) {
            composing_ = true;
            compose_.clear();
            key_row_ = 0;
            key_col_ = 0;
            feedback.play(audio::Cue::select);
        }
    }

    void draw(app::Frame& frame) const override {
        frame.backdrop.mode = gfx::BackdropMode::gradient;
        frame.backdrop.colors[0] = kBg;
        frame.backdrop.colors[1] = Color::rgb(0x15161a);
        auto& list = frame.scene;
        const auto& fonts = context_.fonts;

        list.rounded_rect({24, 28, 1872, 1024}, 28, kBg);
        ui::text(list, fonts.semibold, "PS5 Discord", 48, 78, 34, kText);

        std::string identity = snapshot_.me.username.empty() ? "Not connected" :
            ((snapshot_.me.global_name.empty() ? snapshot_.me.username : snapshot_.me.global_name) + "  @" + snapshot_.me.username);
        ui::text(list, fonts.regular, identity, 1848, 76, 22, kMuted, gfx::Align::right);

        if (!service_) {
            draw_auth_error(list, fonts);
            return;
        }

        draw_column(list, fonts, {kGuildX, kTop, kGuildW, kBottom - kTop}, "SERVERS", pane_ == 0);
        draw_column(list, fonts, {kChannelX, kTop, kChannelW, kBottom - kTop}, "CHANNELS", pane_ == 1);
        draw_column(list, fonts, {kChatX, kTop, kChatW, kBottom - kTop}, "CHAT", pane_ == 2);
        draw_guilds(list, fonts);
        draw_channels(list, fonts);
        draw_messages(list, fonts);

        Color status_color = snapshot_.error.empty() ? kMuted : kDanger;
        std::string status = snapshot_.error.empty() ? snapshot_.status : snapshot_.error;
        ui::text(list, fonts.regular, status, 48, 1026, 20, status_color);
        ui::text(list, fonts.regular, "D-Pad navigate   X open   Square compose   Triangle refresh", 1848, 1026, 20, kMuted, gfx::Align::right);

        if (composing_) draw_keyboard(frame);
    }

private:
    void reload_auth() {
        if (service_) service_->stop();
        service_.reset();
        ps5discord::Config cfg;
        std::string error;
        if (!ps5discord::load_config(cfg, &error)) {
            auth_error_ = error;
            return;
        }
        auth_error_.clear();
        service_ = std::make_unique<ps5discord::discord::Service>(std::move(cfg));
        service_->start();
    }

    void clamp_focus() {
        if (snapshot_.guilds.empty()) guild_focus_ = 0;
        else guild_focus_ = std::clamp(guild_focus_, 0, static_cast<int>(snapshot_.guilds.size()) - 1);
        if (snapshot_.channels.empty()) channel_focus_ = 0;
        else channel_focus_ = std::clamp(channel_focus_, 0, static_cast<int>(snapshot_.channels.size()) - 1);
    }

    void draw_auth_error(gfx::DrawList& list, const ui::Fonts& fonts) const {
        list.rounded_rect({300, 280, 1320, 420}, 30, kPanel);
        ui::text(list, fonts.semibold, "Discord session not configured", 960, 370, 42, kText, gfx::Align::center);
        ui::text(list, fonts.regular, "Create this file on the PS5 through FTP:", 960, 448, 28, kMuted, gfx::Align::center);
        ui::text(list, fonts.semibold, "/download0/ps5-discord/token.txt", 960, 505, 30, kAccent, gfx::Align::center);
        ui::text(list, fonts.regular, "Put only the token in the file. Press Triangle to retry.", 960, 566, 26, kText, gfx::Align::center);
        if (!auth_error_.empty()) ui::text(list, fonts.regular, auth_error_, 960, 635, 20, kDanger, gfx::Align::center);
    }

    void draw_column(gfx::DrawList& list, const ui::Fonts& fonts, Rect rect, const char* title, bool active) const {
        list.rounded_rect(rect, 22, active ? Color::rgb(0x353840) : kPanel);
        if (active) {
            list.rounded_rect({rect.x + 3, rect.y + 3, rect.w - 6, 5}, 2, kAccent);
        }
        ui::text(list, fonts.semibold, title, rect.x + 24, rect.y + 48, 20, kMuted);
    }

    void draw_guilds(gfx::DrawList& list, const ui::Fonts& fonts) const {
        float y = kTop + 92;
        int start = std::max(0, guild_focus_ - 8);
        int end = std::min(static_cast<int>(snapshot_.guilds.size()), start + 17);
        for (int i = start; i < end; ++i) {
            const auto& g = snapshot_.guilds[static_cast<std::size_t>(i)];
            bool focused = pane_ == 0 && i == guild_focus_;
            if (focused) list.rounded_rect({kGuildX + 14, y - 34, kGuildW - 28, 48}, 12, kAccent);
            std::string label = context_.fonts.regular.font->fit(g.name, 23, kGuildW - 58);
            ui::text(list, fonts.regular, label, kGuildX + 28, y, 23, focused ? Color::rgb(0xffffff) : kText);
            y += 50;
        }
    }

    void draw_channels(gfx::DrawList& list, const ui::Fonts& fonts) const {
        float y = kTop + 92;
        int start = std::max(0, channel_focus_ - 8);
        int end = std::min(static_cast<int>(snapshot_.channels.size()), start + 17);
        for (int i = start; i < end; ++i) {
            const auto& c = snapshot_.channels[static_cast<std::size_t>(i)];
            bool focused = pane_ == 1 && i == channel_focus_;
            if (focused) list.rounded_rect({kChannelX + 14, y - 34, kChannelW - 28, 48}, 12, Color::rgb(0x404249));
            std::string label = "# " + c.name;
            label = context_.fonts.regular.font->fit(label, 23, kChannelW - 58);
            ui::text(list, fonts.regular, label, kChannelX + 28, y, 23, focused ? kText : kMuted);
            y += 50;
        }
    }

    void draw_messages(gfx::DrawList& list, const ui::Fonts& fonts) const {
        if (snapshot_.selected_channel_id.empty()) {
            ui::text(list, fonts.semibold, "Select a text channel", kChatX + kChatW / 2, 505, 34, kMuted, gfx::Align::center);
            return;
        }
        if (snapshot_.messages.empty()) {
            ui::text(list, fonts.semibold, "No messages", kChatX + kChatW / 2, 505, 34, kMuted, gfx::Align::center);
            return;
        }

        const int visible = 9;
        int last = static_cast<int>(snapshot_.messages.size()) - 1 - message_scroll_;
        int first = std::max(0, last - visible + 1);
        float y = kTop + 102;
        for (int i = first; i <= last; ++i) {
            const auto& m = snapshot_.messages[static_cast<std::size_t>(i)];
            std::string author = m.author.global_name.empty() ? m.author.username : m.author.global_name;
            if (author.empty()) author = "Unknown";
            ui::text(list, fonts.semibold, author, kChatX + 30, y, 23, Color::rgb(0xb5bac1));
            y += 30;
            std::string content = m.content.empty() && !m.attachments.empty() ? "[attachment]" : m.content;
            ui::paragraph(list, fonts.regular, content, kChatX + 30, y, 24, kChatW - 64, 29, kText, 2);
            y += 66;
        }
    }

    void update_keyboard(const InputFrame& input, app::Feedback& feedback) {
        if (input.is_pressed(Action::back)) {
            composing_ = false;
            feedback.play(audio::Cue::back);
            return;
        }
        if (input.is_pressed(Action::west)) {
            if (!compose_.empty()) compose_.pop_back();
            feedback.play(audio::Cue::select);
        }
        if (input.is_pressed(Action::north) && compose_.size() < 1000) {
            compose_.push_back(' ');
            feedback.play(audio::Cue::select);
        }
        if (input.is_pressed(Action::menu)) {
            if (!compose_.empty()) service_->send_message(compose_);
            composing_ = false;
            feedback.play(audio::Cue::select);
            return;
        }

        if (input.nav == Direction::left) key_col_ = (key_col_ + key_width() - 1) % key_width();
        else if (input.nav == Direction::right) key_col_ = (key_col_ + 1) % key_width();
        else if (input.nav == Direction::up) key_row_ = (key_row_ + key_rows() - 1) % key_rows();
        else if (input.nav == Direction::down) key_row_ = (key_row_ + 1) % key_rows();

        if (input.is_pressed(Action::confirm) && compose_.size() < 1000) {
            compose_.push_back(key_at(key_row_, key_col_));
            feedback.play(audio::Cue::select);
        }
    }

    static constexpr int key_rows() { return 5; }
    static constexpr int key_width() { return 10; }
    static char key_at(int row, int col) {
        static constexpr std::array<const char*, 5> rows = {
            "ABCDEFGHIJ", "KLMNOPQRST", "UVWXYZ0123", "456789.,!?", "@#:/_-+()='",
        };
        return rows[static_cast<std::size_t>(row)][col];
    }

    void draw_keyboard(app::Frame& frame) const {
        frame.glass = true;
        auto& list = frame.overlay;
        const auto& fonts = context_.fonts;
        list.rounded_rect({255, 190, 1410, 700}, 34, Color::rgb(0x202225));
        ui::text(list, fonts.semibold, "Write message", 310, 255, 34, kText);
        std::string shown = compose_.empty() ? " " : context_.fonts.regular.font->fit(compose_, 28, 1280);
        list.rounded_rect({310, 292, 1300, 76}, 14, kPanel2);
        ui::text(list, fonts.regular, shown, 335, 342, 28, kText);

        constexpr float key_w = 112;
        constexpr float key_h = 72;
        constexpr float gap = 10;
        float base_x = 340;
        float base_y = 410;
        for (int r = 0; r < key_rows(); ++r) {
            for (int c = 0; c < key_width(); ++c) {
                float x = base_x + c * (key_w + gap);
                float y = base_y + r * (key_h + gap);
                bool focus = r == key_row_ && c == key_col_;
                list.rounded_rect({x, y, key_w, key_h}, 12, focus ? kAccent : Color::rgb(0x3f4147));
                char label[2] = {key_at(r, c), 0};
                ui::text(list, fonts.semibold, label, x + key_w / 2, y + 47, 28, kText, gfx::Align::center);
            }
        }
        ui::text(list, fonts.regular, "X type   Square delete   Triangle space   Options send   Circle cancel", 960, 850, 22, kMuted, gfx::Align::center);
    }

    app::Context& context_;
    std::unique_ptr<ps5discord::discord::Service> service_;
    ps5discord::discord::Snapshot snapshot_;
    std::string auth_error_;
    int pane_ = 0;
    int guild_focus_ = 0;
    int channel_focus_ = 0;
    int message_scroll_ = 0;
    bool composing_ = false;
    std::string compose_;
    int key_row_ = 0;
    int key_col_ = 0;
};

} // namespace

std::unique_ptr<app::Concept> make_ps5discord(app::Context& context) {
    return std::make_unique<DiscordDesign>(context);
}

} // namespace hui::concepts
