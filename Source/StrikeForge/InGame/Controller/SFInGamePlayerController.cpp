#include "SFInGamePlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystemInterface.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "ActorComponent/Combat/CombatTargetComponent.h"
#include "ActorComponent/Controller/BuildingPlacement.h"
#include "Buildings/Building/SFBaseBuilding.h"
#include "Game/GameState/SFGameState.h"
#include "InGame/SFInGameHUDWidget.h"
#include "Units/Commander/SFCommander.h"
#include "Units/Interfaces/Movable.h"
#include "SFRTSCameraPawn.h"
#include "Logging/SFLog.h"
#include "Datas/Building/SFBuildingCatalog.h"
#include "Datas/Building/SFBuildingDefinition.h"
#include "Datas/Building/BuildingTierData/SFBuildingTierData.h"

ASFInGamePlayerController::ASFInGamePlayerController()
{
    bShowMouseCursor = true;
    bEnableClickEvents = true;
    bEnableMouseOverEvents = true;
    
    DefaultMouseCursor = EMouseCursor::Default;
    
    PlacementComponent= CreateDefaultSubobject<UBuildingPlacement>(TEXT("PlacementComponent"));
}

bool ASFInGamePlayerController::SpawnBuildingFunction(const FName& InBuildingId)
{
    // 자원이 충분한가 아닌가 검사 로직. 가능구역2
    if (IsValid(SpawnedBuildingRef))
    {
        UE_LOG(LogTemp, Error, TEXT("Exist Spawned Building Ref"));
        return false;
    }

    ASFGameState* SFGS = GetWorld()->GetGameState<ASFGameState>();

    if (!IsValid(SFGS))
    {
        SF_LOG(
            Error,
            "[SFPC] GameState가 없습니다.");

        return false;
    }

    USFBuildingCatalog* Catalog = SFGS->GetTeamBuildingCatalog(GetMyTeamIndex());

    if (!IsValid(Catalog))
    {
        SF_LOG(
            Error,
            "[SFPC] Team %d의 BuildingCatalog가 없습니다.",
            GetMyTeamIndex());

        return false;
    }

    USFBuildingDefinition* Definition = Catalog->FindDefinition(InBuildingId);

    if (!IsValid(Definition))
    {
        SF_LOG(
            Error,
            "[SFPC] BuildingDefinition을 찾지 못했습니다. Id=%s",
            *InBuildingId.ToString());

        return false;
    }
    
#pragma region CheckResources 
    // FName의 첫번째 티어의 정보 불러오기
    // 나중에 2티어부터 건설할수있게되면 수정이 필요한지점이다
    const FSFBuildingTierData* FirstTier = Definition->FindTier(0);
    if (!FirstTier) return false;
    
    if (!SFGS->CanAffordResource(GetMyTeamIndex(), FirstTier->Construction.ConstructCost))
    {
        SF_LOG(Warning, "[SFPC]: 자원 부족: %s", *Definition->BuildingId.ToString());
        return false;
    }

#pragma endregion 
    
    FTransform Trans;
    Trans.SetLocation(GetPawn()->GetActorLocation());
    Trans.SetRotation(FRotator(0.f, 90.f, 0.f).Quaternion());
    
    UClass* SpawnClass = Definition->BuildingClass.LoadSynchronous();     // ★ADD
    if (!SpawnClass)
    {
        UE_LOG(LogTemp, Error, TEXT("SpawnBuildingFunction: BuildingClass is None. Row=%s"),
            *Definition->BuildingId.ToString());
        return false;
    }
    
    ASFBaseBuilding* DeferredActor = GetWorld()->SpawnActorDeferred<ASFBaseBuilding>(
        SpawnClass,
        Trans,
        nullptr,
        nullptr,
        ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn
        );
    
    if (!IsValid(DeferredActor))                                   // ★ADD
    {
        UE_LOG(LogTemp, Error, TEXT("SpawnBuildingFunction: SpawnActorDeferred failed."));
        return false;
    }
    /*
     * RowName은 현재 선택 UI 테이블을 조회하기 위한 임시 브리지다.
     * Selection UI가 DA로 전환되면 제거할 수 있다.
     */
    if (!DeferredActor->SetBuildingDefinition(Definition))
    {
        DeferredActor->Destroy();
        return false;
    }
    const FName SelectionRowName =
    Definition->SelectionUIRowName.IsNone()
        ? Definition->BuildingId
        : Definition->SelectionUIRowName;
    
    DeferredActor->SetRowName(SelectionRowName);
    DeferredActor->SetMyTeamIndex(MyTeamIndex);
    
    if (UCombatTargetComponent* Combat = DeferredActor->FindComponentByClass<UCombatTargetComponent>())
    {
        Combat->SetTeamIndex(MyTeamIndex);
    }
    // deferred spawn finished
    DeferredActor->FinishSpawning(Trans);

    // gird init
    // 배치가능여부 return
    if (!PlacementComponent->StartPlacementMode(DeferredActor))
    {   // 배치불가한데 배치하려하면 destroy
        DeferredActor->Destroy();
        return false;
    }
    SpawnedBuildingRef = DeferredActor;
    return true;
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
    
    const FResources NeedConstructCost = PendingSpawnedBuildingRef->GetConstructCost();
    ASFGameState* SFGS = GetWorld()->GetGameState<ASFGameState>();
    
    if (!IsValid(SFGS))
    {   // SFGS 게임스테이트 없을때
        SF_LOG(Error, "[SFPC] SFGameState 를 찾을 수 없어 건설할 수 없습니다.");
        ClearPendingBuildingRef();
        return;
    }

    if (!SFGS->CanAffordResource(GetMyTeamIndex(), NeedConstructCost))
    {   // 자원 없을때
        SF_LOG(Warning, "[SFPC] 지휘관이 도착했으나 자원이 부족해 건설이 취소되었습니다: %s", *PendingSpawnedBuildingRef->GetName());
        ClearPendingBuildingRef();
        return;
    }

    if (!PendingSpawnedBuildingRef->TryStartConstruction())
    {   // 다른 외부 조건
        SF_LOG(Error, "[SFPC] 건설 시작 실패: %s", *PendingSpawnedBuildingRef->GetName());
        ClearPendingBuildingRef();
        return;
    }
    
    // 모든 조건 통과시 - 차감
    SFGS->TrySpendCost(GetMyTeamIndex(), NeedConstructCost);
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

    SF_LOG(
        Log,
        "[SFInGamePlayerController] Command target changed: %s",
        IsValid(InActor) ? *InActor->GetName() : TEXT("None")
    );
}

