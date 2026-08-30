#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SFTutorialGhostActor.generated.h"

class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * "여기에 지으세요" 를 자리에 세워 보여주는 반투명 모형.
 *
 * 말로 설명하는 것보다 자리에 회색 모형을 세워두는 편이 훨씬 빠르다.
 * 플레이어는 그 자리에 겹쳐 놓기만 하면 된다.
 *
 * ── 왜 머티리얼을 C++ 에서 안 정하는가 ──────────────────
 *
 * 어떤 회색이 어울리는지는 아트 쪽 판단이고, 반투명 정도는 맵 조명에 따라
 * 달라진다. 이 클래스는 메시를 세우고 콜리전을 끄는 것까지만 하고,
 * 머티리얼은 블루프린트 자식 클래스에서 지정한다.
 *
 * USFTutorialSubsystem::GhostActorClass 에 그 블루프린트를 넣어두면
 * 안내가 알아서 스폰하고 지운다.
 */
UCLASS(Blueprintable)
class STRIKEFORGE_API ASFTutorialGhostActor : public AActor
{
    GENERATED_BODY()

public:
    ASFTutorialGhostActor();

    /**
     * 세울 메시와 그 상대 트랜스폼을 지정한다. 스폰 직후 서브시스템이 부른다.
     *
     * 상대 트랜스폼을 함께 받는 이유: 건물 BP 안의 메시 컴포넌트는 대개
     * 돌아가 있거나 줄어들어 있거나 바닥에 맞춰 내려가 있다. 그 보정을
     * 그대로 옮겨야 실제 건물과 같은 자리에 같은 크기로 선다.
     *
     * 블루프린트에서 GhostMaterial 을 지정해 두었다면 모든 슬롯에 덮어씌운다.
     */
    UFUNCTION(BlueprintCallable, Category = "SF|Tutorial|Ghost")
    void SetGhostMesh(UStaticMesh* InMesh, const FTransform& InRelativeTransform);

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    /** 연출을 더 얹고 싶을 때. 스폰 직후 한 번 불린다 */
    UFUNCTION(BlueprintImplementableEvent, Category = "SF|Tutorial|Ghost")
    void OnGhostAppeared();

    /**
     * 액터의 자리를 잡는 루트.
     *
     * 메시를 루트로 두면 안 된다. 루트 컴포넌트의 "상대" 트랜스폼은 곧
     * 액터 자신의 트랜스폼이라, 건물에서 읽어온 메시 보정을 얹는 순간
     * 고스트가 스폰 위치를 버리고 그 보정값 자리로 순간이동한다.
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> GhostMeshComponent;

    /**
     * 모든 머티리얼 슬롯에 덮어씌울 반투명 머티리얼.
     *
     * 비워두면 메시 원본 머티리얼을 그대로 쓴다 — 그러면 실제 건물과
     * 구분이 안 되므로 반드시 지정하는 편이 낫다.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Tutorial|Ghost")
    TObjectPtr<UMaterialInterface> GhostMaterial = nullptr;

    /**
     * 이 메시에서 Nanite 를 끌지.
     *
     * Nanite 는 투명 머티리얼을 지원하지 않아서, 건물 메시가 Nanite 면
     * 반투명 머티리얼이 무시된다. 이 값을 켜면 Nanite 를 끄고 일반 메시로
     * 그리므로 반투명이 살아난다.
     *
     * 기본값이 꺼짐인 이유: Nanite 를 끄면 폴백 메시로 그리는데, 폴백이
     * 거의 비어 있게 빌드된 에셋이면 아무것도 안 보인다. 그 경우는 눈으로만
     * 보면 "고스트가 안 뜬다" 와 구분이 안 되므로, 기본값은 안전한 쪽으로
     * 두고 필요할 때만 켠다.
     *
     * 켜도 안 보이면 머티리얼을 Opaque 나 Masked 로 바꾸는 편이 낫다.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Tutorial|Ghost")
    bool bDisableNanite = false;

    /** 위아래로 흔들리는 폭(cm). 0 이면 가만히 있는다 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Tutorial|Ghost", meta = (ClampMin = "0.0"))
    float BobAmplitude = 12.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Tutorial|Ghost", meta = (ClampMin = "0.0"))
    float BobSpeed = 2.f;

private:
    FVector BaseLocation = FVector::ZeroVector;
    float ElapsedTime = 0.f;
};
