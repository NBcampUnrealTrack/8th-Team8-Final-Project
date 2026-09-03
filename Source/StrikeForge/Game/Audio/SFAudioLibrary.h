#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SFAudioLibrary.generated.h"

class UAudioComponent;
class USoundBase;

/**
 * 일시정지 중에도 계속 나와야 하는 소리를 다루는 헬퍼.
 *
 * 게임을 멈추면 오디오 컴포넌트도 함께 멈춘다. bIsUISound 가 켜진 것만
 * 예외다. 튜토리얼 창은 게임을 일시정지시키므로, 그동안에도 들려야 하는
 * 배경음은 반드시 이 플래그가 켜져 있어야 한다.
 *
 * 이 플래그는 엔진 버전에 따라 블루프린트에 노출되기도 하고 아니기도 해서,
 * 어느 쪽이든 되도록 여기에 묶어 둔다.
 */
UCLASS()
class STRIKEFORGE_API USFAudioLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /**
     * 이미 재생 중인 오디오 컴포넌트를 일시정지 영향에서 빼낸다.
     *
     * 레벨에 배치한 Ambient Sound 나 Spawn Sound 2D 가 돌려준 컴포넌트에
     * 쓴다. 재생 중에 켜도 즉시 적용된다.
     */
    UFUNCTION(BlueprintCallable, Category = "SF|Audio")
    static void SetIgnoreGamePause(UAudioComponent* InAudioComponent, bool bInIgnorePause = true);

    /**
     * 일시정지에도 끊기지 않는 2D 배경음을 재생한다.
     *
     * Spawn Sound 2D 와 달리 재생을 시작하기 전에 플래그를 세운다.
     * 먼저 울리고 나서 켜면 그 사이에 일시정지가 걸렸을 때 이미 끊긴다.
     *
     * bAutoDestroy 를 켜지 않으므로 돌려받은 컴포넌트를 들고 있다가
     * Stop 을 부르면 된다. 배경음은 보통 루프라 스스로 끝나지 않는다.
     *
     * @return 재생 중인 컴포넌트. 실패하면 nullptr
     */
    UFUNCTION(BlueprintCallable, Category = "SF|Audio",
        meta = (WorldContext = "InWorldContext", AdvancedDisplay = "bInPersistAcrossLevelTransition"))
    static UAudioComponent* PlayMusic2D(
        const UObject* InWorldContext,
        USoundBase* InSound,
        float InVolumeMultiplier = 1.f,
        bool bInPersistAcrossLevelTransition = false);
};
