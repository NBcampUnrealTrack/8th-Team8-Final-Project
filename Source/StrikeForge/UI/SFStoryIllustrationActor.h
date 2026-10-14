#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SFStoryIllustrationActor.generated.h"

class USkeletalMeshComponent;
class UNiagaraComponent;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/** Isolated portrait/particle stage. Nothing is spawned into the playable lobby scene. */
UCLASS(Transient, NotBlueprintable)
class STRIKEFORGE_API ASFStoryIllustrationActor : public AActor
{
    GENERATED_BODY()
public:
    ASFStoryIllustrationActor();
    void SetChapter(int32 Index);
    UMaterialInstanceDynamic* GetPresentationMaterial();
    virtual void Tick(float DeltaSeconds) override;
    bool HasActiveParticles() const;
protected:
    virtual void BeginPlay() override;
private:
    UPROPERTY() TObjectPtr<USkeletalMeshComponent> Beast;
    UPROPERTY() TObjectPtr<USkeletalMeshComponent> Etherian;
    UPROPERTY() TObjectPtr<UNiagaraComponent> EtherParticles;
    UPROPERTY() TObjectPtr<UNiagaraComponent> WarmParticles;
    UPROPERTY() TObjectPtr<USceneCaptureComponent2D> Capture;
    UPROPERTY() TObjectPtr<UTextureRenderTarget2D> Target;
    UPROPERTY() TObjectPtr<UMaterialInterface> CaptureMaterial;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> PresentationMaterial;
    float Elapsed = 0.f;
    int32 Chapter = 0;
};
