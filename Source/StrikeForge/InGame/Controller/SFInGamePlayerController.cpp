#include "SFInGamePlayerController.h"
#include "EnhancedInputComponent.h"
#include "Game/Subsystem/SFFogOfWarSubsystem.h"
#include "EnhancedInputSubsystemInterface.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/EngineTypes.h"
#include "Engine/LocalPlayer.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "ActorComponent/Controller/BuildingPlacement.h"
#include "Buildings/Building/SFBaseBuilding.h"
#include "Game/GameMode/SFGameMode.h"
#include "Game/GameState/SFGameState.h"
#include "Game/Subsystem/SFBuildingSynergySubsystem.h"
#include "InGame/SFInGameHUDWidget.h"
#include "InGame/HUD/Tutorial/SFTutorialSubsystem.h"
#include "InputCoreTypes.h"
#include "Units/Commander/SFCommander.h"
#include "Units/Unit/UnitBase.h"
#include "Units/Interfaces/Movable.h"
#include "ActorComponent/Combat/CombatTargetComponent.h"
#include "SFRTSCameraPawn.h"
#include "Logging/SFLog.h"
#include "Datas/Building/SFBuildingCatalog.h"
#include "Datas/Building/SFBuildingDefinition.h"
#include "Datas/Building/BuildingTierData/SFBuildingTierData.h"
#include "Game/Spawn/SFBuildingSpawnLibrary.h"
#include "Buildings/Building/ProductBuilding/SFProductionBuilding.h"
#include "Kismet/GameplayStatics.h"

ASFInGamePlayerController::ASFInGamePlayerController()
{
    bShowMouseCursor = true;
    bEnableClickEvents = true;
    bEnableMouseOverEvents = true;

    DefaultMouseCursor = EMouseCursor::Default;

    PlacementComponent = CreateDefaultSubobject<UBuildingPlacement>(TEXT("PlacementComponent"));
}

bool ASFInGamePlayerController::TryStartBuildingPlacement(FName InBuildingId)
{
    APawn* ControlledPawn = GetPawn();

    if (InBuildingId.IsNone() ||
        IsValid(SpawnedBuildingRef) ||
        !IsValid(PlacementComponent) ||
        !IsValid(ControlledPawn))
    {
        return false;
    }

    UWorld* World = GetWorld();

    ASFGameState* SFGS = IsValid(World) ?
        World->GetGameState<ASFGameState>() : nullptr;

    // 초기 로딩과 LoadingWidget 종료가 모두 완료된 이후에만
    // 건설 배치에 진입할 수 있도록 제한합니다.
    if (!IsValid(SFGS) || !SFGS->IsStagePlaying())
    {
        SF_LOG(Warning,
            "[SFPC] 스테이지가 아직 플레이 상태가 아닙니다.");

        return false;
    }

    const int32 TeamIndex = GetMyTeamIndex();

    USFBuildingCatalog* Catalog = SFGS->GetTeamBuildingCatalog(TeamIndex);

    USFBuildingDefinition* Definition =
        IsValid(Catalog)
        ? Catalog->FindLoadedDefinition(InBuildingId)
        : nullptr;

    if (!IsValid(Definition) || Definition->BuildingId != InBuildingId)
    {
        SF_LOG(Error,
            "[SFPC] 건물 Definition을 찾을 수 없습니다. BuildingId=%s",
            *InBuildingId.ToString());

        return false;
    }

    // GameState의 넥서스 티어는 0부터 시작하는 내부 인덱스다. 반면
    // BuildingDefinition의 RequiredNexusTier는 UI에 표시하는 1부터의 티어 번호다.
    // 인덱스를 그대로 비교하면 시작 넥서스(인덱스 0)가 있는 상태에서도
    // "티어 1 필요" 건물인 미네랄 광산을 잘못 막게 된다.
    const int32 CurrentNexusTier = SFGS->GetNexusTier(TeamIndex) + 1;
    if (CurrentNexusTier < Definition->RequiredNexusTier)
    {
        SF_LOG(Warning,
            "[SFPC] 넥서스 티어 부족: %s (필요=%d, 현재=%d)",
            *Definition->BuildingId.ToString(),
            Definition->RequiredNexusTier,
            CurrentNexusTier);

        return false;
    }

    constexpr int32 PlacementTierIndex = 0;

    const FSFBuildingTierData* FirstTier = Definition->FindTier(PlacementTierIndex);

    if (!FirstTier)
    {
        SF_LOG(Error,
            "[SFPC] Tier 0 데이터가 없습니다. Definition=%s",
            *GetNameSafe(Definition));

        return false;
    }

    if (!SFGS->CanAffordResource(
        TeamIndex,
        FirstTier->Construction.ConstructCost))
    {
        SF_LOG(Warning,
            "[SFPC] 자원 부족: %s",
            *Definition->BuildingId.ToString());

        if (IsValid(InGameHUDWidget))
        {
            InGameHUDWidget->PlayResourceWarningSound();
        }

        return false;
    }

    // 이 시점에는 GameMode의 초기 로딩 과정에서
    // 모든 건설 가능 Tier 0 에셋이 준비되어 있어야 합니다.
    if (!Definition->AreRuntimeAssetsLoaded(PlacementTierIndex))
    {
        SF_LOG(Error,
            "[SFPC] 사전 로딩되지 않은 Tier 0 건설 요청입니다. "
            "Definition=%s",
            *GetNameSafe(Definition));

        return false;
    }

    FTransform SpawnTransform;
    SpawnTransform.SetLocation(ControlledPawn->GetActorLocation());
    SpawnTransform.SetRotation(FRotator(0.f, 90.f, 0.f).Quaternion());

    ASFBaseBuilding* NewBuilding =
        USFBuildingSpawnLibrary::SpawnBuildingFromDefinition(
            this,
            Definition,
            TeamIndex,
            SpawnTransform,
            ESFBuildingSpawnMode::PlayerPlacedMode,
            PlacementTierIndex);

    if (!IsValid(NewBuilding))
    {
        SF_LOG(Error,
            "[SFPC] 고스트 건물 생성에 실패했습니다. Definition=%s",
            *GetNameSafe(Definition));

        return false;
    }

    if (!PlacementComponent->StartPlacementMode(NewBuilding))
    {
        NewBuilding->Destroy();
        return false;
    }

    SpawnedBuildingRef = NewBuilding;
    return true;
}

void ASFInGamePlayerController::ClearSpawnedBuildingRef()
{
    PlacementComponent->EndPlacementSession();

    if (IsValid(SpawnedBuildingRef))
    {
        // 배치 모드에서 우클릭으로 취소했다면 튜토리얼 고스트도 없애고
        // 건설 메뉴 열기 단계부터 다시 안내한다.
        USFTutorialSubsystem::NotifyTutorialBuildingPlacementCancelled(this, SpawnedBuildingRef);
        SpawnedBuildingRef->Destroy();
        SpawnedBuildingRef = nullptr;
    }
}

