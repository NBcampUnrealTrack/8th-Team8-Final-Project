// SFUnitPortraitActor.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SFUnitPortraitActor.generated.h"

class UAnimationAsset;
class USpotLightComponent;
class USceneCaptureComponent2D;
class USceneComponent;
class USkeletalMesh;
class USkeletalMeshComponent;
class UTextureRenderTarget2D;

UCLASS(Blueprintable)
class STRIKEFORGE_API ASFUnitPortraitActor : public AActor
{
    GENERATED_BODY()

public:
    ASFUnitPortraitActor();

    UFUNCTION(BlueprintCallable, Category = "SF|Portrait")
    bool ApplyPortrait(USkeletalMesh* NewPortraitMesh, UAnimationAsset* NewPortraitAnimation);

    UFUNCTION(BlueprintCallable, Category = "SF|Portrait")
    void ClearPortrait();

    UFUNCTION(BlueprintCallable, Category = "SF|Portrait")
    void SetPortraitCaptureEnabled(bool bEnabled);

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SF|Portrait")
    TObjectPtr<USceneComponent> SceneRoot = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SF|Portrait")
    TObjectPtr<USkeletalMeshComponent> PortraitMesh = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SF|Portrait")
    TObjectPtr<USceneCaptureComponent2D> PortraitCapture = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SF|Portrait")
    TObjectPtr<USpotLightComponent> PortraitSpotLight = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Portrait")
    TObjectPtr<UTextureRenderTarget2D> PortraitRenderTarget = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Portrait|Camera")
    FVector PortraitCameraLocation = FVector(50.0f, 0.0f, 75.0f);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Portrait|Camera")
    FRotator PortraitCameraRotation = FRotator(0.0f, 180.0f, 0.0f);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Portrait|Camera", meta = (ClampMin = "5.0", ClampMax = "170.0"))
    float PortraitFieldOfView = 40.0f;
};