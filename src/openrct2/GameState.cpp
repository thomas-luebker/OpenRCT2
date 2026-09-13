/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "GameState.h"
#include "platform/AmigaTrace.h"
#include "core/String.hpp"
#include "entity/Peep.h"
#include "peep/GuestPathfinding.h"
#include "platform/Platform.h"

#include "Game.h"
#include "GameStateSnapshots.h"
#include "Input.h"
#include "OpenRCT2.h"
#include "ReplayManager.h"
#include "actions/GameActionRunner.h"
#include "config/Config.h"
#include "drawing/Palette.h"
#include "entity/EntityTweener.h"
#include "entity/PatrolArea.h"
#include "interface/Screenshot.h"
#include "platform/Platform.h"
#include "profiling/Profiling.h"
#include "ride/Vehicle.h"
#include "scenario/Scenario.h"
#include "scenes/editor/EditorScene.h"
#include "scenes/title/TitleScene.h"
#include "scenes/title/TitleSequencePlayer.h"
#include "scripting/ScriptEngine.h"
#include "ui/UiContext.h"
#include "windows/Intent.h"
#include "world/Map.h"
#include "world/MapAnimation.h"
#include "world/Park.h"
#include "world/Scenery.h"

using namespace OpenRCT2::Scripting;

namespace OpenRCT2
{
    static auto _gameState = std::make_unique<GameState_t>();

    GameState_t& getGameState()
    {
        return *_gameState;
    }

    void swapGameState(std::unique_ptr<GameState_t>& otherState)
    {
        _gameState.swap(otherState);
    }

    /**
     * Initialises the map, park etc. basically all S6 data.
     */
    void gameStateInitAll(GameState_t& gameState, const TileCoordsXY& mapSize)
    {
        PROFILED_FUNCTION();

        gInMapInitCode = true;
        gameState.currentTicks = 0;

        MapInit(mapSize);
        Park::Initialise(gameState.park, gameState);
        FinanceInit();
        BannerInit(gameState);
        RideInitAll();
        gameState.entities.resetAllEntities();
        UpdateConsolidatedPatrolAreas();
        ResetDate();
        Weather::reset();
        News::InitQueue(gameState);

        gInMapInitCode = false;

        gameState.nextGuestNumber = 1;

        ContextInit();

        auto sceneryIntent = Intent(INTENT_ACTION_SET_DEFAULT_SCENERY_CONFIG);
        ContextBroadcastIntent(&sceneryIntent);

        auto clipboardIntent = Intent(INTENT_ACTION_CLEAR_TILE_INSPECTOR_CLIPBOARD);
        ContextBroadcastIntent(&clipboardIntent);

        Drawing::LoadPalette();

        CheatsReset();
        ClearRestrictedScenery();

#ifdef ENABLE_SCRIPTING
        auto& scriptEngine = GetContext()->GetScriptEngine();
        scriptEngine.ClearParkStorage();
#endif

        EntityTweener::get().reset();
    }

