#pragma once

#include "CoreMinimal.h"
#include "Components/AudioComponent.h"
#include "SFBattleMusicComponent.generated.h"

class USoundCue;
class USoundWave;

// One local playlist per player. No network audio or changes to the campaign's songs.
UCLASS()
class STRIKEFORGE_API USFBattleMusicComponent : public UAudioComponent
{
    GENERATED_BODY()
public:
    USFBattleMusicComponent();
    void PreparePlaylist();
    void StartPlaylist();
    void StopPlaylist();
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
    void HandleTracksLoaded();
    void HandleTrackFinished(UAudioComponent* FinishedComponent);
    void PlayNextTrack();
    int32 TakeNextTrack();

    UPROPERTY() TArray<TSoftObjectPtr<USoundWave>> Tracks;
    UPROPERTY() TObjectPtr<USoundClass> MusicClass;
    UPROPERTY(Transient) TArray<TObjectPtr<USoundCue>> Playlist;
    TSharedPtr<struct FStreamableHandle> PlaylistLoad;
    TArray<int32> RemainingTracks;
    int32 LastTrack=INDEX_NONE;
    bool bLoadRequested=false;
    bool bPlaylistStarted=false;

    friend class FSFBattleMusicTest;
};