void ASFInGamePlayerController::RegisterCommander(ASFCommander* InCommander)
{
    MyCommander = InCommander;
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

    OnConfirmBuildInput.Broadcast();
    
    // ★CHANGED: SpawnedBuildingRef → PendingConstructionBuilding
    Commander->MoveToBuild(
        PendingSpawnedBuildingRef->GetActorLocation(),
        PendingSpawnedBuildingRef->GetBuildBounds(),
        PendingSpawnedBuildingRef->GetActorRotation().Yaw);

    SetSFControlMode(ESFControlMode::Gameplay);
}

void ASFInGamePlayerController::HandleCancelBuildInput()
{
    OnCancelBuildInput.Broadcast();

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

    OnMoveInput.Broadcast(Destination);

    // 명령 대상 수집.
    // 다중 선택은 HUD 가 관리하므로 거기서 가져오고,
    // 비어 있으면 기존 단일 선택으로 넘어간다(건물·커맨더 흐름 유지).
    TArray<AActor*> Commandables;

    if (IsValid(InGameHUDWidget))
    {
        for (const TObjectPtr<AActor>& Actor : InGameHUDWidget->GetSelectedActors())
        {
            if (!IsValid(Actor)) continue;
            if (!Actor->GetClass()->ImplementsInterface(UMovable::StaticClass())) continue;

            Commandables.AddUnique(Actor.Get());
        }
    }

    if (Commandables.Num() == 0)
    {
        AActor* Single = SelectedActor.Get();

        if (IsValid(Single)
            && Single->GetClass()->ImplementsInterface(UMovable::StaticClass()))
        {
            Commandables.Add(Single);
        }
    }

    if (Commandables.Num() == 0)
    {
        return;
    }

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

        IMovable::Execute_MoveToLocation(
            Commandables[Index],
            Goal
        );
    }

    if (IsValid(InGameHUDWidget))
    {
        InGameHUDWidget->ShowMoveDestinationIndicator(
            Destination
        );
    }

    SF_LOG(Log, "[SFInGamePlayerController] Move command | Count: %d | Location: %s",
        Count, *Destination.ToString());
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