    /**
     * Function will be called every kGameUpdateTimeMS.
     * It has its own loop which might run multiple updates per call such as
     * when operating as a client it may run multiple updates to catch up with the server tick,
     * another influence can be the game speed setting.
     */
    void gameStateTick()
    {
        PROFILED_FUNCTION();

        // Normal game play will update only once every kGameUpdateTimeMS
        uint32_t numUpdates = 1;

        // 0x006E3AEC // screen_game_process_mouse_input();
        ScreenshotCheck();
        GameHandleKeyboardInput();

        if (GameIsNotPaused() && gPreviewingTitleSequenceInGame)
        {
            auto player = GetContext()->GetUiContext().GetTitleSequencePlayer();
            if (player != nullptr)
            {
                player->Update();
            }
        }

        Network::Update();

        if (Network::GetMode() == Network::Mode::client && Network::GetStatus() == Network::Status::connected
            && Network::GetAuthstatus() == Network::Auth::ok)
        {
            numUpdates = std::clamp<uint32_t>(Network::GetServerTick() - getGameState().currentTicks, 0, 10);
        }
        else
        {
            // Determine how many times we need to update the game
            if (gGameSpeed > 1)
            {
                // Update more often if game speed is above normal.
                numUpdates = 1 << (gGameSpeed - 1);
            }
        }

        bool isPaused = GameIsPaused();
        if (Network::GetMode() == Network::Mode::server && Config::Get().network.pauseServerIfNoClients)
        {
            // If we are headless we always have 1 player (host), pause if no one else is around.
            if (gOpenRCT2Headless && Network::GetNumPlayers() == 1)
            {
                isPaused |= true;
            }
        }

        bool didRunSingleFrame = false;
        if (isPaused)
        {
            if (gDoSingleUpdate && Network::GetMode() == Network::Mode::none)
            {
                didRunSingleFrame = true;
                PauseToggle();
                numUpdates = 1;
            }
            else
            {
                // NOTE: Here are a few special cases that would be normally handled in UpdateLogic.
                // If the game is paused it will not call UpdateLogic at all.
                numUpdates = 0;

                if (Network::GetMode() == Network::Mode::server)
                {
                    // Make sure the client always knows about what tick the host is on.
                    Network::SendTick();
                }

                // Keep updating the money effect even when paused.
                auto& gameState = getGameState();
                gameState.entities.updateMoneyEffect();

                // Post-tick network update
                Network::PostTick();

                // Post-tick game actions.
                GameActions::ProcessQueue(gameState);
                gameState.entities.updateEntitiesSpatialIndex();
            }
        }

        // Network has to always tick.
        if (numUpdates == 0)
        {
            Network::Tick();
        }

        // Update the game one or more times
        for (uint32_t i = 0; i < numUpdates; i++)
        {
            gameStateUpdateLogic();
            if (gGameSpeed == 1)
            {
                if (InputGetState() == InputState::reset || InputGetState() == InputState::normal)
                {
                    if (gInputFlags.has(InputFlag::viewportScrolling))
                    {
                        gInputFlags.unset(InputFlag::viewportScrolling);
                        break;
                    }
                }
                else
                {
                    break;
                }
            }
            // Don't call UpdateLogic again if the game was just paused.
            isPaused |= GameIsPaused();
            if (isPaused)
                break;
        }

        Network::Flush();

        if (!gOpenRCT2Headless)
        {
            gInputFlags.unset(InputFlag::viewportScrolling);
        }

        // Always perform autosave check, even when paused
        if (gLegacyScene != LegacyScene::titleSequence && gLegacyScene != LegacyScene::trackDesigner
            && gLegacyScene != LegacyScene::trackDesignsManager)
        {
            ScenarioAutosaveCheck();
        }

        if (didRunSingleFrame && GameIsNotPaused() && gLegacyScene != LegacyScene::titleSequence)
        {
            PauseToggle();
        }

        gDoSingleUpdate = false;
    }

    static void gameStateCreateStateSnapshot()
    {
        PROFILED_FUNCTION();

        IGameStateSnapshots* snapshots = GetContext()->GetGameStateSnapshots();

        auto& snapshot = snapshots->CreateSnapshot();
        snapshots->Capture(snapshot);
        snapshots->LinkSnapshot(snapshot, getGameState().currentTicks, ScenarioRandState().s0);
    }

