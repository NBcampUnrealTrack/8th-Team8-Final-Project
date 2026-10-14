#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SFUnitVoiceSubsystem.generated.h"
class UAudioComponent;
class USoundBase;

/** One local unit voice at a time. Requests during playback are discarded, never queued. */
UCLASS()
class STRIKEFORGE_API USFUnitVoiceSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    void RequestVoice(AActor* Speaker, USoundBase* Sound, bool bCommand = false);
    virtual void Deinitialize() override;
    UFUNCTION(BlueprintPure) UAudioComponent* GetActiveVoice() const { return ActiveVoice; }
private:
    void StartVoice(AActor* Speaker, USoundBase* Sound);
    void VoiceFinished(UAudioComponent* Component);
    UPROPERTY(Transient) TObjectPtr<UAudioComponent> ActiveVoice;
    double LastCommandTime = -1.;
};
