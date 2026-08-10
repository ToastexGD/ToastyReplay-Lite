#include "Engine.hpp"
#include "RandomSeed.hpp"

#include "../compat/InputPrecision.hpp"
#include "../replay/TtrlFingerprint.hpp"
#include "../replay/TtrlStorage.hpp"
#include "../timing/TpsBypass.hpp"
#include "../ui/Notifications.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/PlayLayer.hpp>
#include <fmt/format.h>

#include <algorithm>
#include <utility>

using namespace geode::prelude;

namespace toasty::engine {
    namespace {
        std::string s_selectedReplay;

        void resetAttempt(Session& session) {
            session.tick = 0;
            session.nextInput = 0;
            session.nextFix = 0;
            session.heldInputs.fill(false);
            if (session.mode == Mode::Record) {
                session.recording.inputs.clear();
                session.recording.frameFixes.clear();
            }
        }

        Replay finishRecording(Session& session) {
            auto replay = std::move(session.recording);
            replay.tps = {static_cast<uint64_t>(session.recordingRate), 1};
            replay.mode = session.platformer ? replay::PlayMode::Platformer : replay::PlayMode::Normal;
            replay.levelId = session.levelId;
            replay.levelRevision = session.levelRevision;
            replay.levelFingerprint = replay::ttrl::fingerprintLevelData(session.levelData);
            replay.tickCount = std::max<uint64_t>(session.tick + (session.processingTick ? 1 : 0), 1);
            session.recording = {};
            session.mode = Mode::Off;
            return replay;
        }

        void restorePlayback(Session& session, PlayLayer* layer) {
            session.mode = Mode::Off;
            if (layer) {
                for (size_t index = 0; index < session.heldInputs.size(); ++index) {
                    if (!session.heldInputs[index]) {
                        continue;
                    }
                    auto button = static_cast<int>(index % 3) + 1;
                    auto isPlayer1 = index < 3;
                    layer->handleButton(false, button, isPlayer1);
                }
            }
            session.playback.reset();
            session.nextInput = 0;
            session.nextFix = 0;
            session.tick = 0;
            session.heldInputs.fill(false);
            if (session.tpsOverride) {
                toasty::tps::endReplayOverride();
                session.tpsOverride = false;
            }
            toasty::compat::endSession();
            if (layer && session.changedTestMode) {
                layer->m_isTestMode = session.previousTestMode;
            }
            session.changedTestMode = false;
        }

        std::optional<std::string> validateReplay(Session const& session, Replay& replay) {
            if (replay.mode == replay::PlayMode::Platformer && !session.platformer) {
                return "The replay is for platformer mode";
            }
            if (replay.mode == replay::PlayMode::Normal && session.platformer) {
                return "The replay is for normal mode";
            }
            if (replay.levelId != 0 && session.levelId != 0 && replay.levelId != session.levelId) {
                return "The replay is for a different level";
            }
            if (replay.levelRevision != 0 && replay.levelRevision != session.levelRevision) {
                return "The replay is for a different level version";
            }
            if (replay.levelFingerprint != 0 &&
                !replay::ttrl::matchesLevelFingerprint(replay.levelFingerprint, session.levelData)) {
                return "The replay does not match this level";
            }
            auto rate = replay.tps.normalized();
            if (!rate || rate->denominator != 1 || rate->numerator < toasty::tps::Minimum ||
                rate->numerator > toasty::tps::Maximum) {
                return "The replay TPS is not supported";
            }
            replay.tps = *rate;
            return std::nullopt;
        }
    } // namespace

    Session::~Session() {
        if (tpsOverride) {
            toasty::tps::endReplayOverride();
        }
        toasty::compat::endSession();
    }

    Mode mode() {
        if (auto session = activeSession()) {
            return session->mode;
        }
        return Mode::Off;
    }

    bool recording() {
        return mode() == Mode::Record;
    }

    bool playing() {
        return mode() == Mode::Play;
    }

    bool startRecording() {
        auto layer = PlayLayer::get();
        auto session = activeSession();
        if (!layer || !session) {
            FLAlertLayer::create("No Level", "Enter a level before recording", "OK")->show();
            return false;
        }
        if (session->mode == Mode::Play) {
            restorePlayback(*session, layer);
        }
        toasty::compat::beginSession();
        session->mode = Mode::Off;
        session->recording = {};
        if (toasty::seed::enabled()) {
            session->recording.seed = toasty::seed::value();
            toasty::seed::apply(layer, *session->recording.seed);
        }
        layer->resetLevelFromStart();
        if (session->recording.seed) {
            toasty::seed::apply(layer, *session->recording.seed);
        }
        session->recordingRate = toasty::tps::effectiveRate();
        session->mode = Mode::Record;
        resetAttempt(*session);
        toasty::notifications::show("Recording started", NotificationIcon::Info);
        return true;
    }

