#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "InputActionValue.h"
#include "SFRTSCameraPawn.generated.h"

class APlayerController;
class UCameraComponent;
class UFloatingPawnMovement;
class UInputAction;
class UInputComponent;
class USphereComponent;
class USceneComponent;
class USpringArmComponent;

UCLASS()
class STRIKEFORGE_API ASFRTSCameraPawn : public APawn
{
    GENERATED_BODY()

public:
    ASFRTSCameraPawn();

    virtual void Tick(float DeltaTime) override;

    UFUNCTION(
        BlueprintCallable,
        Category = "SF|Camera|EdgeScroll"
    )
    void SetEdgeScrollEnabled(bool bEnabled);

    void FocusOnWorldLocation(const FVector& WorldLocation);

protected:
    virtual void BeginPlay() override;

    virtual void SetupPlayerInputComponent(
        UInputComponent* PlayerInputComponent
    ) override;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "SF|Camera"
    )
    TObjectPtr<USphereComponent> CameraCollision;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "SF|Camera"
    )
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "SF|Camera"
    )
    TObjectPtr<USpringArmComponent> SpringArm;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "SF|Camera"
    )
    TObjectPtr<UCameraComponent> Camera;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "SF|Movement"
    )
    TObjectPtr<UFloatingPawnMovement> FloatingPawnMovement;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "SF|Input"
    )
    TObjectPtr<UInputAction> CameraMoveAction;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "SF|Input"
    )
    TObjectPtr<UInputAction> CameraZoomAction;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "SF|Camera",
        meta = (ClampMin = "100.0")
    )
    float CameraMoveSpeed = 2600.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "SF|Camera",
        meta = (ClampMin = "1.0")
    )
    float CameraZoomSpeed = 150.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "SF|Camera"
    )
    float InitialCameraDistance = 1600.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "SF|Camera"
    )
    float MinimumCameraDistance = 1000.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "SF|Camera"
    )
    float MaximumCameraDistance = 3000.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "SF|Camera"
    )
    float CameraPitch = -55.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "SF|Camera|EdgeScroll"
    )
    bool bEnableEdgeScroll = true;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "SF|Camera|EdgeScroll",
        meta = (ClampMin = "1.0")
    )
    float EdgeScrollThreshold = 20.0f;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "SF|Camera|EdgeScroll",
        meta = (ClampMin = "0.0", ClampMax = "1.0")
    )
    float EdgeScrollInputScale = 0.85f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "SF|Camera|Bounds"
    )
    bool bConstrainCameraToBounds = false;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "SF|Camera|Bounds"
    )
    FVector2D MinimumCameraBounds =
        FVector2D(-5000.0f, -5000.0f);

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "SF|Camera|Bounds"
    )
    FVector2D MaximumCameraBounds =
        FVector2D(5000.0f, 5000.0f);

private:
    void MoveCamera(const FInputActionValue& InputValue);

    void ZoomCamera(const FInputActionValue& InputValue);
    /** 현재 SpringArm 길이로 오버헤드 UI 배율을 갱신한다. */
    void PushOverheadUIScale() const;
    
    void HandleEdgeScroll();

    void ApplyCameraMovement(
        const FVector2D& MovementInput
    );

    void ClampCameraLocation();
};