// 지휘관 도착시 호출하는 함수
void ASFInGamePlayerController::NotifyBuilderArrived() // start construct =====================================
{
    if (!IsValid(PendingSpawnedBuildingRef)) return;

    ASFBaseBuilding* const Building = PendingSpawnedBuildingRef;
    const FResources ConstructCost = Building->GetConstructCost();

    ASFGameState* const SFGS = GetWorld()->GetGameState<ASFGameState>();
    if (!IsValid(SFGS))
    {
        SF_LOG(Error, "[SFPC] SFGameState를 찾을 수 없어 건설할 수 없습니다.");
        ClearPendingBuildingRef();
        return;
    }

    const int32 TeamIndex = GetMyTeamIndex();

    // 실제 비용 차감에 성공해야 건설을 시작한다.
    if (!SFGS->TrySpendCost(TeamIndex, ConstructCost))
    {
        SF_LOG(Warning, "[SFPC] 자원이 부족해 건설을 취소합니다: %s",
            *Building->GetName());

        if (IsValid(InGameHUDWidget))
        {
            InGameHUDWidget->PlayResourceWarningSound();
        }
        ClearPendingBuildingRef();
        return;
    }

    // 비용 차감 후 건설 시작에 실패하면 비용을 복구한다.
    if (!Building->TryStartConstruction())
    {
        SFGS->RefundCost(TeamIndex, ConstructCost);

        SF_LOG(Error, "[SFPC] 건설 시작에 실패하여 비용을 환불했습니다: %s",
            *Building->GetName());

        ClearPendingBuildingRef();
        return;
    }

    PendingSpawnedBuildingRef = nullptr;
}

void ASFInGamePlayerController::ClearPendingBuildingRef()
{
    if (!IsValid(PendingSpawnedBuildingRef))
    {
        return;
    }

    SF_LOG(Log, "[SFPC] 건설 명령 취소: %s", *PendingSpawnedBuildingRef->GetName());

	// 고스트 위치를 확정한 뒤 지휘관이 도달하지 못했거나 명령이 취소된 경우다.
	// 이때는 실제 건설이 시작되지 않았으므로, 다시 배치할 수 있게 예약을 푼다.
	USFTutorialSubsystem::NotifyTutorialBuildingPlacementCancelled(this, PendingSpawnedBuildingRef);

    PendingSpawnedBuildingRef->Destroy();
    PendingSpawnedBuildingRef = nullptr;
}

void ASFInGamePlayerController::SetSelectedActor(AActor* InActor)
{
    SelectedActor = InActor;

    // 적/중립 액터가 선택된 상태로 이전 명령 모드가 남지 않도록 정리한다.
    if (PendingUnitCommandMode != ESFUnitCommandMode::None &&
        !IsLocallyControllableActor(InActor))
    {
        PendingUnitCommandMode = ESFUnitCommandMode::None;
    }

    // 시너지 건물(집·병영)을 선택하면 그 건물이 속한 활성 시너지 클러스터를 시너지 색으로 한 번 보여 준다.
    if (ASFBaseBuilding* SelectedBuilding = Cast<ASFBaseBuilding>(InActor))
    {
        if (IsValid(PlacementComponent))
        {
            PlacementComponent->FlashSynergyClustersOfBuilding(SelectedBuilding);
        }

        // 내 팀 시너지 건물을 선택하면 소속 클러스터 배지를 그 건물 위로 옮긴다.
        USFBuildingSynergySubsystem* Synergy = USFBuildingSynergySubsystem::Get(this);
        if (IsValid(Synergy) && SelectedBuilding->GetTeamIndex() == GetMyTeamIndex())
        {
            Synergy->SelectBadgeAnchorBuilding(SelectedBuilding);
        }
    }

    SF_LOG(
        Log,
        "[SFInGamePlayerController] Command target changed: %s",
        IsValid(InActor) ? *InActor->GetName() : TEXT("None")
    );
}

void ASFInGamePlayerController::RegisterCommander(ASFCommander* InCommander)
{
    MyCommander = InCommander;

    OnSkillInput.Clear();
    OnSkillInput.AddUniqueDynamic(InCommander, &ASFCommander::ActiveCommanderSkill);
}

void ASFInGamePlayerController::BeginPlay()
{
    Super::BeginPlay();

    ConfigureMouseInput();
    RegisterBaseMappingContext();
    SetSFControlMode(
        ESFControlMode::Gameplay
    );

    BindStageResultEvent();

    CreateInGameHUD();
}

void ASFInGamePlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

#if WITH_EDITOR
    if (IsValid(InputComponent))
    {
        // ProtoType/PT_Stage_2의 레벨 BP에는 K 승리 이벤트가 있지만
        // PT_StageDefence에는 누락돼 있다. 에디터 테스트에서만 보완한다.
        FInputKeyBinding& DebugVictoryBinding = InputComponent->BindKey(
            EKeys::K,
            IE_Pressed,
            this,
            &ASFInGamePlayerController::HandleStage03DebugVictoryInput);
        DebugVictoryBinding.bExecuteWhenPaused = true;
        // ProtoType과 PT_Stage_2의 레벨 BP도 같은 K 입력을 사용한다.
        // 여기서 소비하면 Stage03이 아닌 경우 기존 즉시 승리가 막힌다.
        DebugVictoryBinding.bConsumeInput = false;
    }
