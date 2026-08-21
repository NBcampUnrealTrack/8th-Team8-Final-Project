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
#include "Game/Subsystem/SFDataTableManager.h"
#include "InGame/SFInGameHUDWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Units/Commander/SFCommander.h"
//#include "NavigationSystem.h"
#include "Units/Interfaces/Movable.h"
#include "SFRTSCameraPawn.h"
#include "Logging/SFLog.h"

ASFInGamePlayerController::ASFInGamePlayerController()
{
    bShowMouseCursor = true;
    bEnableClickEvents = true;
    bEnableMouseOverEvents = true;
    
    DefaultMouseCursor = EMouseCursor::Default;
    
    PlacementComponent= CreateDefaultSubobject<UBuildingPlacement>(TEXT("PlacementComponent"));
}

bool ASFInGamePlayerController::SpawnBuildingFunction(const FName& InRowName)
{
    // 자원이 충분한가 아닌가 검사 로직. 가능구역2
    if (IsValid(SpawnedBuildingRef))
    {
        UE_LOG(LogTemp, Error, TEXT("Exist Spawned Building Ref"));
        return false;
    }

    USFDataTableManager* DataManager = GetGameInstance()->GetSubsystem<USFDataTableManager>();
    if (!IsValid(DataManager)) return false;
    
    const FBuildingAppearance* Row = DataManager->GetDataRow<FBuildingAppearance>(EDataTableType::Building, InRowName);
    if (!Row)
    {
        UE_LOG(LogTemp, Error, TEXT("No build table for %s"), *InRowName.ToString());
        return false;
    }
    
#pragma region CheckResources 
    // FName의 첫번째 티어의 정보 불러오기
    // 나중에 2티어부터 건설할수있게되면 수정이 필요한지점이다
    const FBuildingTier* FirstTier = Row->FindTier(0);
    if (!FirstTier) return false;
    
    if (const ASFGameState* SFGS = GetWorld()->GetGameState<ASFGameState>())
    {
        if (!SFGS->CanAfford(FirstTier->Construction.ConstructCost))
        {
            SF_LOG(Warning, "[SFPC]: 자원 부족: %s", *InRowName.ToString());
            return false;
        }
    }
#pragma endregion 
    
    FTransform Trans;
    Trans.SetLocation(GetPawn()->GetActorLocation());
    FRotator Rot(0.f, 90.f, 0.f);
    Trans.SetRotation(Rot.Quaternion());
    
    UClass* SpawnClass = Row->BuildingClass.LoadSynchronous();     // ★ADD
    if (!SpawnClass)
    {
        UE_LOG(LogTemp, Error, TEXT("SpawnBuildingFunction: BuildingClass is None. Row=%s"),
            *InRowName.ToString());
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
    
    DeferredActor->SetRowName(InRowName);
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

    if (!SFGS->CanAfford(NeedConstructCost))
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
    SFGS->TrySpendCost(NeedConstructCost);
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
            &ASFInGamePlayerController::
            HandleSelectInput
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

void ASFInGamePlayerController::
HandleSelectInput()
{
    SF_LOG(
        Log,
        "[SFInGamePlayerController] "
        "SelectAction triggered."
    );

    FHitResult Hit;

    const ETraceTypeQuery TraceType =
        UEngineTypes::ConvertToTraceType(
            ECC_Visibility
        );

    AActor* NewSelected = nullptr;

    if (
        GetHitResultUnderCursorByChannel(
            TraceType,
            false,
            Hit
        )
        )
    {
        AActor* HitActor = Hit.GetActor();

        if (
            IsValid(HitActor)
            && HitActor->GetClass()->ImplementsInterface(
                USFSelectable::StaticClass()
            )
            )
        {
            NewSelected = HitActor;
        }
    }

    SetSelectedActor(NewSelected);

    SF_LOG(
        Log,
        "[SFInGamePlayerController] "
        "Broadcasting OnSelectInput."
    );

    OnSelectInput.Broadcast();
}
void ASFInGamePlayerController::HandleMoveInput()
{
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

    AActor* Target = SelectedActor.Get();

    if (!IsValid(Target))
    {
        return;
    }

    if (!Target->GetClass()->ImplementsInterface(UMovable::StaticClass()))
    {
        return;
    }

    IMovable::Execute_MoveToLocation(Target, Destination);

    SF_LOG(Log, "[SFInGamePlayerController] Move command | Target: %s | Location: %s", *Target->GetName(), *Destination.ToString());
}

void ASFInGamePlayerController::HandleToggleSettingsInput()
{
    if (!IsValid(InGameHUDWidget))
    {
        return;
    }

    InGameHUDWidget->ToggleSettings();
}