    bool stopRecording(bool save) {
        auto session = activeSession();
        if (!session || session->mode != Mode::Record) {
            return false;
        }
        toasty::compat::endSession();
        if (!save) {
            session->recording = {};
            session->mode = Mode::Off;
            return true;
        }

        auto replay = finishRecording(*session);
        replay::ttrl::Storage storage(replay::ttrl::defaultReplayDirectory());
        auto result = storage.save(session->levelName, replay);
        if (result.isErr()) {
            auto message = replay::ttrl::describe(result.unwrapErr());
            log::error("Failed to save replay: {}", message);
            toasty::notifications::show("Failed to save replay", NotificationIcon::Error);
            return false;
        }
        auto name = result.unwrap();
        setSelectedReplay(name);
        log::info("Saved replay {}", name);
        toasty::notifications::show(fmt::format("Saved {}", name), NotificationIcon::Success);
        return true;
    }

    bool loadReplayAndBegin(std::string name) {
        auto layer = PlayLayer::get();
        auto session = activeSession();
        if (!layer || !session) {
            FLAlertLayer::create("No Level", "Enter a level to play a replay", "OK")->show();
            return false;
        }
        if (session->mode == Mode::Record) {
            stopRecording(true);
        }
        if (session->mode == Mode::Play) {
            restorePlayback(*session, layer);
        }
        replay::ttrl::Storage storage(replay::ttrl::defaultReplayDirectory());
        auto loaded = storage.load(name);
        if (loaded.isErr()) {
            auto message = replay::ttrl::describe(loaded.unwrapErr());
            log::error("Failed to load replay: {}", message);
            FLAlertLayer::create("Load Failed", message, "OK")->show();
            return false;
        }

        auto replay = std::move(loaded.unwrap());
        if (auto error = validateReplay(*session, replay)) {
            log::error("Replay rejected: {}", *error);
            FLAlertLayer::create("Replay Rejected", *error, "OK")->show();
            return false;
        }
        auto replayRate = static_cast<int64_t>(replay.tps.numerator);
        auto needsOverride = toasty::tps::effectiveRate() != replayRate;
        if (needsOverride && !toasty::tps::beginReplayOverride(replayRate)) {
            auto reason = toasty::tps::unavailableReason();
            if (reason.empty()) {
                reason = "The replay TPS could not be applied";
            }
            FLAlertLayer::create("TPS Bypass Unavailable", reason, "OK")->show();
            return false;
        }

        toasty::compat::beginSession();
        session->previousTestMode = layer->m_isTestMode;
        session->changedTestMode = true;
        session->tpsOverride = needsOverride;
        layer->m_isTestMode = true;
        session->playback = std::move(replay);
        session->mode = Mode::Off;
        if (session->playback->seed) {
            toasty::seed::apply(layer, *session->playback->seed);
        }
        layer->resetLevel();
        if (session->playback->seed) {
            toasty::seed::apply(layer, *session->playback->seed);
        }
        session->mode = Mode::Play;
        resetAttempt(*session);
        setSelectedReplay(std::move(name));
        log::info("Loaded replay {} with {} inputs", selectedReplay(), session->playback->inputs.size());
        toasty::notifications::show("Replay started", NotificationIcon::Info);
        return true;
    }

    void stopPlayback() {
        auto session = activeSession();
        if (!session || session->mode != Mode::Play) {
            return;
        }
        restorePlayback(*session, PlayLayer::get());
    }

    void togglePlayback() {
        if (playing()) {
            stopPlayback();
            toasty::notifications::show("Replay stopped", NotificationIcon::Info);
            return;
        }
        auto selected = selectedReplay();
        if (!selected.empty()) {
            loadReplayAndBegin(selected);
            return;
        }
        replay::ttrl::Storage storage(replay::ttrl::defaultReplayDirectory());
        auto files = storage.list();
        if (files.isErr()) {
            FLAlertLayer::create("Load Failed", replay::ttrl::describe(files.unwrapErr()), "OK")
                ->show();
            return;
        }
        auto names = std::move(files.unwrap());
        if (names.empty()) {
            FLAlertLayer::create("No Replays", "Record a replay first", "OK")->show();
            return;
        }
        loadReplayAndBegin(names.front());
    }

    void setSelectedReplay(std::string name) {
        s_selectedReplay = std::move(name);
    }

    std::string const& selectedReplay() {
        return s_selectedReplay;
    }
} // namespace toasty::engine
