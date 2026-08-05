#include "Engine.hpp"

#include "../replay/TtrlCodec.hpp"
#include "../timing/TpsBypass.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/CheckpointObject.hpp>
#include <Geode/loader/SettingV3.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/ui/Notification.hpp>

#include <algorithm>
#include <array>
#include <unordered_map>

using namespace geode::prelude;

using toasty::engine::Mode;
using toasty::engine::Session;
using toasty::replay::FrameFix;
using toasty::replay::InputButton;
using toasty::replay::InputPlayer;

namespace {
    bool s_frameFixes = false;

    size_t heldIndex(InputButton button, InputPlayer player) {
        auto playerOffset = player == InputPlayer::Player2 ? 3 : 0;
        return playerOffset + static_cast<size_t>(button) - 1;
    }

    void rewind(Session& session) {
        session.tick = 0;
        session.nextInput = 0;
        session.nextFix = 0;
        session.heldInputs.fill(false);
        if (session.mode == Mode::Record) {
            session.recording.inputs.clear();
            session.recording.frameFixes.clear();
        }
    }

    void applyFix(PlayLayer* layer, FrameFix const& fix) {
        auto player = fix.player == InputPlayer::Player2 ? layer->m_player2 : layer->m_player1;
        if (!player) {
            return;
        }
        player->setPosition({fix.x, fix.y});
        player->setRotation(fix.rotation);
        player->m_yVelocity = fix.verticalVelocity;
    }

    void captureFix(Session& session, PlayerObject* player, InputPlayer inputPlayer) {
        if (!player || session.recording.frameFixes.size() >=
                           toasty::replay::ttrl::codec::MaximumFrameFixes) {
            return;
        }
        session.recording.frameFixes.push_back({.afterTick = session.tick,
                                                .player = inputPlayer,
                                                .x = player->getPositionX(),
                                                .y = player->getPositionY(),
                                                .rotation = player->getRotation(),
                                                .verticalVelocity = player->m_yVelocity});
    }
} // namespace

class $modify(ToastyReplayGameLayer, GJBaseGameLayer) {
    void handleButton(bool down, int button, bool isPlayer1) {
        auto session = toasty::engine::activeSession();
        if (session && session->mode == Mode::Play && !session->resetting &&
            !session->acceptingPlaybackInput) {
            return;
        }
        if (session && session->mode == Mode::Record && !session->resetting &&
            button >= static_cast<int>(InputButton::Jump) &&
            button <= static_cast<int>(InputButton::Right)) {
            auto inputButton = static_cast<InputButton>(button);
            auto inputPlayer = isPlayer1 ? InputPlayer::Player1 : InputPlayer::Player2;
            session->heldInputs[heldIndex(inputButton, inputPlayer)] = down;
            if (session->recording.inputs.size() <
                toasty::replay::ttrl::codec::MaximumInputEvents) {
                session->recording.inputs.push_back({.beforeTick = session->tick,
                                                     .button = inputButton,
                                                     .player = inputPlayer,
                                                     .pressed = down});
            }
        }
        GJBaseGameLayer::handleButton(down, button, isPlayer1);
    }

    void processCommands(float dt, bool isHalfTick, bool isLastTick) {
        auto session = toasty::engine::activeSession();
        auto layer = PlayLayer::get();
        if (!session || !layer || static_cast<GJBaseGameLayer*>(layer) !=
                                      static_cast<GJBaseGameLayer*>(this)) {
            GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);
            return;
        }

        session->processingTick = true;
        if (session->mode == Mode::Play && session->playback) {
            auto& replay = *session->playback;
            while (session->nextInput < replay.inputs.size() &&
                   replay.inputs[session->nextInput].beforeTick <= session->tick) {
                auto const& input = replay.inputs[session->nextInput++];
                session->acceptingPlaybackInput = true;
                this->handleButton(input.pressed,
                                   static_cast<int>(input.button),
                                   input.player == InputPlayer::Player1);
                session->acceptingPlaybackInput = false;
                session->heldInputs[heldIndex(input.button, input.player)] = input.pressed;
            }
        }

        GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);

        if (session->mode == Mode::Play && session->playback) {
            auto& replay = *session->playback;
            if (s_frameFixes) {
                while (session->nextFix < replay.frameFixes.size() &&
                       replay.frameFixes[session->nextFix].afterTick == session->tick) {
                    applyFix(layer, replay.frameFixes[session->nextFix++]);
                }
            }
            session->tick++;
            if (session->tick >= replay.tickCount) {
                toasty::engine::stopPlayback();
                Notification::create("Replay finished", NotificationIcon::Success)->show();
            }
        } else if (session->mode == Mode::Record) {
            if (s_frameFixes) {
                captureFix(*session, layer->m_player1, InputPlayer::Player1);
                if (layer->m_player2 && layer->m_player2 != layer->m_player1) {
                    captureFix(*session, layer->m_player2, InputPlayer::Player2);
                }
            }
            session->tick++;
        }
        session->processingTick = false;
    }
};

class $modify(ToastyReplayPlayLayer, PlayLayer) {
    struct PracticeCheckpoint {
        uint64_t tick = 0;
        std::array<bool, 6> heldInputs = {};
    };

    struct Fields {
        Session session;
        std::unordered_map<CheckpointObject*, PracticeCheckpoint> checkpoints;
        bool resettingLevel = false;
        bool loadedCheckpoint = false;
    };

