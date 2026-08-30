#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SFTutorialGhostActor.generated.h"

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

    /** 위아래로 흔들리는 폭(cm). 0 이면 가만히 있는다 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Tutorial|Ghost", meta = (ClampMin = "0.0"))
    float BobAmplitude = 12.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Tutorial|Ghost", meta = (ClampMin = "0.0"))
    float BobSpeed = 2.f;

private:
    FVector BaseLocation = FVector::ZeroVector;
    float ElapsedTime = 0.f;
};
