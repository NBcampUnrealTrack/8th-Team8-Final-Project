#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Game/GameState/SFGameState.h"
#include "SFInGamePlayerController.generated.h"

class ASFCommander;
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
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FSFOnMoveInput,
    FVector,
    Destination
);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSFOnConfirmBuildInput);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSFOnCancelBuildInput);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FSFOnSelectionDragStarted,
    FVector2D,
    StartScreenPosition
);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FSFOnSelectionDragCompleted,
    FVector2D,
    StartScreenPosition,
    FVector2D,
    EndScreenPosition
);
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
    bool SpawnBuildingFunction(const FName& InRowName);
    void ClearSpawnedBuildingRef();
    void ClearPendingBuildingRef();
    void NotifyBuilderArrived();
    
    void SetSelectedActor(AActor* InActor);
    void RegisterCommander(ASFCommander* InCommander);
    void IssueMoveCommand(const FVector& Destination);
protected:
    virtual void BeginPlay() override;

    virtual void EndPlay(
        const EEndPlayReason::Type EndPlayReason
    ) override;

    virtual void SetupInputComponent() override;

private:
    UEnhancedInputLocalPlayerSubsystem*
        GetEnhancedInputSubsystem() const;

    void ConfigureMouseInput();

    void RegisterBaseMappingContext();

    void BindStageResultEvent();

    UFUNCTION()
    void HandleStageResultChanged(
        ESFStageResult NewStageResult
    );
    //드래그
    void HandleSelectionDragStarted();
    void HandleSelectionDragCompleted();

    void HandleMoveInput();

    void HandleConfirmBuildInput();

    void HandleCancelBuildInput();
    
    void HandleFocusCommanderInput();

    void CreateInGameHUD();
    
    void HandleToggleSettingsInput();

    void HandleToggleBuildModeInput();
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
    ESFControlMode CurrentControlMode = ESFControlMode::Gameplay;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "SF|Input"
    )
    TObjectPtr<UInputAction> FocusCommanderAction = nullptr;

    UPROPERTY(
        EditDefaultsOnly, 
        BlueprintReadOnly, 
        Category = "SF|Input"
    )
    TObjectPtr<UInputAction> ToggleSettingsAction = nullptr;

    UPROPERTY(
        EditDefaultsOnly, 
        BlueprintReadOnly, 
        Category = "SF|Input", 
        meta = (AllowPrivateAccess = "true")
    )
    TObjectPtr<UInputAction> ToggleBuildModeAction = nullptr;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "SF|Selection"
    )
    float SelectionDragThreshold = 8.f;

    /**
     * 다중 이동 시 유닛 간 간격(cm).
     *
     * 같은 좌표를 여러 기에 주면 한 점에 몰려 서로 밀친다.
     * 유닛 지름 90cm(Agent Radius 45 x 2) 보다 여유가 있어야 한다.
     */
    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "SF|Selection",
        meta = (ClampMin = "50.0")
    )
    float FormationSpacing = 120.f;

private:
    UPROPERTY(
        Transient
    )
    TObjectPtr<USFInGameHUDWidget> InGameHUDWidget;

    bool bControlModeInitialized = false;
    
    UPROPERTY()
    TWeakObjectPtr<AActor> SelectedActor;

    UPROPERTY()
    TObjectPtr<ASFBaseBuilding> SpawnedBuildingRef;           // 드래그 중인 고스트

    UPROPERTY()
    TObjectPtr<ASFBaseBuilding> PendingSpawnedBuildingRef;  // 확정, 커맨더 대기 중

    UPROPERTY()
    TWeakObjectPtr<ASFCommander> MyCommander;

    UPROPERTY(Transient)
    TObjectPtr<ASFGameState> SFGameState;

    FVector2D SelectionPressPosition = FVector2D::ZeroVector;
    bool bSelectionPressActive = false;
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

    UPROPERTY(
        BlueprintAssignable,
        Category = "SF|Input"
    )
    FSFOnSelectionDragStarted OnSelectionDragStarted;

    UPROPERTY(
        BlueprintAssignable,
        Category = "SF|Input"
    )
    FSFOnSelectionDragCompleted OnSelectionDragCompleted;
};