#endif

    UEnhancedInputComponent* EnhancedInputComponent =
        Cast<UEnhancedInputComponent>(
            InputComponent
        );

    if (!IsValid(EnhancedInputComponent))
    {
        return;
    }

    if (IsValid(SelectAction))
    {
        EnhancedInputComponent->BindAction(
            SelectAction,
            ETriggerEvent::Started,
            this,
            &ASFInGamePlayerController::HandleSelectionDragStarted
        );

        EnhancedInputComponent->BindAction(
            SelectAction,
            ETriggerEvent::Completed,
            this,
            &ASFInGamePlayerController::HandleSelectionDragCompleted
        );
    }

    if (IsValid(MoveAction))
    {
        EnhancedInputComponent->BindAction(
            MoveAction,
            ETriggerEvent::Started,
            this,
            &ASFInGamePlayerController::
            HandleMoveInput
        );
    }

    if (IsValid(ConfirmBuildAction))
    {
        EnhancedInputComponent->BindAction(
            ConfirmBuildAction,
            ETriggerEvent::Started,
            this,
            &ASFInGamePlayerController::
            HandleConfirmBuildInput
        );
    }

    if (IsValid(CancelBuildAction))
    {
        EnhancedInputComponent->BindAction(
            CancelBuildAction,
            ETriggerEvent::Started,
            this,
            &ASFInGamePlayerController::
            HandleCancelBuildInput
        );
    }

    if (IsValid(FocusCommanderAction))
    {
        EnhancedInputComponent->BindAction(
            FocusCommanderAction,
            ETriggerEvent::Started,
            this,
            &ASFInGamePlayerController::
            HandleFocusCommanderInput
        );
    }
    if (IsValid(ToggleSettingsAction))
    {
        EnhancedInputComponent->BindAction(
            ToggleSettingsAction,
            ETriggerEvent::Started,
            this,
            &ASFInGamePlayerController::
            HandleToggleSettingsInput);
    }

    if (IsValid(SelectAllUnitsAction))
    {
        EnhancedInputComponent->BindAction(
            SelectAllUnitsAction,
            ETriggerEvent::Started,
            this,
            &ASFInGamePlayerController::HandleSelectAllUnitsInput
        );
    }

    TArray<TWeakObjectPtr<UInputAction>> ControlGroupActions =
    {
        ControlGroup01Action,
        ControlGroup02Action,
        ControlGroup03Action,
        ControlGroup04Action,
        ControlGroup05Action,
        ControlGroup06Action
    };

    for (int32 Index = 0; Index < ControlGroupActions.Num(); ++Index)
    {
        if (!ControlGroupActions[Index].IsValid())
        {
            continue;
        }

        EnhancedInputComponent->BindAction(
            ControlGroupActions[Index].Get(),
            ETriggerEvent::Started,
            this,
            &ASFInGamePlayerController::HandleControlGroupInput,
            Index
        );
    }

    if (IsValid(ToggleBuildModeAction))
    {
        EnhancedInputComponent->BindAction(
            ToggleBuildModeAction,
            ETriggerEvent::Started,
            this,
            &ASFInGamePlayerController::HandleToggleBuildModeInput);
    }

    TArray<TWeakObjectPtr<UInputAction>> BuildSlotActions =
    {
        BuildSlot01Action,
        BuildSlot02Action,
        BuildSlot03Action,
        BuildSlot04Action,
        BuildSlot05Action,
        BuildSlot06Action
    };

    for (int32 Index = 0; Index < BuildSlotActions.Num(); ++Index)
    {
        if (!BuildSlotActions[Index].IsValid())
        {
            continue;
        }

        EnhancedInputComponent->BindAction(
            BuildSlotActions[Index].Get(),
            ETriggerEvent::Started,
            this,
            &ASFInGamePlayerController::HandleBuildSlotInput,
            Index
        );
    }

    if (IsValid(ProduceSlot1Action))
    {
        EnhancedInputComponent->BindAction(
            ProduceSlot1Action,
            ETriggerEvent::Started,
            this,
            &ASFInGamePlayerController::HandleProduceSlot1Input
        );
    }

    if (IsValid(ProduceSlot2Action))
    {
        EnhancedInputComponent->BindAction(
            ProduceSlot2Action,
            ETriggerEvent::Started,
            this,
            &ASFInGamePlayerController::HandleProduceSlot2Input
        );
    }

    if (IsValid(ProduceSlot3Action))
    {
        EnhancedInputComponent->BindAction(
            ProduceSlot3Action,
            ETriggerEvent::Started,
            this,
            &ASFInGamePlayerController::HandleProduceSlot3Input
        );
    }

    if (IsValid(AttackCommandAction))
    {
        EnhancedInputComponent->BindAction(
            AttackCommandAction,
            ETriggerEvent::Started,
            this,
            &ASFInGamePlayerController::HandleAttackCommandInput
        );
    }

    if (IsValid(StopCommandAction))
    {
        EnhancedInputComponent->BindAction(
            StopCommandAction,
            ETriggerEvent::Started,
            this,
            &ASFInGamePlayerController::HandleStopCommandInput
        );
    }

    if (IsValid(MoveCommandModeAction))
    {
        EnhancedInputComponent->BindAction(
            MoveCommandModeAction,
            ETriggerEvent::Started,
            this,
            &ASFInGamePlayerController::HandleMoveCommandModeInput
        );
    }

    if (IsValid(RallyPointAction))
    {
        EnhancedInputComponent->BindAction(
            RallyPointAction,
            ETriggerEvent::Started,
            this,
            &ASFInGamePlayerController::HandleRallyPointInput
        );
    }

    TArray<TWeakObjectPtr<UInputAction>> SkillArray = { CommanderSkillAction1,CommanderSkillAction2 ,CommanderSkillAction3 };
    for (int32 i = 0; i < SkillArray.Num(); i++)
    {
        if (SkillArray[i].IsValid())
        {
            EnhancedInputComponent->BindAction(
                SkillArray[i].Get(),
                ETriggerEvent::Started,
                this,
                &ASFInGamePlayerController::HandleSkillCommandInput,
                i + 1
            );
        }
    }
}

void ASFInGamePlayerController::SetSFControlMode(
    ESFControlMode NewControlMode
)
{
    if (NewControlMode == ESFControlMode::BuildPlacement)
    {
        SetBuildModeInputEnabled(false);
    }

    if (
        bControlModeInitialized
        && CurrentControlMode == NewControlMode
        )
    {
        return;
    }

    UEnhancedInputLocalPlayerSubsystem* InputSubsystem =
        GetEnhancedInputSubsystem();

    if (!IsValid(InputSubsystem))
    {
        return;
    }

    UInputMappingContext* TargetMappingContext = nullptr;
    int32 TargetMappingPriority = 0;

    switch (NewControlMode)
    {
    case ESFControlMode::Gameplay:
        TargetMappingContext =
            GameplayMappingContext;

        TargetMappingPriority =
            GameplayMappingPriority;
        break;

    case ESFControlMode::BuildPlacement:
        TargetMappingContext =
            BuildPlacementMappingContext;

        TargetMappingPriority =
            BuildPlacementMappingPriority;
        break;

    default:
        return;
    }

    if (!IsValid(TargetMappingContext))
    {
        return;
    }

    FModifyContextOptions ModifyOptions;

    ModifyOptions
        .bIgnoreAllPressedKeysUntilRelease = true;

    if (IsValid(GameplayMappingContext))
    {
        InputSubsystem->RemoveMappingContext(
            GameplayMappingContext,
            ModifyOptions
        );
    }

    if (IsValid(BuildPlacementMappingContext))
    {
        InputSubsystem->RemoveMappingContext(
            BuildPlacementMappingContext,
            ModifyOptions
        );
    }

    InputSubsystem->AddMappingContext(
        TargetMappingContext,
        TargetMappingPriority,
        ModifyOptions
    );

    if (NewControlMode != ESFControlMode::Gameplay)
    {
        // 건설 모드에서는 유닛 명령의 대기 클릭이 남아 있으면 안 된다.
        PendingUnitCommandMode = ESFUnitCommandMode::None;
        bCommandClickConsumed = false;
        bSelectionPressActive = false;
    }

    CurrentControlMode = NewControlMode;
    bControlModeInitialized = true;
}

ESFControlMode
ASFInGamePlayerController::GetSFControlMode() const
{
    return CurrentControlMode;
}

UEnhancedInputLocalPlayerSubsystem*
ASFInGamePlayerController::
GetEnhancedInputSubsystem() const
{
    ULocalPlayer* LocalPlayer =
        GetLocalPlayer();

    if (!IsValid(LocalPlayer))
    {
        return nullptr;
    }

    return ULocalPlayer::GetSubsystem<
        UEnhancedInputLocalPlayerSubsystem
    >(LocalPlayer);
}

void ASFInGamePlayerController::ConfigureMouseInput()
{
    bShowMouseCursor = true;
    bEnableClickEvents = true;
    bEnableMouseOverEvents = true;

    FInputModeGameAndUI InputMode;

    InputMode.SetHideCursorDuringCapture(false);

    InputMode.SetLockMouseToViewportBehavior(
        bLockMouseToViewport
        ? EMouseLockMode::LockAlways
        : EMouseLockMode::DoNotLock
    );

    SetInputMode(InputMode);
}

void ASFInGamePlayerController::RegisterBaseMappingContext()
{
    UEnhancedInputLocalPlayerSubsystem* InputSubsystem =
        GetEnhancedInputSubsystem();

    if (!IsValid(InputSubsystem))
    {
        return;
    }

    if (!IsValid(BaseMappingContext))
    {
        return;
    }

    InputSubsystem->AddMappingContext(
        BaseMappingContext,
        BaseMappingPriority
    );
}

void ASFInGamePlayerController::HandleSelectionDragStarted()
{
    if (
        IsValid(InGameHUDWidget) &&
        InGameHUDWidget->IsPointerOverHUDPanel()
        )
    {
        bCommandClickConsumed = false;
        bSelectionPressActive = false;

        SF_LOG(
            Log,
            "[SFInGamePlayerController] "
            "Selection ignored because cursor is over HUD panel."
        );

        return;
    }

    bCommandClickConsumed =
        ExecutePendingRallyPointUnderCursor();

    if (!bCommandClickConsumed)
    {
        bCommandClickConsumed =
            ExecutePendingUnitCommandUnderCursor();
    }

    if (bCommandClickConsumed)
    {
        bSelectionPressActive = false;
        return;
    }

    if (!GetMousePosition(
        SelectionPressPosition.X,
        SelectionPressPosition.Y
    ))
    {
        return;
    }

    bSelectionPressActive = true;

    OnSelectionDragStarted.Broadcast(
        SelectionPressPosition
    );

    SF_LOG(
        Log,
        "[SFInGamePlayerController] "
        "Selection drag started | Position: %s",
        *SelectionPressPosition.ToString()
    );
}

