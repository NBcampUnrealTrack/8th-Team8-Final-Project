#include "SFInGamePlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystemInterface.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/EngineTypes.h"
#include "Engine/LocalPlayer.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "ActorComponent/Controller/BuildingPlacement.h"
#include "Buildings/Building/SFBaseBuilding.h"
#include "Game/GameState/SFGameState.h"
#include "InGame/SFInGameHUDWidget.h"
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
#include "Engine/GameInstance.h"
#include "Game/Subsystem/SFBuildingAssetSubsystem.h"

ASFInGamePlayerController::ASFInGamePlayerController()
{
    bShowMouseCursor = true;
    bEnableClickEvents = true;
    bEnableMouseOverEvents = true;

    DefaultMouseCursor = EMouseCursor::Default;

    PlacementComponent = CreateDefaultSubobject<UBuildingPlacement>(TEXT("PlacementComponent"));
}

void ASFInGamePlayerController::RequestBuildingPlacementAsync(FName InBuildingId,
    FSFOnBuildingPlacementReady OnComplete)
{
    if (bBuildingPlacementLoadPending ||
        IsValid(SpawnedBuildingRef) ||
        InBuildingId.IsNone())
    {
        OnComplete.ExecuteIfBound(false);
        return;
    }

    ASFGameState* SFGS =
        GetWorld()
        ? GetWorld()->GetGameState<ASFGameState>()
        : nullptr;

    if (!IsValid(SFGS))
    {
        OnComplete.ExecuteIfBound(false);
        return;
    }

    USFBuildingCatalog* Catalog =
        SFGS->GetTeamBuildingCatalog(
            GetMyTeamIndex());

    USFBuildingDefinition* Definition =
        IsValid(Catalog)
            ? Catalog->FindLoadedDefinition(
                InBuildingId)
            : nullptr;

    if (!IsValid(Definition))
    {
        OnComplete.ExecuteIfBound(false);
        return;
    }

    const FSFBuildingTierData* FirstTier =
        Definition->FindTier(0);

    if (!FirstTier ||
        !SFGS->CanAffordResource(
            GetMyTeamIndex(),
            FirstTier->Construction.ConstructCost))
    {
        OnComplete.ExecuteIfBound(false);
        return;
    }

    UGameInstance* GameInstance =
        GetGameInstance();

    USFBuildingAssetSubsystem* AssetSubsystem =
        IsValid(GameInstance)
            ? GameInstance
                ->GetSubsystem<USFBuildingAssetSubsystem>()
            : nullptr;

    if (!IsValid(AssetSubsystem))
    {
        OnComplete.ExecuteIfBound(false);
        return;
    }

    bBuildingPlacementLoadPending = true;

    AssetSubsystem->RequestBuildingAssetsAsync(
        Definition,
        0,
        FSFOnBuildingAssetsReady::CreateUObject(
            this,
            &ThisClass::HandleBuildingPlacementAssetsReady,
            InBuildingId,
            MoveTemp(OnComplete)));
}

