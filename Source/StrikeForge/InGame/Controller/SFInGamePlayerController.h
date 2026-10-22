#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Game/GameState/SFGameState.h"
#include "SFInGamePlayerController.generated.h"

class ASFCommander;
class AActor;
class UBuildingPlacement;
class ASFBaseBuilding;
class ASFProductionBuilding;
class UEnhancedInputLocalPlayerSubsystem;
class UInputAction;
class UInputMappingContext;
class USFInGameHUDWidget;
class USFBuildingDefinition;
class AUnitBase;

UENUM(BlueprintType)
enum class ESFControlMode : uint8
{
    Gameplay UMETA(DisplayName = "Gameplay"),
    BuildPlacement UMETA(DisplayName = "Build Placement")
};

UENUM(BlueprintType)
enum class ESFUnitCommandMode : uint8
{
    None UMETA(DisplayName = "None"),
    Attack UMETA(DisplayName = "Attack"),
    Move UMETA(DisplayName = "Move")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSFOnSelectInput);
UENUM()
enum class ESFGameplayAction : uint8
{
    Move, Attack, Stop, Skill, Produce, CancelProduct, Upgrade, CancelConstruction, Rally
};
//DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
//    FSFOnMoveInput,
//    FVector,
//    Destination
//);
//DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSFOnConfirmBuildInput);
//DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSFOnCancelBuildInput);
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

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSFOnSkillInput, int32, SkillTier);

struct FSFForcedMoveGroup
{
    int32 GroupId = INDEX_NONE;
    TArray<TWeakObjectPtr<AUnitBase>> Units;
};

