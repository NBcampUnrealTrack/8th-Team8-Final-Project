#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "SFInGamePlayerController.generated.h"

class UBuildingPlacement;
class ASFBaseBuilding;
class UEnhancedInputLocalPlayerSubsystem;
class UInputAction;
class UInputMappingContext;
class USFInGameHUDWidget;

UENUM(BlueprintType)
enum class ESFControlMode : uint8
{
    Gameplay UMETA(DisplayName = "Gameplay"),
    BuildPlacement UMETA(DisplayName = "Build Placement")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSFOnSelectInput);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSFOnMoveInput, FVector, Destination);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSFOnConfirmBuildInput);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSFOnCancelBuildInput);

UCLASS(Blueprintable)
class STRIKEFORGE_API ASFInGamePlayerController
    : public APlayerController
{
    GENERATED_BODY()

    // 함수 영역
public:
    ASFInGamePlayerController();

    UFUNCTION(
        BlueprintCallable,
        Category = "SF|Input"
    )
    void SetSFControlMode(
        ESFControlMode NewControlMode
    );

    UFUNCTION(
        BlueprintPure,
        Category = "SF|Input"
    )
    ESFControlMode GetSFControlMode() const;
    // building
    FORCEINLINE void SetSpawnedBuildingRef(ASFBaseBuilding* InBuildingRef) { SpawnedBuildingRef = InBuildingRef; }
    void SpawnBuildingFunction(const FName& InRowName);
    void ClearSpawnedBuildingRef();
    void NotifyBuilderArrived();
    
    void SetSelectedActor(AActor* InActor);
protected:
    virtual void BeginPlay() override;

    virtual void SetupInputComponent() override;

private:
    UEnhancedInputLocalPlayerSubsystem*
        GetEnhancedInputSubsystem() const;

    void ConfigureMouseInput();

    void RegisterBaseMappingContext();

    void HandleSelectInput();

    void HandleMoveInput();

    void HandleConfirmBuildInput();

    void HandleCancelBuildInput();

    void CreateInGameHUD();

    // 변수 영역
protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Component")
    TObjectPtr<UBuildingPlacement> PlacementComponent; //======================================================================
    
    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "SF|Input|Mapping"
    )
    TObjectPtr<UInputMappingContext> BaseMappingContext;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "SF|Input|Mapping"
    )
    TObjectPtr<UInputMappingContext> GameplayMappingContext;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "SF|Input|Mapping"
    )
    TObjectPtr<UInputMappingContext> BuildPlacementMappingContext;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "SF|Input|Action"
    )
    TObjectPtr<UInputAction> SelectAction;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "SF|Input|Action"
    )
    TObjectPtr<UInputAction> MoveAction;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "SF|Input|Action"
    )
    TObjectPtr<UInputAction> ConfirmBuildAction;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "SF|Input|Action"
    )
    TObjectPtr<UInputAction> CancelBuildAction;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "SF|Input|Mapping"
    )
    int32 BaseMappingPriority = 0;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "SF|Input|Mapping"
    )
    int32 GameplayMappingPriority = 10;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "SF|Input|Mapping"
    )
    int32 BuildPlacementMappingPriority = 20;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "SF|UI"
    )
    TSubclassOf<USFInGameHUDWidget> InGameHUDWidgetClass;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "SF|Input"
    )
    bool bLockMouseToViewport = true;

    UPROPERTY(
        VisibleInstanceOnly,
        BlueprintReadOnly,
        Category = "SF|Input"
    )
    ESFControlMode CurrentControlMode =
        ESFControlMode::Gameplay;

private:
    UPROPERTY(
        Transient
    )
    TObjectPtr<USFInGameHUDWidget> InGameHUDWidget;

    bool bControlModeInitialized = false;
    
    UPROPERTY()
    TObjectPtr<ASFBaseBuilding> SpawnedBuildingRef; //=============================================================================================
    UPROPERTY()
    TWeakObjectPtr<AActor> SelectedActor;
    // Delegate 변수는 최하단
public:
    UPROPERTY(
        BlueprintAssignable,
        Category = "SF|Input"
    )
    FSFOnSelectInput OnSelectInput;

    UPROPERTY(
        BlueprintAssignable,
        Category = "SF|Input"
    )
    FSFOnMoveInput OnMoveInput;

    UPROPERTY(
        BlueprintAssignable,
        Category = "SF|Input"
    )
    FSFOnConfirmBuildInput OnConfirmBuildInput;

    UPROPERTY(
        BlueprintAssignable,
        Category = "SF|Input"
    )
    FSFOnCancelBuildInput OnCancelBuildInput;
};