    void gameStateUpdateLogic()
    {
        PROFILED_FUNCTION();

        gInUpdateCode = true;

        gScreenAge++;
        if (gScreenAge == 0)
            gScreenAge--;

        GetContext()->GetReplayManager()->Update();

        Network::Tick();

        auto& gameState = getGameState();

        if (Network::GetMode() == Network::Mode::server)
        {
            if (Network::GamestateSnapshotsEnabled())
            {
                gameStateCreateStateSnapshot();
            }

            // Send current tick out.
            Network::SendTick();
        }
        else if (Network::GetMode() == Network::Mode::client)
        {
            // Don't run past the server, this condition can happen during map changes.
            if (Network::GetServerTick() == gameState.currentTicks)
            {
                gInUpdateCode = false;
                return;
            }

            // Check desync.
            bool desynced = Network::CheckDesynchronisation();
            if (desynced)
            {
                // If desync debugging is enabled and we are still connected request the specific game state from server.
                if (Network::GamestateSnapshotsEnabled() && Network::GetStatus() == Network::Status::connected)
                {
                    // Create snapshot from this tick so we can compare it later
                    // as we won't pause the game on this event.
                    gameStateCreateStateSnapshot();

                    Network::RequestGamestateSnapshot();
                }
            }
        }

#ifdef ENABLE_SCRIPTING
        // Stash the current day number before updating the date so that we
        // know if the day number changes on this tick.
        auto day = gameState.date.GetDay();
#endif

#ifdef __amigaos__
        // trace-gated tick profile: ms per 40 ticks for the main subsystems
        static uint32_t tp[10] = {};
        static uint32_t tpTicks = 0;
        uint32_t t0 = Platform::GetTicks();
    #define TP(i)                                                                                                            \
        do                                                                                                                 \
        {                                                                                                                  \
            uint32_t tn = Platform::GetTicks();                                                                            \
            tp[i] += tn - t0;                                                                                              \
            t0 = tn;                                                                                                       \
        } while (0)
#else
    #define TP(i) ((void)0)
#endif
        DateUpdate(gameState);

        ScenarioUpdate(gameState);
        Weather::update();
        MapUpdateTiles();
        TP(0);

        // Temporarily remove provisional paths to prevent peep from interacting with them
        auto removeProvisionalIntent = Intent(INTENT_ACTION_REMOVE_PROVISIONAL_ELEMENTS);
        ContextBroadcastIntent(&removeProvisionalIntent);

        MapUpdatePathWideFlags();
        TP(1);
        PeepUpdateAll();
        TP(2);
        auto restoreProvisionalIntent = Intent(INTENT_ACTION_RESTORE_PROVISIONAL_ELEMENTS);
        ContextBroadcastIntent(&restoreProvisionalIntent);
        VehicleUpdateAll();
        TP(3);
        gameState.entities.updateAllMiscEntities();
        TP(4);
        Ride::updateAll();
        TP(5);

        if (!isInEditorMode())
        {
            auto& park = gameState.park;
            Park::Update(park, gameState);
        }

        ResearchUpdate();
        RideRating::UpdateAll();
        RideMeasurementsUpdate();
        News::UpdateCurrentItem();
        TP(6);

        MapAnimations::InvalidateAndUpdateAll();
        TP(7);
        VehicleSoundsUpdate();
        PeepUpdateCrowdNoise();
        Weather::updateSound();
        TP(8);
#ifdef __amigaos__
        if (++tpTicks % 40 == 0)
        {
            AMIGA_TRACE(String::stdFormat(
                            "tick: per 40 ticks: date/scenario/weather/tiles %u, pathflags %u, peeps %u, vehicles %u, misc %u, rides %u, "
                            "park/research/ratings %u, animations %u, sounds %u ms; %u guests in park",
                            tp[0], tp[1], tp[2], tp[3], tp[4], tp[5], tp[6], tp[7], tp[8], gameState.park.numGuestsInPark)
                            .c_str());
            for (auto& v : tp)
                v = 0;
            {
                static const char* kStates[24] = { "falling",   "one",      "queuingFront", "onRide",    "leavingRide", "walking",
                                                   "queuing",   "entering", "sitting",      "picked",    "patrolling",  "mowing",
                                                   "sweeping",  "entPark",  "leavingPark",  "answering", "fixing",      "buying",
                                                   "watching",  "emptyBin", "usingBin",     "watering",  "toInspect",   "inspecting" };
                std::string line = "tick: guest update ms/calls by state:";
                for (int n = 0; n < 5; n++)
                {
                    int best = -1;
                    for (int st = 0; st < 32; st++)
                        if (gPeepStateN[st] != 0 && (best < 0 || gPeepStateUs[st] > gPeepStateUs[best]))
                            best = st;
                    if (best < 0)
                        break;
                    line += std::string(" ") + (best < 24 ? kStates[best] : "?") + " " + std::to_string(gPeepStateUs[best] / 1000) + "/"
                        + std::to_string(gPeepStateN[best]);
                    gPeepStateN[best] = 0;
                }
                for (int st = 0; st < 32; st++)
                    gPeepStateUs[st] = gPeepStateN[st] = 0;
                AMIGA_TRACE(line.c_str());
                AMIGA_TRACE((std::string("tick: pathfind: ") + std::to_string(PathFinding::gPathStat[0]) + " destinations in "
                             + std::to_string(PathFinding::gPathStat[1] / 1000) + " ms, " + std::to_string(PathFinding::gPathStat[2])
                             + " edge searches, " + std::to_string(PathFinding::gPathStat[3]) + " tiles checked; "
                             + std::to_string(PathFinding::gPathStat[4]) + " searches exhausted their budget with "
                             + std::to_string(PathFinding::gPathStat[5]) + " tiles; " + std::to_string(PathFinding::gPathStat[6]) + " of "
                             + std::to_string(PathFinding::gPathStat[7]) + " direction choices repeat a recent (tile, goal)")
                                .c_str());
                for (auto& v : PathFinding::gPathStat)
                    v = 0;
            }
        }
#endif
#undef TP

        EditorScene::OpenWindowsForCurrentStep();

        // Update windows
        // WindowDispatchUpdateAll();

        gameState.entities.updateEntitiesSpatialIndex();

        // Start autosave timer after update
        if (gLastAutoSaveUpdate == kAutosavePause)
        {
            gLastAutoSaveUpdate = Platform::GetTicks();
        }

        GameActions::ProcessQueue(gameState);

        Network::PostTick();
        Network::Flush();

        gameState.currentTicks++;

#ifdef ENABLE_SCRIPTING
        auto& hookEngine = GetContext()->GetScriptEngine().GetHookEngine();
        hookEngine.Call(HookType::intervalTick, true);

        if (day != gameState.date.GetDay())
        {
            hookEngine.Call(HookType::intervalDay, true);
        }
#endif

        gInUpdateCode = false;
    }
} // namespace OpenRCT2