  public:
    Session* replaySession() {
        return &m_fields->session;
    }

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) {
            return false;
        }
        auto fields = m_fields.self();
        fields->session.levelId = static_cast<uint64_t>(level->m_levelID.value());
        fields->session.levelRevision = static_cast<uint64_t>(level->m_levelVersion);
        fields->session.levelName = std::string(level->m_levelName);
        fields->session.levelData = std::string(level->m_levelString);
        fields->session.platformer = m_isPlatformer;
        return true;
    }

    void resetLevel() {
        auto fields = m_fields.self();
        auto wasResetting = fields->session.resetting;
        fields->resettingLevel = true;
        fields->loadedCheckpoint = false;
        fields->session.resetting = true;
        PlayLayer::resetLevel();
        fields->session.resetting = wasResetting;
        fields->resettingLevel = false;
        if (!fields->loadedCheckpoint) {
            rewind(fields->session);
        }
    }

    void resetLevelFromStart() {
        auto fields = m_fields.self();
        auto wasResetting = fields->session.resetting;
        fields->session.resetting = true;
        fields->checkpoints.clear();
        PlayLayer::resetLevelFromStart();
        fields->session.resetting = wasResetting;
        rewind(fields->session);
    }

    void storeCheckpoint(CheckpointObject* checkpoint) {
        PlayLayer::storeCheckpoint(checkpoint);
        auto fields = m_fields.self();
        if (checkpoint && fields->session.mode == Mode::Record) {
            fields->checkpoints[checkpoint] = {
                .tick = fields->session.tick,
                .heldInputs = fields->session.heldInputs,
            };
        }
    }

    void loadFromCheckpoint(CheckpointObject* checkpoint) {
        auto fields = m_fields.self();
        auto found = fields->checkpoints.find(checkpoint);
        if (fields->session.mode != Mode::Record || found == fields->checkpoints.end()) {
            PlayLayer::loadFromCheckpoint(checkpoint);
            return;
        }

        auto saved = found->second;
        auto liveInputs = fields->session.heldInputs;
        auto& inputs = fields->session.recording.inputs;
        auto inputStart = std::lower_bound(inputs.begin(),
                                           inputs.end(),
                                           saved.tick,
                                           [](auto const& input, uint64_t tick) {
                                               return input.beforeTick < tick;
                                           });
        inputs.erase(inputStart, inputs.end());

        auto& fixes = fields->session.recording.frameFixes;
        auto fixStart = std::lower_bound(fixes.begin(),
                                         fixes.end(),
                                         saved.tick,
                                         [](auto const& fix, uint64_t tick) {
                                             return fix.afterTick < tick;
                                         });
        fixes.erase(fixStart, fixes.end());

        fields->session.tick = saved.tick;
        fields->session.nextInput = 0;
        fields->session.nextFix = 0;
        fields->session.heldInputs = saved.heldInputs;

        auto wasResetting = fields->session.resetting;
        fields->session.resetting = true;
        PlayLayer::loadFromCheckpoint(checkpoint);

        for (size_t index = 0; index < liveInputs.size(); ++index) {
            if (saved.heldInputs[index] == liveInputs[index]) {
                continue;
            }
            auto button = static_cast<InputButton>(index % 3 + 1);
            auto player = index < 3 ? InputPlayer::Player1 : InputPlayer::Player2;
            if (inputs.size() < toasty::replay::ttrl::codec::MaximumInputEvents) {
                inputs.push_back({.beforeTick = saved.tick,
                                  .button = button,
                                  .player = player,
                                  .pressed = liveInputs[index]});
            }
            this->handleButton(liveInputs[index],
                               static_cast<int>(button),
                               player == InputPlayer::Player1);
        }

        fields->session.heldInputs = liveInputs;
        fields->session.resetting = wasResetting;
        if (fields->resettingLevel) {
            fields->loadedCheckpoint = true;
        }
    }

    void removeCheckpoint(bool first) {
        PlayLayer::removeCheckpoint(first);
        auto fields = m_fields.self();
        for (auto it = fields->checkpoints.begin(); it != fields->checkpoints.end();) {
            if (!m_checkpointArray || !m_checkpointArray->containsObject(it->first)) {
                it = fields->checkpoints.erase(it);
            } else {
                ++it;
            }
        }
    }

    void removeAllCheckpoints() {
        PlayLayer::removeAllCheckpoints();
        m_fields->checkpoints.clear();
    }

    void destroyPlayer(PlayerObject* player, GameObject* object) {
        PlayLayer::destroyPlayer(player, object);
        auto fields = m_fields.self();
        if (fields->session.mode == Mode::Play && player && player->m_isDead) {
            toasty::engine::stopPlayback();
            Notification::create("Replay stopped after death", NotificationIcon::Warning)->show();
        }
    }

    void togglePracticeMode(bool practiceMode) {
        auto fields = m_fields.self();
        if (practiceMode && fields->session.mode == Mode::Play) {
            toasty::engine::stopPlayback();
        }
        PlayLayer::togglePracticeMode(practiceMode);
    }

    void levelComplete() {
        auto fields = m_fields.self();
        auto wasPlaying = fields->session.mode == Mode::Play;
        auto previousTestMode = fields->session.previousTestMode;
        auto changedTestMode = fields->session.changedTestMode;
        if (fields->session.mode == Mode::Record) {
            toasty::engine::stopRecording(true);
        } else if (wasPlaying) {
            toasty::engine::stopPlayback();
            if (changedTestMode) {
                m_isTestMode = true;
            }
        }
        PlayLayer::levelComplete();
        if (wasPlaying && changedTestMode) {
            m_isTestMode = previousTestMode;
        }
    }

    void onQuit() {
        auto fields = m_fields.self();
        if (fields->session.mode == Mode::Record) {
            toasty::engine::stopRecording(
                Mod::get()->getSavedValue<bool>("auto-save-macros", true));
        } else if (fields->session.mode == Mode::Play) {
            toasty::engine::stopPlayback();
        }
        PlayLayer::onQuit();
    }
};

namespace toasty::engine {
    Session* activeSession() {
        auto layer = PlayLayer::get();
        if (!layer) {
            return nullptr;
        }
        return static_cast<ToastyReplayPlayLayer*>(layer)->replaySession();
    }
} // namespace toasty::engine

$on_mod(Loaded) {
    s_frameFixes = Mod::get()->getSettingValue<bool>("frame-fixes");
    listenForSettingChanges<bool>("frame-fixes", [](bool value) { s_frameFixes = value; });
}
