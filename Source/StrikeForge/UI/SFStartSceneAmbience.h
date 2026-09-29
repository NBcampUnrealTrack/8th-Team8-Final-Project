#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SFStartSceneAmbience.generated.h"

class UInstancedStaticMeshComponent;
class APlayerController;
class UPointLightComponent;

/** World-space title-scene fireflies: depth tested against the actual forest. */
UCLASS()
class STRIKEFORGE_API ASFStartSceneAmbience : public AActor
{
    GENERATED_BODY()
public:
    ASFStartSceneAmbience();
    void SetViewer(APlayerController* Player);
    virtual void Tick(float DeltaSeconds) override;
private:
    struct FMote { FVector Origin; float Phase; float Size; int32 Group; int32 Instance; };
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> WarmMotes;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> EtherMotes;
    TWeakObjectPtr<APlayerController> Viewer;
    TArray<FMote> Motes;
    TWeakObjectPtr<UPointLightComponent> CampfireLight;
    float CampfireIntensity=0.f;
    float Age=0.f;
};