void ASFInGamePlayerController::HandleSelectionDragCompleted()
{
    if (bCommandClickConsumed)
    {
        bCommandClickConsumed = false;
        bSelectionPressActive = false;
        return;
    }

    if (!bSelectionPressActive)
    {
        return;
    }

    FVector2D EndScreenPosition;

    if (!GetMousePosition(
        EndScreenPosition.X,
        EndScreenPosition.Y
    ))
    {
        bSelectionPressActive = false;
        return;
    }

    const FVector2D StartScreenPosition =
        SelectionPressPosition;

    bSelectionPressActive = false;

    const float DragDistance = FVector2D::Distance(
        StartScreenPosition,
        EndScreenPosition
    );

    OnSelectionDragCompleted.Broadcast(
        StartScreenPosition,
        EndScreenPosition
    );

    SF_LOG(
        Log,
        "[SFInGamePlayerController] Selection drag completed | Distance: %.1f",
        DragDistance
    );

    if (DragDistance <= SelectionDragThreshold)
    {
        SF_LOG(
            Log,
            "[SFInGamePlayerController] Broadcasting click selection."
        );

        OnSelectInput.Broadcast();
    }
}

void ASFInGamePlayerController::HandleMoveInput()
{
    if (
        IsValid(InGameHUDWidget) &&
        InGameHUDWidget->IsPointerOverHUDPanel()
        )
    {
        return;
    }

    PendingUnitCommandMode = ESFUnitCommandMode::None;
    bCommandClickConsumed = false;

    if (IsValid(InGameHUDWidget))
    {
        InGameHUDWidget->CloseBuildingMode();
    }

    FHitResult Hit;

    if (!GetHitResultUnderCursor(ECC_LANDSCAPE, false, Hit))
    {
        return;
    }

    IssueMoveCommand(Hit.Location);
}

void ASFInGamePlayerController::HandleConfirmBuildInput()
{
    if (
        IsValid(InGameHUDWidget) &&
        InGameHUDWidget->IsPointerOverHUDPanel()
        )
    {
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("BuildingMode:: Clicked."));

    if (!IsValid(SpawnedBuildingRef))
    {
        return;
    }

    ASFCommander* Commander = MyCommander.Get();
    if (!IsValid(Commander))
    {
        SF_LOG(Error, "[SFPC] 커맨더가 등록되지 않아 건설을 시작할 수 없습니다.");
        ClearSpawnedBuildingRef();
        SetSFControlMode(ESFControlMode::Gameplay);
        return;
    }

    if (!PlacementComponent->TryConfirmPlacementMode())
    {
        return;
    }

    ClearPendingBuildingRef();
    PendingSpawnedBuildingRef = SpawnedBuildingRef;
    SpawnedBuildingRef = nullptr;

    if (IsValid(InGameHUDWidget))
    {
        InGameHUDWidget->PlayBuildStartSound();
    }

    //OnConfirmBuildInput.Broadcast();

    // ★CHANGED: SpawnedBuildingRef → PendingConstructionBuilding
    Commander->MoveToBuild(
        PendingSpawnedBuildingRef->GetActorLocation(),
        PendingSpawnedBuildingRef->GetBuildBounds(),
        PendingSpawnedBuildingRef->GetActorRotation().Yaw);

    SetSFControlMode(ESFControlMode::Gameplay);
}

void ASFInGamePlayerController::HandleCancelBuildInput()
{
    //OnCancelBuildInput.Broadcast();

    if (IsValid(InGameHUDWidget))
    {
        InGameHUDWidget->CloseBuildingMode();
    }

    // ★ADD: 배치 중이던 고스트 정리 + 컨트롤 모드 복귀
    //        IA_SFCancelBuild 는 IMC_SFBuildPlacement 에만 있으므로
    //        배치 중에만 발동한다. 확정된 건설은 건드리지 않는다.
    ClearSpawnedBuildingRef();
    SetSFControlMode(ESFControlMode::Gameplay);
}

#if WITH_EDITOR
void ASFInGamePlayerController::HandleStage03DebugVictoryInput()
{
    UWorld* World = GetWorld();
    ASFGameState* GameState = IsValid(World)
        ? World->GetGameState<ASFGameState>()
        : nullptr;

    if (!IsValid(GameState) ||
        GameState->GetCurrentStageRowName() != TEXT("Stage03") ||
        !GameState->IsStagePlaying())
    {
        return;
    }

    // 시작 스토리나 단계별 가이드 도중 눌러도 기존 인트로를 먼저 정리한다.
    // 그렇지 않으면 승리 후일담을 열 자리가 없어 결과 창으로 바로 넘어간다.
    if (USFTutorialSubsystem* Tutorial = World->GetSubsystem<USFTutorialSubsystem>())
    {
        Tutorial->CancelGuide();
    }

    if (IsValid(InGameHUDWidget))
    {
        InGameHUDWidget->DismissActiveStageIntroForDebug();
    }

    ASFGameMode* GameMode = World->GetAuthGameMode<ASFGameMode>();
    if (IsValid(GameMode))
    {
        SF_LOG(Log, "[SFInGamePlayerController] Stage03 editor debug victory (K).");
        GameMode->ProcessVictory();
    }
}
#endif

void ASFInGamePlayerController::CreateInGameHUD()
{
    if (!IsLocalController())
    {
        return;
    }

    if (!InGameHUDWidgetClass)
    {
        return;
    }

    InGameHUDWidget =
        CreateWidget<USFInGameHUDWidget>(
            this,
            InGameHUDWidgetClass
        );

    if (!IsValid(InGameHUDWidget))
    {
        return;
    }

    InGameHUDWidget->AddToViewport();
}

void ASFInGamePlayerController::BindStageResultEvent()
{
    SFGameState =
        GetWorld()->GetGameState<ASFGameState>();

    if (!IsValid(SFGameState))
    {
        SF_LOG(
            Warning,
            "[SFInGamePlayerController] "
            "SFGameState is invalid."
        );

        return;
    }

    SFGameState->OnStageResultChanged.AddUniqueDynamic(
        this,
        &ASFInGamePlayerController::HandleStageResultChanged
    );
}

void ASFInGamePlayerController::HandleStageResultChanged(
    ESFStageResult NewStageResult
)
{
    if (!IsValid(InGameHUDWidget))
    {
        return;
    }

    switch (NewStageResult)
    {
    case ESFStageResult::Victory:
        SF_LOG(
            Log,
            "[SFInGamePlayerController] "
            "Stage Result: Victory"
        );

        InGameHUDWidget->ShowResultWidget(
            true
        );
        break;

    case ESFStageResult::Defeat:
        SF_LOG(
            Log,
            "[SFInGamePlayerController] "
            "Stage Result: Defeat"
        );

        InGameHUDWidget->ShowResultWidget(
            false
        );
        break;

    case ESFStageResult::Undetermined:
    default:
        break;
    }
}

void ASFInGamePlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (IsValid(SFGameState))
    {
        SFGameState->OnStageResultChanged.RemoveDynamic(
            this,
            &ASFInGamePlayerController::
            HandleStageResultChanged
        );
    }

    Super::EndPlay(EndPlayReason);
}

