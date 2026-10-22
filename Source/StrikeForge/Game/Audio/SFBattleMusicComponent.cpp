#include "SFBattleMusicComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Sound/AmbientSound.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundCue.h"
#include "Sound/SoundNodeWavePlayer.h"
#include "Sound/SoundWave.h"
#include "UObject/ConstructorHelpers.h"

USFBattleMusicComponent::USFBattleMusicComponent()
{
    bAutoActivate=false;
    bAutoDestroy=false;
    bAllowSpatialization=false;
    SetUISound(true);
    Tracks={
        TSoftObjectPtr<USoundWave>(FSoftObjectPath(TEXT("/Game/Sound/BGM/Title.Title"))),
        TSoftObjectPtr<USoundWave>(FSoftObjectPath(TEXT("/Game/Sound/BGM/Lobby.Lobby"))),
        TSoftObjectPtr<USoundWave>(FSoftObjectPath(TEXT("/Game/Sound/BGM/StageBgm/Stage1.Stage1")))};
    static ConstructorHelpers::FObjectFinder<USoundClass> Class(TEXT("/Game/Sound/SC_BGM.SC_BGM"));
    MusicClass=Class.Object;
}

void USFBattleMusicComponent::PreparePlaylist()
{
    if(bLoadRequested||GetNetMode()==NM_DedicatedServer)return;
    if(!IsRegistered())RegisterComponent();
    bLoadRequested=true;
    // Some campaign copies retain ambient music. The local playlist replaces it.
    for(TActorIterator<AAmbientSound> It(GetWorld());It;++It)
        if(auto* Audio=It->GetAudioComponent();Audio&&Audio->Sound&&Audio->Sound->GetPathName().StartsWith(TEXT("/Game/Sound/BGM/")))
        {Audio->bAutoActivate=false;Audio->Stop();}

    OnAudioFinishedNative.AddUObject(this,&ThisClass::HandleTrackFinished);
    TArray<FSoftObjectPath> Paths;
    for(const auto& Track:Tracks)Paths.Add(Track.ToSoftObjectPath());
    PlaylistLoad=UAssetManager::GetStreamableManager().RequestAsyncLoad(Paths,
        FStreamableDelegate::CreateUObject(this,&ThisClass::HandleTracksLoaded));
}

void USFBattleMusicComponent::HandleTracksLoaded()
{
    if(!Playlist.IsEmpty())return;
    for(const auto& Track:Tracks)
    {
        auto* Wave=Track.Get();
        if(!Wave)
        {UE_LOG(LogTemp,Warning,TEXT("[PVPBGM] Missing track: %s"),*Track.ToString());continue;}
        // A node overrides an authored wave's infinite loop without modifying the asset.
        auto* Cue=NewObject<USoundCue>(this);
        auto* Player=NewObject<USoundNodeWavePlayer>(Cue);
        Player->SetSoundWave(Wave);
        Player->bLooping=false;
        Cue->FirstNode=Player;
        Cue->Duration=Wave->Duration;
        Cue->VolumeMultiplier=1.f;
        Cue->SoundClassObject=MusicClass;
        Cue->CacheAggregateValues();
        Playlist.Add(Cue);
    }
    UE_LOG(LogTemp,Display,TEXT("[PVPBGM] Playlist ready: tracks=%d"),Playlist.Num());
    if(bPlaylistStarted)PlayNextTrack();
}

void USFBattleMusicComponent::StartPlaylist()
{
    if(bPlaylistStarted||GetNetMode()==NM_DedicatedServer)return;
    bPlaylistStarted=true;
    PreparePlaylist();
    if(!Playlist.IsEmpty())PlayNextTrack();
}

int32 USFBattleMusicComponent::TakeNextTrack()
{
    if(Playlist.IsEmpty())return INDEX_NONE;
    if(RemainingTracks.IsEmpty())
    {
        for(int32 I=0;I<Playlist.Num();++I)RemainingTracks.Add(I);
        for(int32 I=RemainingTracks.Num()-1;I>0;--I)RemainingTracks.Swap(I,FMath::RandRange(0,I));
        // Shuffle every full pass, and avoid repeating the boundary song.
        if(RemainingTracks.Num()>1&&RemainingTracks.Last()==LastTrack)RemainingTracks.Swap(0,RemainingTracks.Num()-1);
    }
    LastTrack=RemainingTracks.Pop(EAllowShrinking::No);
    return LastTrack;
}

void USFBattleMusicComponent::PlayNextTrack()
{
    if(!bPlaylistStarted||!GetWorld()||GetWorld()->bIsTearingDown)return;
    const int32 Next=TakeNextTrack();
    if(Next==INDEX_NONE)return;
    SetSound(Playlist[Next]);
    SetUISound(true);
    Play();
    const auto* Player=Cast<USoundNodeWavePlayer>(Playlist[Next]->FirstNode);
    UE_LOG(LogTemp,Display,TEXT("[PVPBGM] Playing %s duration=%.2f"),*GetPathNameSafe(Player?Player->GetSoundWave():nullptr),Playlist[Next]->GetDuration());
}

void USFBattleMusicComponent::HandleTrackFinished(UAudioComponent* FinishedComponent)
{
    if(FinishedComponent==this&&bPlaylistStarted)PlayNextTrack();
}

void USFBattleMusicComponent::StopPlaylist()
{
    bPlaylistStarted=false;
    if(IsPlaying())Stop();
}

void USFBattleMusicComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    StopPlaylist();
    OnAudioFinishedNative.RemoveAll(this);
    if(PlaylistLoad)PlaylistLoad->CancelHandle();
    PlaylistLoad.Reset();
    Super::EndPlay(Reason);
}
