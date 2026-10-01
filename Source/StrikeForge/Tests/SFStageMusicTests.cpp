#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Components/AudioComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Game/Audio/SFAudioLibrary.h"
#include "Game/GameMode/SFGameMode.h"
#include "Game/GameState/SFGameState.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/WorldSettings.h"
#include "Sound/AmbientSound.h"
#include "Sound/SoundCue.h"
#include "Sound/SoundNodeWavePlayer.h"
#include "Sound/SoundWave.h"
#include "UObject/Script.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFStageMusicTest,
    "StrikeForge.Audio.StageMusic.LoadingPauseAndLoop",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFStageMusicTest::RunTest(const FString& Parameters)
{
    FEditorScriptExecutionGuard ScriptGuard;
    for (const TCHAR* WavePath : {
        TEXT("/Game/Sound/BGM/StageBgm/Stage1.Stage1"),
        TEXT("/Game/Sound/BGM/Lobby.Lobby"),
        TEXT("")})
    {
        UWorld::InitializationValues Values;
        Values.AllowAudioPlayback(true).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
        UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
            ERHIFeatureLevel::Num, &Values);
        if (!TestNotNull(TEXT("Test world"), World)) return false;
        GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);

        UAudioComponent* OriginalAudio = nullptr;
        USoundWave* OriginalWave = nullptr;
        bool bOriginallyLooping = false;
        if (*WavePath)
        {
            OriginalWave = LoadObject<USoundWave>(nullptr, WavePath);
            if (!TestNotNull(TEXT("Existing stage track"), OriginalWave))
            {
                GEngine->DestroyWorldContext(World);
                World->DestroyWorld(false);
                return false;
            }
            bOriginallyLooping = OriginalWave->bLooping;
            AAmbientSound* Ambient = World->SpawnActor<AAmbientSound>();
            OriginalAudio = Ambient->GetAudioComponent();
            OriginalAudio->SetSound(OriginalWave);
            OriginalAudio->SetVolumeMultiplier(0.6f);
            OriginalAudio->SetPitchMultiplier(1.1f);
        }

        ASFGameMode* Mode = World->SpawnActor<ASFGameMode>();
        Mode->PreInitializeComponents();
        UAudioComponent* Music = Mode->FindComponentByClass<UAudioComponent>();
        ASFGameState* State = World->GetGameState<ASFGameState>();
        TestFalse(TEXT("Music stays silent during loading"), Music->IsPlaying());
        TestTrue(TEXT("Music ignores tutorial and settings pause"), Music->bIsUISound);
        TestFalse(TEXT("Music is not spatialized"), Music->bAllowSpatialization);
        TestFalse(TEXT("Music cannot auto-start before loading ends"), Music->bAutoActivate);
        TestTrue(TEXT("Music has an infinite native loop"), Music->Sound->IsLooping());
        if (OriginalAudio)
        {
            TestFalse(TEXT("Placed BGM cannot produce duplicate playback"), OriginalAudio->bAutoActivate);
            TestFalse(TEXT("Placed BGM is stopped during loading"), OriginalAudio->IsPlaying());
            TestEqual(TEXT("Existing track volume survives"), Music->VolumeMultiplier, 0.6f);
            TestEqual(TEXT("Existing track pitch survives"), Music->PitchMultiplier, 1.1f);
            USoundCue* Cue = Cast<USoundCue>(Music->Sound);
            USoundNodeWavePlayer* Player = Cue ? Cast<USoundNodeWavePlayer>(Cue->FirstNode) : nullptr;
            if (TestNotNull(TEXT("Native looping player"), Player))
            {
                TestTrue(TEXT("The same authored song is retained"), Player->GetSoundWave() == OriginalWave);
            }
            TestEqual(TEXT("Original sound asset is not changed"), OriginalWave->bLooping != 0, bOriginallyLooping);
        }
        TestNotNull(TEXT("BGM volume settings apply to the track"), Music->Sound->GetSoundClass());

        State->BeginRaceDataLoading();
        State->InitializeStage(TEXT("Stage01"), {});
        State->BeginInitialBuildingAssetsLoading();
        State->MarkStageReadyToStart();
        World->GetWorldSettings()->SetPauserPlayerState(World->SpawnActor<APlayerState>());
        TestTrue(TEXT("Test world is paused like the tutorial"), World->IsPaused());
        // This isolated world deliberately has no navigation; camera-boundary errors are expected.
        AddExpectedError(TEXT("NavigationSystem"), EAutomationExpectedErrorFlags::Contains, 1);
        Mode->StartStageAfterLoadingScreen();
        TestTrue(TEXT("Stage starts after loading"), State->GetStageState() == ESFStageState::Playing);
        TestTrue(TEXT("Music starts immediately even while the tutorial pauses the world"), Music->IsPlaying());
        TestTrue(TEXT("Music is playing rather than paused"), Music->GetPlayState() == EAudioComponentPlayState::Playing);
        USFAudioLibrary::SetIgnoreGamePause(Music, true);
        Mode->StartStageAfterLoadingScreen();
        TestTrue(TEXT("Repeated loading completion keeps a single track playing"), Music->IsPlaying());
        Mode->EndPlay(EEndPlayReason::LevelTransition);
        TestFalse(TEXT("Music stops on level exit or retry"), Music->IsPlaying());
        TestFalse(TEXT("Level exit removes the restart handler"), Music->OnAudioFinishedNative.IsBound());
        GEngine->DestroyWorldContext(World);
        World->DestroyWorld(false);
    }
    return true;
}
#endif