void ASFInGamePlayerController::HandleFocusCommanderInput()
{
    if (IsValid(InGameHUDWidget))
    {
        InGameHUDWidget->CloseBuildingMode();
    }

    ASFCommander* Commander =
        MyCommander.Get();

    if (!IsValid(Commander))
    {
        return;
    }

    SetSelectedActor(
        Commander
    );

    if (IsValid(InGameHUDWidget))
    {
        InGameHUDWidget->SelectActor(
            Commander
        );
    }

    ASFRTSCameraPawn* CameraPawn =
        Cast<ASFRTSCameraPawn>(
            GetPawn()
        );

    if (!IsValid(CameraPawn))
    {
        return;
    }

    CameraPawn->FocusOnWorldLocation(
        Commander->GetActorLocation()
    );
}

void ASFInGamePlayerController::IssueMoveCommand(const FVector& Destination)
{
    if (CurrentControlMode != ESFControlMode::Gameplay)
    {
        return;
    }

    TArray<AActor*> Commandables;
    CollectLocallyControllableActors(Commandables);

    if (Commandables.Num() == 0)
    {
        SF_LOG(Log, "[SFInGamePlayerController] Move command ignored: no locally controlled movable selected.");

        return;
    }

    const bool bAppendToQueue = IsShiftCommandModifierDown();

    // 선택된 유닛 중 한 기라도 교전 또는 추격 중이면,
    // 이번 우클릭은 선택 부대 전체의 강제 이동으로 처리한다.
    bool bForceGroupMove = false;

    for (AActor* Actor : Commandables)
    {
        const AUnitBase* Unit = Cast<AUnitBase>(Actor);

        if (!IsValid(Unit))
        {
            continue;
        }

        const EUnitCombatState State = Unit->GetCombatState();

        if (State == EUnitCombatState::Chasing || State == EUnitCombatState::Attacking || Unit->IsWaitingForGroupMove())
        {
            bForceGroupMove = true;
            break;
        }
    }

    FSFForcedMoveGroup NewGroup;

    if (bForceGroupMove)
    {
        NewGroup.GroupId = NextForcedMoveGroupId++;

        // 후퇴 명령은 기존 명령 뒤에 예약하면 안 된다.
        // 즉시 전투를 취소해야 하므로 Shift 여부와 관계없이 즉시 실행한다.
        SF_LOG(Log, "[SFInGamePlayerController] 교전 중 우클릭 — 부대 강제 이동 시작 | Group=%d | Count=%d", NewGroup.GroupId, Commandables.Num());
    }

    const int32 Count = Commandables.Num();

    for (int32 Index = 0; Index < Count; ++Index)
    {
        AActor* Actor = Commandables[Index];

        if (!IsValid(Actor))
        {
            continue;
        }

        FVector Goal = Destination;

        // 실제 유닛은 클릭 지점 주변에 원형으로 배치
        if (Count > 1 && !bAppendToQueue)
        {
            const float Angle =
                2.f * PI * Index / Count;

            const float Radius =
                FormationSpacing *
                FMath::Sqrt(
                    static_cast<float>(Count)
                );

            Goal += FVector(
                FMath::Cos(Angle),
                FMath::Sin(Angle),
                0.f
            ) * Radius;
        }

        AUnitBase* Unit = Cast<AUnitBase>(Actor);

        if (!IsValid(Unit))
        {
            IMovable::Execute_MoveToLocation(
                Actor,
                Goal
            );

            continue;
        }

        RemoveUnitFromForcedMoveGroups(
            Unit,
            false
        );

        if (bForceGroupMove)
        {
            Unit->SetWaitingForGroupMove(true);

            NewGroup.Units.Add(Unit);

            Unit->ExecuteCommandWithQueue(
                ESFUnitCommand::Move,
                Goal,
                nullptr,
                false
            );
        }
        else
        {
            Unit->SetWaitingForGroupMove(false);

            Unit->ExecuteCommandWithQueue(
                ESFUnitCommand::AttackMove,
                Goal,
                nullptr,
                bAppendToQueue
            );
        }
    }

    if (bForceGroupMove && NewGroup.Units.Num() > 0)
    {
        ActiveForcedMoveGroups.Add(MoveTemp(NewGroup));

        // 이미 검사 타이머가 동작 중이면
        // 새로 생성하지 않는다.
        if (!GetWorldTimerManager().IsTimerActive(ForcedMoveGroupCheckTimer))
        {
            GetWorldTimerManager().SetTimer(ForcedMoveGroupCheckTimer, this, &ASFInGamePlayerController::CheckForcedMoveGroups, 0.1f, true);
        }
    }

    if (IsValid(InGameHUDWidget))
    {
        InGameHUDWidget->ShowMoveDestinationIndicator(Destination, false);
    }

    SF_LOG(
        Log,
        "[SFInGamePlayerController] "
        "Move command | Count: %d | "
        "ForcedGroupMove: %s | Location: %s",
        Count,
        bForceGroupMove
        ? TEXT("true")
        : TEXT("false"),
        *Destination.ToString()
    );
}

void ASFInGamePlayerController::RemoveUnitFromForcedMoveGroups(AUnitBase* Unit, bool bReleaseWaitingState)
{
    if (!IsValid(Unit))
    {
        return;
    }

    for (int32 GroupIndex = ActiveForcedMoveGroups.Num() - 1; GroupIndex >= 0; --GroupIndex)
    {
        FSFForcedMoveGroup& Group = ActiveForcedMoveGroups[GroupIndex];

        Group.Units.RemoveAll([Unit](const TWeakObjectPtr<AUnitBase>&UnitPtr)
            {
                return !UnitPtr.IsValid() || UnitPtr.Get() == Unit;
            }
        );

        if (Group.Units.IsEmpty())
        {
            ActiveForcedMoveGroups.RemoveAt(GroupIndex);
        }
    }

    if (bReleaseWaitingState)
    {
        Unit->SetWaitingForGroupMove(false);
    }

    if (ActiveForcedMoveGroups.IsEmpty())
    {
        GetWorldTimerManager().ClearTimer(ForcedMoveGroupCheckTimer);
    }
}

void ASFInGamePlayerController::CheckForcedMoveGroups()
{
    for (int32 GroupIndex = ActiveForcedMoveGroups.Num() - 1; GroupIndex >= 0; --GroupIndex)
    {
        FSFForcedMoveGroup& Group = ActiveForcedMoveGroups[GroupIndex];

        bool bAllCompleted = true;

        for (const TWeakObjectPtr<AUnitBase>&UnitPtr : Group.Units)
        {
            AUnitBase* Unit = UnitPtr.Get();

            // 제거된 유닛은 완료로 처리한다.
            if (!IsValid(Unit))
            {
                continue;
            }

            // 이동 중 사망한 유닛 때문에
            // 그룹 전체가 영원히 기다리지 않게 한다.
            if (Unit->GetCombatState() == EUnitCombatState::Dead)
            {
                continue;
            }

            // 한 기라도 이동 중이면
            // 아직 그룹을 해제하지 않는다.
            if (!Unit->HasCompletedGroupMove())
            {
                bAllCompleted = false;
                break;
            }
        }

        if (!bAllCompleted)
        {
            continue;
        }

        const int32 CompletedGroupId = Group.GroupId;

        // 먼저 모든 유닛의 대기 상태를
        // 동시에 해제한다.
        for (const TWeakObjectPtr<AUnitBase>&UnitPtr : Group.Units)
        {
            if (AUnitBase* Unit = UnitPtr.Get())
            {
                Unit->SetWaitingForGroupMove(false);
            }
        }

        ActiveForcedMoveGroups.RemoveAt(GroupIndex);

        SF_LOG(Log, "[SFInGamePlayerController] 부대 강제 이동 완료 — 자동 전투 재개 | Group=%d", CompletedGroupId);
    }

    if (ActiveForcedMoveGroups.IsEmpty())
    {
        GetWorldTimerManager().ClearTimer(ForcedMoveGroupCheckTimer);
    }
}

