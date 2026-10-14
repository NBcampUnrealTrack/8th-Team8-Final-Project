#include "Game/Audio/SFUnitVoiceSubsystem.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Units/Unit/UnitBase.h"
#include "Units/Commander/SFCommander.h"
#include "ActorComponent/Combat/CombatTargetComponent.h"
#include "InGame/Controller/SFInGamePlayerController.h"
#include "ActorComponent/Stat/HealthComponent.h"

void USFUnitVoiceSubsystem::RequestVoice(AActor* Speaker, USoundBase* Sound, bool bCommand)
{
    const auto* Target = IsValid(Speaker) ? Speaker->FindComponentByClass<UCombatTargetComponent>() : nullptr;
    const auto* PC = Cast<ASFInGamePlayerController>(GetWorld()->GetFirstPlayerController());
    if ((!Cast<AUnitBase>(Speaker) && !Cast<ASFCommander>(Speaker)) || !Target || !Sound || !PC || !PC->IsLocalController() ||
        Target->GetTeamIndex() != PC->GetMyTeamIndex() || !Target->IsAlive()) return;
    // A busy response consumes no future input: the player must click again after it ends.
    if (IsValid(ActiveVoice) && ActiveVoice->IsPlaying()) return;
    if (bCommand)
    {
        const double Now = FPlatformTime::Seconds();
        if (LastCommandTime >= 0. && Now - LastCommandTime < .2) return;
        LastCommandTime = Now;
    }
    StartVoice(Speaker, Sound);
}

void USFUnitVoiceSubsystem::StartVoice(AActor* Speaker, USoundBase* Sound)
{
    ActiveVoice = UGameplayStatics::CreateSound2D(this, Sound, 1.f, 1.f, 0.f, nullptr, false, true);
    if (!ActiveVoice) return;
    ActiveVoice->SetUISound(true);
    ActiveVoice->OnAudioFinishedNative.AddUObject(this, &ThisClass::VoiceFinished);
    ActiveVoice->Play();
}

void USFUnitVoiceSubsystem::VoiceFinished(UAudioComponent* Component)
{
    if (Component != ActiveVoice) return;
    Component->OnAudioFinishedNative.RemoveAll(this);
    ActiveVoice = nullptr;
}

void USFUnitVoiceSubsystem::Deinitialize()
{
    if (IsValid(ActiveVoice))
    {
        ActiveVoice->OnAudioFinishedNative.RemoveAll(this);
        ActiveVoice->Stop();
    }
    ActiveVoice = nullptr;
    Super::Deinitialize();
}
