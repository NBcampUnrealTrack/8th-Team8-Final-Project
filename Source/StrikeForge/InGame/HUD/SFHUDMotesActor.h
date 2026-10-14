#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SFHUDMotesActor.generated.h"

class UMaterialInstanceDynamic;

/** One small, isolated Niagara capture for the selection frame's gemstone. */
UCLASS(Transient, NotBlueprintable)
class STRIKEFORGE_API ASFHUDMotesActor : public AActor
{
    GENERATED_BODY()
public:
    ASFHUDMotesActor();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    void RequestFrame();
    UMaterialInstanceDynamic* GetMaterial() const { return Material; }
private:
    UPROPERTY() TObjectPtr<class UNiagaraComponent> Motes;
    UPROPERTY() TObjectPtr<class USceneCaptureComponent2D> Capture;
    UPROPERTY() TObjectPtr<class UTextureRenderTarget2D> Target;
    UPROPERTY() TObjectPtr<class UMaterialInterface> BaseMaterial;
    UPROPERTY() TObjectPtr<class UMaterialInstanceDynamic> Material;
    double LastRequest = -100.;
};