UCLASS(Blueprintable)
class STRIKEFORGE_API ASFInGamePlayerController
    : public APlayerController
{
    GENERATED_BODY()

    // 함수 영역
public:
    ASFInGamePlayerController();
#if WITH_DEV_AUTOMATION_TESTS
    friend struct FSFForcedAttackTestAccess;
#endif

    UFUNCTION(BlueprintPure, Category = "SF|Building|Placement")
    UBuildingPlacement* GetBuildingPlacementComponent() const { return PlacementComponent; }

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

    /**
     * 사전 로딩된 Tier 0 에셋을 검증하고 건물 배치를 준비한다.
     * Catalog Entry 클릭에서 호출되며 고스트 건물 생성과
     * StartPlacementMode로 이어진다.
     */
    bool TryStartBuildingPlacement(FName InBuildingId);

    UFUNCTION(BlueprintCallable, Category = "SF|Building|Placement")
    void ClearSpawnedBuildingRef();

    void ClearPendingBuildingRef();

    void NotifyBuilderArrived();

    void SetSelectedActor(AActor* InActor);

    void RegisterCommander(ASFCommander* InCommander);

    /** 이 로컬 플레이어에게 등록된 지휘관을 반환한다. */
    UFUNCTION(BlueprintPure, Category = "SF|Commander")
    ASFCommander* GetMyCommander() const
    {
        return MyCommander.Get();
    }

    /** 튜토리얼 스포트라이트가 실제 HUD 버튼 위치를 찾을 때만 사용한다. */
    USFInGameHUDWidget* GetInGameHUDWidget() const
    {
        return InGameHUDWidget;
    }

    void IssueMoveCommand(const FVector& Destination);
    bool IsPVPGameplay() const;
    UFUNCTION(Client,Reliable) void ClientGameplayMoveVoice(const TArray<AActor*>& Speakers);
    UFUNCTION(Server, Reliable)
    void ServerGameplayAction(ESFGameplayAction Action, const TArray<AActor*>& Actors, FVector_NetQuantize Location,
        AActor* Target = nullptr, FName Row = NAME_None, int32 Index = 0, bool bAppend = false);
    UFUNCTION(Server, Reliable)
    void ServerPlaceGameplayBuilding(FName BuildingId, FTransform Transform);
    /** Shared authoritative placement path for player requests and the practice opponent. */
    ASFBaseBuilding* TryPlaceGameplayBuilding(FName BuildingId, FTransform Transform);

    /** Consume a pending A/M command on the minimap. */
    bool ExecutePendingUnitCommandAtLocation(const FVector& Destination);

    UFUNCTION(BlueprintCallable, Category = "SF|Command")
    void BeginAttackCommand();

    UFUNCTION(BlueprintCallable, Category = "SF|Command")
    void StopSelectedUnits();

    UFUNCTION(BlueprintCallable, Category = "SF|Command")
    void BeginMoveCommand();

    UFUNCTION(BlueprintCallable, Category = "SF|Command")
    void BeginRallyPointCommand();

    // team
    FORCEINLINE void SetMyTeamIndex(const int32 InMyTeamIndex) { MyTeamIndex = InMyTeamIndex; }

    FORCEINLINE int32 GetMyTeamIndex() const { return MyTeamIndex; }

    void SetBuildModeInputEnabled(bool bEnabled);

    UFUNCTION(BlueprintPure, Category = "SF|Input")
    bool IsBuildModeInputEnabled() const { return bBuildModeInputEnabled; }

    /** Shared selection audio entry point for mouse, HUD and blueprint selection. */
    UFUNCTION(BlueprintCallable)
    void PlaySelectionSound(AActor* SelectedTarget);

protected:
    void ConfigurePVEGameplayProfile();
    virtual void BeginPlay() override;

    virtual void EndPlay(
        const EEndPlayReason::Type EndPlayReason
    ) override;

    virtual void SetupInputComponent() override;

private:
    UEnhancedInputLocalPlayerSubsystem* GetEnhancedInputSubsystem() const;

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

    void HandleSelectAllUnitsInput();

    void HandleControlGroupInput(int32 GroupIndex);

    void FocusCameraOnCurrentControlGroup();

    void HandleToggleBuildModeInput();

    // 생산 슬롯 입력 처리
    void HandleProduceSlot1Input();

    void HandleProduceSlot2Input();

    void HandleProduceSlot3Input();

    void RequestProductionBySlot(int32 SlotIndex);

    // 커맨드 입력 처리
    void HandleAttackCommandInput();

    void HandleStopCommandInput();

    void HandleMoveCommandModeInput();

    void HandleSkillCommandInput(int32 Tier);

    bool ExecutePendingUnitCommandUnderCursor();

    void CollectLocallyControllableActors(
        TArray<AActor*>& OutActors
    ) const;

    void ExecuteUnitCommandForActor(
        AActor* Actor,
        ESFUnitCommandMode CommandMode,
        const FVector& Location,
        AActor* Target,
        bool bAppendToQueue
    );

    bool IsValidAttackTarget(const AActor* Actor) const;

    bool HasLocallyControllableSelection() const;

    bool IsLocallyControllableActor(const AActor* Actor) const;

    void HandleRallyPointInput();

    void CollectSelectedProductionBuildings(
        TArray<ASFProductionBuilding*>& OutBuildings
    ) const;

    bool ExecutePendingRallyPointUnderCursor();

    bool IsShiftCommandModifierDown() const;

    /** 교전 중 우클릭으로 생성된 이동 그룹들의 완료 여부를 검사한다 */
    void CheckForcedMoveGroups();

    /** 유닛을 기존 강제 이동 그룹에서 제거한다 */
    void RemoveUnitFromForcedMoveGroups(AUnitBase* Unit, bool bReleaseWaitingState);

    /** 동시에 진행 중인 부대 강제 이동 그룹들 */
    TArray<FSFForcedMoveGroup> ActiveForcedMoveGroups;

    /** 그룹 식별 번호 */
    int32 NextForcedMoveGroupId = 1;

    /** 그룹 이동 완료 검사 타이머 */
    FTimerHandle ForcedMoveGroupCheckTimer;

    // Async


    //빌드모드 단축키
    void HandleBuildSlotInput(int32 SlotIndex);

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

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Input|Mapping")
    TObjectPtr<UInputMappingContext> BuildModeMappingContext = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Input|Mapping")
    int32 BuildModeMappingPriority = 30;

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
        Category = "SF|Input"
    )
    TObjectPtr<UInputAction> SelectAllUnitsAction = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Input|Action")
    TObjectPtr<UInputAction> ControlGroup01Action = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Input|Action")
    TObjectPtr<UInputAction> ControlGroup02Action = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Input|Action")
    TObjectPtr<UInputAction> ControlGroup03Action = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Input|Action")
    TObjectPtr<UInputAction> ControlGroup04Action = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Input|Action")
    TObjectPtr<UInputAction> ControlGroup05Action = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Input|Action")
    TObjectPtr<UInputAction> ControlGroup06Action = nullptr;

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
        Category = "SF|Input|Action"
    )
    TObjectPtr<UInputAction> RallyPointAction = nullptr;

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

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "SF|Control Group",
        meta = (ClampMin = "0.15", ClampMax = "0.6")
    )
    float ControlGroupDoubleTapWindow = 0.3f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Input|Action")
    TObjectPtr<UInputAction> ProduceSlot1Action = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Input|Action")
    TObjectPtr<UInputAction> ProduceSlot2Action = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Input|Action")
    TObjectPtr<UInputAction> ProduceSlot3Action = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Input|Action")
    TObjectPtr<UInputAction> AttackCommandAction = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Input|Action")
    TObjectPtr<UInputAction> StopCommandAction = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Input|Action")
    TObjectPtr<UInputAction> MoveCommandModeAction = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Input|Action")
    TObjectPtr<UInputAction> CommanderSkillAction1 = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Input|Action")
    TObjectPtr<UInputAction> CommanderSkillAction2 = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Input|Action")
    TObjectPtr<UInputAction> CommanderSkillAction3 = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Input|Action")
    TObjectPtr<UInputAction> BuildSlot01Action = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Input|Action")
    TObjectPtr<UInputAction> BuildSlot02Action = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Input|Action")
    TObjectPtr<UInputAction> BuildSlot03Action = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Input|Action")
    TObjectPtr<UInputAction> BuildSlot04Action = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Input|Action")
    TObjectPtr<UInputAction> BuildSlot05Action = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Input|Action")
    TObjectPtr<UInputAction> BuildSlot06Action = nullptr;

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

    ESFUnitCommandMode PendingUnitCommandMode = ESFUnitCommandMode::None;
    bool bCommandClickConsumed = false;

    bool bRallyPointPending = false;

    int32 LastControlGroupIndex = INDEX_NONE;

    double LastControlGroupInputTime = -1.0;

    bool bBuildModeInputEnabled = false;

    // team
    int32 MyTeamIndex = 0;

    // Delegate 변수는 최하단
public:
    UPROPERTY(
        BlueprintAssignable,
        Category = "SF|Input"
    )
    FSFOnSelectInput OnSelectInput;

    // 쓰이지 않는 델리게이트 변수? 체크바람.
    //UPROPERTY(
    //    BlueprintAssignable,
    //    Category = "SF|Input"
    //)
    //FSFOnMoveInput OnMoveInput;

    UPROPERTY(
        BlueprintAssignable,
        Category = "SF|Input"
    )
    FSFOnSkillInput OnSkillInput;

    // 쓰이지 않는 델리게이트 변수? 체크바람.
    //UPROPERTY(
    //    BlueprintAssignable,
    //    Category = "SF|Input"
    //)
    //FSFOnConfirmBuildInput OnConfirmBuildInput;

    // 쓰이지 않는 델리게이트 변수? 체크바람.
    //UPROPERTY(
    //    BlueprintAssignable,
    //    Category = "SF|Input"
    //)
    //FSFOnCancelBuildInput OnCancelBuildInput;

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
