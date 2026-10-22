#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Game/Audio/SFBattleMusicComponent.h"
#include "Sound/SoundCue.h"
#include "Sound/SoundNodeWavePlayer.h"
#include "Sound/SoundWave.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFBattleMusicTest,"StrikeForge.Audio.BattleMusic.ShuffleAndAssets",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)

bool FSFBattleMusicTest::RunTest(const FString&)
{
    auto* Music=NewObject<USFBattleMusicComponent>();
    Music->AddToRoot();
    TArray<bool> OriginalLooping;
    for(auto& Track:Music->Tracks)
    {
        auto* Wave=Track.LoadSynchronous();
        if(!TestNotNull(TEXT("Existing BGM asset"),Wave)){Music->RemoveFromRoot();return false;}
        OriginalLooping.Add(Wave->bLooping);
    }
    Music->HandleTracksLoaded();
    TestEqual(TEXT("Three distinct existing songs"),Music->Playlist.Num(),3);
    TestFalse(TEXT("Music cannot auto-start during loading"),Music->bAutoActivate);
    TestFalse(TEXT("Music has no positional attenuation"),Music->bAllowSpatialization);
    TestTrue(TEXT("Menu pauses do not silence music"),Music->bIsUISound);
    for(int32 I=0;I<Music->Playlist.Num();++I)
    {
        auto* Cue=Music->Playlist[I].Get();
        auto* Player=Cast<USoundNodeWavePlayer>(Cue->FirstNode);
        TestTrue(TEXT("BGM settings control every song"),Cue->GetSoundClass()==Music->MusicClass&&Music->MusicClass!=nullptr);
        TestFalse(TEXT("A song finishes so the next song can start"),Cue->IsLooping());
        TestTrue(TEXT("Each song has its real duration"),Cue->GetDuration()>1&&Cue->GetDuration()<INDEFINITELY_LOOPING_DURATION);
        if(TestNotNull(TEXT("Non-looping cue player"),Player))TestTrue(TEXT("Original song retained"),Player->GetSoundWave()==Music->Tracks[I].Get());
        TestEqual(TEXT("Original looping flag untouched"),Music->Tracks[I]->bLooping!=0,OriginalLooping[I]);
    }
    int32 Previous=INDEX_NONE;
    for(int32 Pass=0;Pass<100;++Pass)
    {
        TSet<int32> Heard;
        for(int32 I=0;I<Music->Playlist.Num();++I)
        {
            const int32 Next=Music->TakeNextTrack();
            TestTrue(TEXT("Consecutive songs differ across shuffle boundaries"),Next!=Previous);
            TestFalse(TEXT("Each pass plays every song once"),Heard.Contains(Next));
            Heard.Add(Next);Previous=Next;
        }
        TestEqual(TEXT("Every song heard in each pass"),Heard.Num(),Music->Playlist.Num());
    }
    Music->bPlaylistStarted=true;
    Music->StopPlaylist();
    Music->HandleTrackFinished(Music);
    TestFalse(TEXT("A completion after stopping cannot restart the playlist"),Music->bPlaylistStarted);
    Music->RemoveFromRoot();
    return true;
}
#endif