void ASFInGamePlayerController::BeginAttackCommand()
{
    if (CurrentControlMode != ESFControlMode::Gameplay)
    {
        return;
    }

    if (!HasLocallyControllableSelection())
    {
        PendingUnitCommandMode = ESFUnitCommandMode::None;
        SF_LOG(
            Log,
            "[SFInGamePlayerController] Attack command ignored: no locally controlled movable selected."
        );
        return;
    }

    PendingUnitCommandMode = ESFUnitCommandMode::Attack;

    SF_LOG(
        Log,
        "[SFInGamePlayerController] Attack command mode started."
    );
}

void ASFInGamePlayerController::StopSelectedUnits()
{
    if (CurrentControlMode != ESFControlMode::Gameplay)
    {
        return;
    }

    TArray<AActor*> Commandables;
    CollectLocallyControllableActors(Commandables);

    if (Commandables.Num() == 0)
    {
        PendingUnitCommandMode = ESFUnitCommandMode::None;
        SF_LOG(
            Log,
            "[SFInGamePlayerController] Stop command ignored: no locally controlled movable selected."
        );
        return;
    }

    PendingUnitCommandMode = ESFUnitCommandMode::None;

    for (AActor* Actor : Commandables)
    {
        if (!IsValid(Actor))
        {
            continue;
        }

        if (AUnitBase* Unit = Cast<AUnitBase>(Actor))
        {
            RemoveUnitFromForcedMoveGroups(Unit, true);

            Unit->ExecuteCommand(ESFUnitCommand::Stop, Actor->GetActorLocation(), nullptr);
        }
        else
        {
            IMovable::Execute_StopMove(Actor);
        }
    }

    SF_LOG(
        Log,
        "[SFInGamePlayerController] Stop command executed | Count: %d",
        Commandables.Num()
    );
}

void ASFInGamePlayerController::BeginMoveCommand()
{
    if (CurrentControlMode != ESFControlMode::Gameplay)
    {
        return;
    }

    if (!HasLocallyControllableSelection())
    {
        PendingUnitCommandMode = ESFUnitCommandMode::None;
        SF_LOG(
            Log,
            "[SFInGamePlayerController] Move command ignored: no locally controlled movable selected."
        );
        return;
    }

    PendingUnitCommandMode = ESFUnitCommandMode::Move;

    SF_LOG(
        Log,
        "[SFInGamePlayerController] Move command mode started."
    );
}

void ASFInGamePlayerController::HandleToggleSettingsInput()
{
    if (!IsValid(InGameHUDWidget))
    {
        return;
    }

    InGameHUDWidget->CloseBuildingMode();
    InGameHUDWidget->ToggleSettings();
}

void ASFInGamePlayerController::HandleSelectAllUnitsInput()
{
    if (CurrentControlMode != ESFControlMode::Gameplay)
    {
        return;
    }

    if (!IsValid(InGameHUDWidget))
    {
        return;
    }

    InGameHUDWidget->SelectAllOwnedUnits();
}

void ASFInGamePlayerController::HandleToggleBuildModeInput()
{
    if (!IsValid(InGameHUDWidget))
    {
        return;
    }

    InGameHUDWidget->ToggleBuildingMode();

    SF_LOG(Log, "[SFInGamePlayerController] Build mode toggled.");
}

void ASFInGamePlayerController::HandleProduceSlot1Input()
{
    RequestProductionBySlot(0);
}

void ASFInGamePlayerController::HandleProduceSlot2Input()
{
    RequestProductionBySlot(1);
}

void ASFInGamePlayerController::HandleProduceSlot3Input()
{
    RequestProductionBySlot(2);
}

void ASFInGamePlayerController::HandleAttackCommandInput()
{
    BeginAttackCommand();
}

void ASFInGamePlayerController::HandleStopCommandInput()
{
    StopSelectedUnits();
}

void ASFInGamePlayerController::HandleMoveCommandModeInput()
{
    BeginMoveCommand();
}

void ASFInGamePlayerController::HandleSkillCommandInput(int32 Tier)
{
    SF_LOG(Log, "Commander Skill: Tier%d", Tier);

    OnSkillInput.Broadcast(Tier);
}

void ASFInGamePlayerController::RequestProductionBySlot(int32 SlotIndex)
{
    if (CurrentControlMode != ESFControlMode::Gameplay)
    {
        return;
    }

    if (!IsValid(InGameHUDWidget))
    {
        return;
    }

    InGameHUDWidget->RequestProductionBySlot(SlotIndex);
}

bool ASFInGamePlayerController::ExecutePendingUnitCommandUnderCursor()
{
    if (PendingUnitCommandMode == ESFUnitCommandMode::None)
    {
        return false;
    }

    const ESFUnitCommandMode CommandMode = PendingUnitCommandMode;
    PendingUnitCommandMode = ESFUnitCommandMode::None;

    FHitResult Hit;
    AActor* AttackTarget = nullptr;

    if (CommandMode == ESFUnitCommandMode::Attack)
    {
        // 공격은 먼저 액터를 찾고, 액터가 없으면 지형을 공격 이동 목적지로 쓴다.
        // 선택 trace와 동일하게 Visibility 채널을 사용해 유닛·건물 메시를 잡는다.
        const ETraceTypeQuery VisibilityTraceType =
            UEngineTypes::ConvertToTraceType(ECC_Visibility);

        if (GetHitResultUnderCursorByChannel(
            VisibilityTraceType,
            false,
            Hit
        ))
        {
            AActor* Candidate = Hit.GetActor();

            if (IsValidAttackTarget(Candidate))
            {
                AttackTarget = Candidate;
            }
        }

        // 대상이 없으면 클릭 지점을 공격 이동 목적지로 처리한다.
        if (!IsValid(AttackTarget) &&
            !GetHitResultUnderCursor(ECC_LANDSCAPE, false, Hit))
        {
            SF_LOG(
                Log,
                "[SFInGamePlayerController] Attack command canceled: no valid target or ground."
            );
            return true;
        }
    }
    else if (CommandMode == ESFUnitCommandMode::Move)
    {
        if (!GetHitResultUnderCursor(ECC_LANDSCAPE, false, Hit))
        {
            SF_LOG(
                Log,
                "[SFInGamePlayerController] Move command canceled: no ground under cursor."
            );
            return true;
        }

        // 일반 우클릭과 같은 이동 경로를 사용해 FormationSpacing,
        // OnMoveInput, 목적지 표시를 중복 구현하지 않는다.
        IssueMoveCommand(Hit.Location);
        return true;
    }
    else
    {
        return true;
    }

    TArray<AActor*> Commandables;
    CollectLocallyControllableActors(Commandables);

    if (Commandables.Num() == 0)
    {
        return true;
    }

    const bool bAppendToQueue =
        IsShiftCommandModifierDown();

    bool bAcceptedTargetAttack = false;
    for (AActor* Actor : Commandables)
    {
        if (IsValid(AttackTarget))
        {
            const AUnitBase* Unit = Cast<AUnitBase>(Actor);
            if (!Unit || !Unit->CanForceAttackTarget(AttackTarget)) continue;
            bAcceptedTargetAttack = true;
        }
        ExecuteUnitCommandForActor(
            Actor,
            CommandMode,
            Hit.Location,
            AttackTarget,
            bAppendToQueue
        );
    }

    if (IsValid(AttackTarget) && !bAcceptedTargetAttack) return true;

    if (CommandMode == ESFUnitCommandMode::Attack &&
        IsValid(AttackTarget) &&
        IsValid(InGameHUDWidget))
    {
        InGameHUDWidget->
            PlayEnemyHoverAttackFeedback(
                AttackTarget
            );
    }

    if (!IsValid(AttackTarget) && IsValid(InGameHUDWidget))
    {
        InGameHUDWidget->ShowMoveDestinationIndicator(
            Hit.Location,
            true
        );
    }

    SF_LOG(
        Log,
        "[SFInGamePlayerController] %s command executed | Count: %d | Target: %s | Location: %s",
        CommandMode == ESFUnitCommandMode::Attack ? TEXT("Attack") : TEXT("Move"),
        Commandables.Num(),
        IsValid(AttackTarget) ? *AttackTarget->GetName() : TEXT("Ground"),
        *Hit.Location.ToString()
    );

    return true;
}