void ASFInGamePlayerController::ClearSpawnedBuildingRef()
{
    PlacementComponent->EndPlacementSession();

    if (IsValid(SpawnedBuildingRef))
    {
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

    if (IsValid(ToggleBuildModeAction))
    {
        EnhancedInputComponent->BindAction(
            ToggleBuildModeAction,
            ETriggerEvent::Started,
            this,
            &ASFInGamePlayerController::HandleToggleBuildModeInput
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
        "[SFInGamePlayerController] Selection drag started | Position: %s",
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

    // 명령 대상 수집은 이동·공격·정지에서 같은 함수를 사용한다.
    // HUD 다중 선택과 기존 단일 선택을 합치되, 적/중립 액터는 제외한다.
    TArray<AActor*> Commandables;
    CollectLocallyControllableActors(Commandables);

    if (Commandables.Num() == 0)
    {
        SF_LOG(
            Log,
            "[SFInGamePlayerController] Move command ignored: no locally controlled movable selected."
        );
        return;
    }

    // 실제로 조종할 대상이 확인된 뒤에만 UI/BP 쪽 이동 이벤트를 알린다.
    //OnMoveInput.Broadcast(Destination);

    // 같은 좌표를 여러 기에 주면 한 점에 몰려 서로 밀친다.
    // MoveAcceptanceRadius 가 50cm 라 도착 판정이 겹치기 때문이다.
    // 원형으로 흩어 놓아 각자 다른 지점을 목표로 삼게 한다.
    const int32 Count = Commandables.Num();

    for (int32 Index = 0; Index < Count; ++Index)
    {
        FVector Goal = Destination;

        if (Count > 1)
        {
            const float Angle = 2.f * PI * Index / Count;
            const float Radius =
                FormationSpacing *
                FMath::Sqrt(static_cast<float>(Count));

            Goal += FVector(
                FMath::Cos(Angle),
                FMath::Sin(Angle),
                0.f
            ) * Radius;
        }

        ExecuteUnitCommandForActor(
            Commandables[Index],
            ESFUnitCommandMode::Move,
            Goal,
            nullptr
        );
    }

    if (IsValid(InGameHUDWidget))
    {
        InGameHUDWidget->ShowMoveDestinationIndicator(
            Destination,
            false
        );
    }

    SF_LOG(Log, "[SFInGamePlayerController] Move command | Count: %d | Location: %s",
        Count, *Destination.ToString());
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
            Unit->ExecuteCommand(
                ESFUnitCommand::Stop,
                Actor->GetActorLocation(),
                nullptr
            );
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

        // 적 대상이 아니면 클릭 지점을 공격 이동 목적지로 처리한다.
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

    for (AActor* Actor : Commandables)
    {
        ExecuteUnitCommandForActor(
            Actor,
            CommandMode,
            Hit.Location,
            AttackTarget
        );
    }

    if (IsValid(InGameHUDWidget))
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
    AActor* Target
) const
{
    if (!IsValid(Actor) || !IsLocallyControllableActor(Actor))
    {
        return;
    }

    if (AUnitBase* Unit = Cast<AUnitBase>(Actor))
    {
        ESFUnitCommand UnitCommand = ESFUnitCommand::Move;

        switch (CommandMode)
        {
        case ESFUnitCommandMode::Attack:
            UnitCommand = ESFUnitCommand::Attack;
            break;

        case ESFUnitCommandMode::Move:
            UnitCommand = ESFUnitCommand::Move;
            break;

        default:
            return;
        }

        // 유닛의 사거리·시야·강제 타겟·정지 상태는 UnitBase가 책임진다.
        Unit->ExecuteCommand(UnitCommand, Location, Target);
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
    if (!IsValid(Actor) || IsLocallyControllableActor(Actor))
    {
        return false;
    }

    const UCombatTargetComponent* CombatTarget =
        Actor->FindComponentByClass<UCombatTargetComponent>();

    return IsValid(CombatTarget) &&
        CombatTarget->IsAlive() &&
        CombatTarget->GetTeamIndex() != MyTeamIndex;
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

void ASFInGamePlayerController::BeginRallyPointCommand()
{
    if (CurrentControlMode != ESFControlMode::Gameplay)
    {
        return;
    }

    ASFProductionBuilding* ProductionBuilding =
        Cast<ASFProductionBuilding>(SelectedActor.Get());

    if (!IsValid(ProductionBuilding))
    {
        SF_LOG(
            Warning,
            "[SFInGamePlayerController] "
            "Rally point requires a production building."
        );
        return;
    }

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
            "Cannot set rally point on enemy building."
        );
        return;
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

    ASFProductionBuilding* ProductionBuilding =
        Cast<ASFProductionBuilding>(SelectedActor.Get());

    if (!IsValid(ProductionBuilding))
    {
        return true;
    }

    const UCombatTargetComponent* CombatTarget =
        ProductionBuilding->FindComponentByClass<
        UCombatTargetComponent
        >();

    if (!IsValid(CombatTarget) ||
        CombatTarget->GetTeamIndex() != MyTeamIndex)
    {
        return true;
    }

    ProductionBuilding->SetAssembleVectorPoint(
        Hit.Location
    );

    if (IsValid(InGameHUDWidget))
    {
        InGameHUDWidget->ShowMoveDestinationIndicator(
            Hit.Location,
            false
        );
    }

    SF_LOG(
        Log,
        "[SFInGamePlayerController] "
        "Rally point set: %s",
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

void ASFInGamePlayerController::HandleBuildingPlacementAssetsReady(USFBuildingDefinition* InDefinition,
    int32 InTierIndex, bool bSuccess, FName InBuildingId, FSFOnBuildingPlacementReady OnComplete)
{
    bBuildingPlacementLoadPending = false;

    if (!bSuccess ||
        !IsValid(InDefinition) ||
        InTierIndex != 0 ||
        InDefinition->BuildingId != InBuildingId ||
        IsValid(SpawnedBuildingRef))
    {
        OnComplete.ExecuteIfBound(false);
        return;
    }

    ASFGameState* SFGS =
        GetWorld()
        ? GetWorld()->GetGameState<ASFGameState>()
        : nullptr;

    const FSFBuildingTierData* FirstTier =
        InDefinition->FindTier(0);

    if (!IsValid(SFGS) ||
        !FirstTier ||
        !SFGS->CanAffordResource(
            GetMyTeamIndex(),
            FirstTier->Construction.ConstructCost) ||
        !IsValid(GetPawn()))
    {
        OnComplete.ExecuteIfBound(false);
        return;
    }

    FTransform SpawnTransform;
    SpawnTransform.SetLocation(
        GetPawn()->GetActorLocation());

    SpawnTransform.SetRotation(
        FRotator(0.f, 90.f, 0.f)
        .Quaternion());

    ASFBaseBuilding* NewBuilding =
        USFBuildingSpawnLibrary::
        SpawnBuildingFromDefinition(
            this,
            InDefinition,
            GetMyTeamIndex(),
            SpawnTransform,
            ESFBuildingSpawnMode::PlayerPlacedMode,
            0);

    if (!IsValid(NewBuilding) ||
        !PlacementComponent
        ->StartPlacementMode(NewBuilding))
    {
        if (IsValid(NewBuilding))
        {
            NewBuilding->Destroy();
        }

        OnComplete.ExecuteIfBound(false);
        return;
    }

    SpawnedBuildingRef = NewBuilding;

    OnComplete.ExecuteIfBound(true);
}
