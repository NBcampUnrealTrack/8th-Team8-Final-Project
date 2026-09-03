#include "Game/Audio/SFAudioLibrary.h"

#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Logging/SFLog.h"

void USFAudioLibrary::SetIgnoreGamePause(UAudioComponent* InAudioComponent, bool bInIgnorePause)
{
    if (!IsValid(InAudioComponent))
    {
        SF_LOG(Warning, "[SFAudioLibrary] SetIgnoreGamePause: 오디오 컴포넌트가 유효하지 않다.");
        return;
    }

    InAudioComponent->bIsUISound = bInIgnorePause;

    SF_LOG(Log, "[SFAudioLibrary] %s 를 일시정지 %s 로 설정했다.",
        *InAudioComponent->GetName(),
        bInIgnorePause ? TEXT("무시") : TEXT("적용"));
}

UAudioComponent* USFAudioLibrary::PlayMusic2D(
    const UObject* InWorldContext,
    USoundBase* InSound,
    float InVolumeMultiplier,
    bool bInPersistAcrossLevelTransition)
{
    if (!IsValid(InSound))
    {
        SF_LOG(Warning, "[SFAudioLibrary] PlayMusic2D: 소리가 지정되지 않았다.");
        return nullptr;
    }

    // CreateSound2D 는 만들기만 하고 재생하지 않는다.
    // SpawnSound2D 를 쓰면 이 자리에서 이미 울려 버려서, 플래그를 세우기
    // 전에 일시정지가 걸리면 소리가 끊긴다.
    UAudioComponent* Component = UGameplayStatics::CreateSound2D(
        InWorldContext,
        InSound,
        InVolumeMultiplier,
        1.f,                                // PitchMultiplier
        0.f,                                // StartTime
        nullptr,                            // ConcurrencySettings
        bInPersistAcrossLevelTransition,
        false);                             // bAutoDestroy — 루프라 직접 멈춘다

    if (!IsValid(Component))
    {
        SF_LOG(Error, "[SFAudioLibrary] PlayMusic2D: 오디오 컴포넌트를 만들지 못했다 | %s",
            *InSound->GetName());
        return nullptr;
    }

    Component->bIsUISound = true;
    Component->Play();

    SF_LOG(Log, "[SFAudioLibrary] 배경음 재생 | %s", *InSound->GetName());

    return Component;
}