bool ASFInGamePlayerController::HasLocallyControllableSelection() const
{
    TArray<AActor*> Commandables;
    CollectLocallyControllableActors(Commandables);
    return Commandables.Num() > 0;
}

void ASFInGamePlayerController::CollectLocallyControllableActors(
    TArray<AActor*>& OutActors
) const
{
    OutActors.Reset();

    if (IsValid(InGameHUDWidget))
    {
        for (const TObjectPtr<AActor>& Actor : InGameHUDWidget->GetSelectedActors())
        {
            AActor* Selected = Actor.Get();

            if (IsLocallyControllableActor(Selected))
            {
                OutActors.AddUnique(Selected);
            }
        }
    }

    // HUD 다중 선택이 비어 있는 일반 단일 선택도 같은 명령 경로로 처리한다.
    AActor* Single = SelectedActor.Get();

    if (OutActors.Num() == 0 && IsLocallyControllableActor(Single))
    {
        OutActors.Add(Single);
    }
}

void ASFInGamePlayerController::ExecuteUnitCommandForActor(
    AActor* Actor,
    ESFUnitCommandMode CommandMode,
    const FVector& Location,
    AActor* Target,
    bool bAppendToQueue
)
{
    if (!IsValid(Actor) || !IsLocallyControllableActor(Actor))
    {
        return;
    }

    if (AUnitBase* Unit = Cast<AUnitBase>(Actor))
    {
        ESFUnitCommand UnitCommand = ESFUnitCommand::Move;

        if (CommandMode == ESFUnitCommandMode::Attack)
        {
            // A키 공격 명령은 사용자의 새로운 선택이므로
            // 기존 부대 후퇴 대기를 해제한다.
            RemoveUnitFromForcedMoveGroups(Unit, true);
        }

        switch (CommandMode)
        {
        case ESFUnitCommandMode::Attack:
            UnitCommand = ESFUnitCommand::Attack;
            break;

        case ESFUnitCommandMode::Move:
            UnitCommand = ESFUnitCommand::AttackMove;
            break;

        default:
            return;
        }

        // 유닛의 사거리·시야·강제 타겟·정지 상태는 UnitBase가 책임진다.
        Unit->ExecuteCommandWithQueue(
            UnitCommand,
            Location,
            Target,
            bAppendToQueue
        );
        return;
    }

    // 지휘관 등 UnitBase가 아닌 기존 IMovable 액터의 하위 호환 경로.
    switch (CommandMode)
    {
    case ESFUnitCommandMode::Move:
        IMovable::Execute_MoveToLocation(Actor, Location);
        break;

    case ESFUnitCommandMode::Attack:
        SF_LOG(
            Warning,
            "[SFInGamePlayerController] Attack command ignored: %s is not an AUnitBase.",
            *Actor->GetName()
        );
        break;

    default:
        break;
    }
}

bool ASFInGamePlayerController::IsValidAttackTarget(const AActor* Actor) const
{
    if (const USFFogOfWarSubsystem* Fog = USFFogOfWarSubsystem::Get(this))
    {
        if (!Fog->IsActorVisibleToLocalPlayer(Actor)) return false;
    }
    if (!IsValid(Actor))
    {
        return false;
    }

    const UCombatTargetComponent* CombatTarget =
        Actor->FindComponentByClass<UCombatTargetComponent>();

    const ASFBaseBuilding* Building = Cast<ASFBaseBuilding>(Actor);
    return (!Building || Building->ProvidesVision()) && IsValid(CombatTarget) &&
        CombatTarget->IsAlive() &&
        CombatTarget->GetTeamIndex() != INDEX_NONE;
}

bool ASFInGamePlayerController::IsLocallyControllableActor(const AActor* Actor) const
{
    if (!IsValid(Actor) ||
        !Actor->GetClass()->ImplementsInterface(UMovable::StaticClass()))
    {
        return false;
    }

    // 커맨더는 별도 등록 포인터로도 소유권을 확정할 수 있다.
    if (Actor == MyCommander.Get())
    {
        return true;
    }

    const UCombatTargetComponent* CombatTarget =
        Actor->FindComponentByClass<UCombatTargetComponent>();

    return IsValid(CombatTarget) &&
        CombatTarget->GetTeamIndex() == MyTeamIndex;
}

void ASFInGamePlayerController::HandleRallyPointInput()
{
    BeginRallyPointCommand();
}

void ASFInGamePlayerController::CollectSelectedProductionBuildings(
    TArray<ASFProductionBuilding*>& OutBuildings
) const
{
    OutBuildings.Reset();

    if (IsValid(InGameHUDWidget))
    {
        for (const TObjectPtr<AActor>& Actor :
            InGameHUDWidget->GetSelectedActors())
        {
            ASFProductionBuilding* ProductionBuilding =
                Cast<ASFProductionBuilding>(Actor.Get());

            if (IsValid(ProductionBuilding))
            {
                OutBuildings.AddUnique(ProductionBuilding);
            }
        }
    }

    // 단일 선택 상태이거나, HUD 목록에 주 선택 건물이 빠진 경우도 처리한다.
    if (ASFProductionBuilding* SingleProductionBuilding =
        Cast<ASFProductionBuilding>(SelectedActor.Get()))
    {
        OutBuildings.AddUnique(SingleProductionBuilding);
    }
}

void ASFInGamePlayerController::BeginRallyPointCommand()
{
    if (CurrentControlMode != ESFControlMode::Gameplay)
    {
        return;
    }

    TArray<ASFProductionBuilding*> ProductionBuildings;
    CollectSelectedProductionBuildings(ProductionBuildings);

    if (ProductionBuildings.IsEmpty())
    {
        SF_LOG(
            Warning,
            "[SFInGamePlayerController] "
            "Rally point requires at least one production building."
        );
        return;
    }

    for (const ASFProductionBuilding* ProductionBuilding : ProductionBuildings)
    {
        const UCombatTargetComponent* CombatTarget =
            ProductionBuilding->FindComponentByClass<
            UCombatTargetComponent
            >();

        if (!IsValid(CombatTarget) ||
            CombatTarget->GetTeamIndex() != MyTeamIndex)
        {
            SF_LOG(
                Warning,
                "[SFInGamePlayerController] "
                "Cannot set rally point on an enemy building."
            );
            return;
        }
    }

    PendingUnitCommandMode = ESFUnitCommandMode::None;
    bRallyPointPending = true;

    SF_LOG(
        Log,
        "[SFInGamePlayerController] "
        "Rally point mode started."
    );
}

bool ASFInGamePlayerController::ExecutePendingRallyPointUnderCursor()
{
    if (!bRallyPointPending)
    {
        return false;
    }

    bRallyPointPending = false;

    FHitResult Hit;

    if (!GetHitResultUnderCursor(ECC_LANDSCAPE, false, Hit))
    {
        SF_LOG(
            Warning,
            "[SFInGamePlayerController] "
            "Rally point canceled: no ground."
        );
        return true;
    }

    TArray<ASFProductionBuilding*> ProductionBuildings;
    CollectSelectedProductionBuildings(ProductionBuildings);

    if (ProductionBuildings.IsEmpty())
    {
        return true;
    }

    int32 UpdatedBuildingCount = 0;

    for (ASFProductionBuilding* ProductionBuilding : ProductionBuildings)
    {
        if (!IsValid(ProductionBuilding))
        {
            continue;
        }

        const UCombatTargetComponent* CombatTarget =
            ProductionBuilding->FindComponentByClass<
            UCombatTargetComponent
            >();

        if (!IsValid(CombatTarget) ||
            CombatTarget->GetTeamIndex() != MyTeamIndex)
        {
            continue;
        }

        ProductionBuilding->SetAssembleVectorPoint(
            Hit.Location
        );

        ++UpdatedBuildingCount;
    }

    if (UpdatedBuildingCount == 0)
    {
        return true;
    }

    if (IsValid(InGameHUDWidget))
    {
        InGameHUDWidget->RefreshRallyPointIndicator();
    }

    SF_LOG(
        Log,
        "[SFInGamePlayerController] "
        "Rally point set for %d buildings: %s",
        UpdatedBuildingCount,
        *Hit.Location.ToString()
    );

    return true;
}

void ASFInGamePlayerController::PlaySelectionSound(AActor* SelectedTarget)
{
    // 소리는 내 컴퓨터에서만 들려야함. 멀티가 아니라서 상관없지만 논리상 IsLocalController 조건추가
    if (!IsLocalController() ||
        !IsValid(SelectedTarget) ||
        !SelectedTarget->Implements<USFSelectable>())
    {
        return;
    }

    USoundBase* SelectionSound =
        ISFSelectable::Execute_GetSelectionSound(
            SelectedTarget);

    if (!IsValid(SelectionSound))
    {
        return;
    }

    UGameplayStatics::PlaySoundAtLocation(
        this,
        SelectionSound,
        SelectedTarget->GetActorLocation()
    );
}

void ASFInGamePlayerController::HandleControlGroupInput(
    int32 GroupIndex
)
{
    if (CurrentControlMode != ESFControlMode::Gameplay)
    {
        return;
    }

    if (!IsValid(InGameHUDWidget))
    {
        return;
    }

    const bool bCtrlPressed =
        IsInputKeyDown(EKeys::LeftControl) ||
        IsInputKeyDown(EKeys::RightControl);

    const bool bShiftPressed =
        IsInputKeyDown(EKeys::LeftShift) ||
        IsInputKeyDown(EKeys::RightShift);

    if (bCtrlPressed)
    {
        InGameHUDWidget->SaveCurrentSelectionToControlGroup(
            GroupIndex,
            false
        );

        // 저장 입력은 더블 입력으로 기록하지 않는다.
        LastControlGroupIndex = INDEX_NONE;
        LastControlGroupInputTime = -1.0;

        return;
    }

    if (bShiftPressed)
    {
        InGameHUDWidget->SaveCurrentSelectionToControlGroup(
            GroupIndex,
            true
        );

        // 추가 입력도 더블 입력으로 기록하지 않는다.
        LastControlGroupIndex = INDEX_NONE;
        LastControlGroupInputTime = -1.0;

        return;
    }

    const double CurrentTime =
        GetWorld()
        ? GetWorld()->GetTimeSeconds()
        : 0.0;

    const bool bDoublePressed =
        LastControlGroupIndex == GroupIndex &&
        CurrentTime - LastControlGroupInputTime
        <= ControlGroupDoubleTapWindow;

    // 첫 번째 숫자 입력에서도 부대를 선택한다.
    InGameHUDWidget->SelectControlGroup(GroupIndex);

    // 같은 숫자를 짧은 시간 안에 다시 누르면 카메라 이동
    if (bDoublePressed)
    {
        FocusCameraOnCurrentControlGroup();
    }

    LastControlGroupIndex = GroupIndex;
    LastControlGroupInputTime = CurrentTime;
}

void ASFInGamePlayerController::FocusCameraOnCurrentControlGroup()
{
    if (!IsValid(InGameHUDWidget))
    {
        return;
    }

    FVector FocusLocation = FVector::ZeroVector;
    int32 ValidActorCount = 0;

    for (
        const TObjectPtr<AActor>& ActorPtr :
        InGameHUDWidget->GetSelectedActors()
        )
    {
        AActor* Actor = ActorPtr.Get();

        if (!IsValid(Actor))
        {
            continue;
        }

        FocusLocation += Actor->GetActorLocation();
        ++ValidActorCount;
    }

    if (ValidActorCount == 0)
    {
        return;
    }

    FocusLocation /= static_cast<float>(ValidActorCount);

    ASFRTSCameraPawn* CameraPawn =
        Cast<ASFRTSCameraPawn>(GetPawn());

    if (!IsValid(CameraPawn))
    {
        return;
    }

    CameraPawn->FocusOnWorldLocation(
        FocusLocation
    );

    SF_LOG(
        Log,
        "[SFInGamePlayerController] "
        "Control group camera focused | Location: %s",
        *FocusLocation.ToString()
    );
}

bool ASFInGamePlayerController::IsShiftCommandModifierDown() const
{
    return
        IsInputKeyDown(EKeys::LeftShift) ||
        IsInputKeyDown(EKeys::RightShift);
}

void ASFInGamePlayerController::HandleBuildSlotInput(
    int32 SlotIndex
)
{
    SF_LOG(
        Log,
        "[BuildInput] Slot=%d Enabled=%s Mode=%d HUD=%s",
        SlotIndex,
        bBuildModeInputEnabled
        ? TEXT("true")
        : TEXT("false"),
        static_cast<int32>(CurrentControlMode),
        IsValid(InGameHUDWidget)
        ? TEXT("Valid")
        : TEXT("Invalid")
    );

    if (
        !bBuildModeInputEnabled ||
        CurrentControlMode !=
        ESFControlMode::Gameplay
        )
    {
        SF_LOG(
            Warning,
            "[BuildInput] 입력 거부"
        );

        return;
    }

    if (!IsValid(InGameHUDWidget))
    {
        return;
    }

    InGameHUDWidget->RequestBuildingBySlot(
        SlotIndex
    );
}

void ASFInGamePlayerController::SetBuildModeInputEnabled(
    bool bEnabled
)
{
    UEnhancedInputLocalPlayerSubsystem* InputSubsystem =
        GetEnhancedInputSubsystem();

    if (!IsValid(InputSubsystem))
    {
        return;
    }

    FModifyContextOptions ModifyOptions;
    ModifyOptions.bIgnoreAllPressedKeysUntilRelease =
        true;

    if (bEnabled)
    {
        // GameplayMappingContext는 유지한다.
        // 지휘관 이동, 유닛 이동, 공격 입력이 계속 작동해야 한다.

        if (IsValid(BuildModeMappingContext))
        {
            InputSubsystem->AddMappingContext(
                BuildModeMappingContext,
                BuildModeMappingPriority,
                ModifyOptions
            );
        }
    }
    else
    {
        if (IsValid(BuildModeMappingContext))
        {
            InputSubsystem->RemoveMappingContext(
                BuildModeMappingContext,
                ModifyOptions
            );
        }
    }

    bBuildModeInputEnabled = bEnabled;
}
