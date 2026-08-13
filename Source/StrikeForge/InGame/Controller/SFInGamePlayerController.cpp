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
#include "Game/Subsystem/SFDataTableManager.h"
#include "InGame/SFInGameHUDWidget.h"
#include "Units/Commander/SFCommander.h"
#include "NavigationSystem.h"
#include "Units/Interfaces/Movable.h"
#include "Game/GameState/SFGameState.h"
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

void ASFInGamePlayerController::SpawnBuildingFunction(const FName& InRowName)
{
    // 자원이 충분한가 아닌가 검사 로직. 가능구역2
    if (IsValid(SpawnedBuildingRef))
    {
        UE_LOG(LogTemp, Error, TEXT("Exist Spawned Building Ref"));
        return;
    }
    USFDataTableManager* DataManager = GetGameInstance()->GetSubsystem<USFDataTableManager>();
    if (!IsValid(DataManager)) return;
    
    const FBuildingAppearance* Row = DataManager->GetDataRow<FBuildingAppearance>(EDataTableType::Building, InRowName);
    if (!Row)
    {
        UE_LOG(LogTemp, Error, TEXT("No build table for %s"), *InRowName.ToString());
        return;
    }
    
    FTransform Trans;
    Trans.SetLocation(GetPawn()->GetActorLocation());
    FRotator Rot(0.f, 90.f, 0.f);
    FVector Scaled(1.f, 1.f, 1.f);
    Trans.SetRotation(Rot.Quaternion());
    Trans.SetScale3D(Scaled);
    
    ASFBaseBuilding* DeferredActor = GetWorld()->SpawnActorDeferred<ASFBaseBuilding>(
        Row->BuildingClass,
        Trans,
        nullptr,
        nullptr,
        ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn
        );
    DeferredActor->SetRowName(InRowName);
    // deferred spawn finished
    DeferredActor->FinishSpawning(Trans);
    
    // gird init. preivew mode init.
    //DeferredActor->SetBuildingState(EBuildingState::Preview);
    PlacementComponent->StartPlacementMode(DeferredActor);
    SpawnedBuildingRef = DeferredActor;
}

void ASFInGamePlayerController::ClearSpawnedBuildingRef()
{
    if (IsValid(SpawnedBuildingRef))
    {
        SpawnedBuildingRef->Destroy();
        SpawnedBuildingRef = nullptr;
    }
}

void ASFInGamePlayerController::NotifyBuilderArrived() // start construct =====================================
{
    // ★CHANGED: SpawnedBuildingRef → PendingConstructionBuilding
    if (IsValid(PendingConstructionBuilding))
    {
        PendingConstructionBuilding->SetBuildingState(EBuildingState::UnderConstruction);
    }

    // 건물은 파괴하지 않는다. 건설이 진행되어야 하므로 참조만 놓는다.
    PendingConstructionBuilding = nullptr;
}

void ASFInGamePlayerController::CancelPendingConstruction()
{
    if (!IsValid(PendingConstructionBuilding))
    {
        return;
    }

    SF_LOG(Log, "[SFPC] 건설 명령 취소: %s", *PendingConstructionBuilding->GetName());

    // TODO: 자원 환불 (자원 차감을 넣은 뒤)
    // TODO: 그리드 점유 해제 (TryRegistBuildingGrid 를 연결한 뒤)

    PendingConstructionBuilding->Destroy();
    PendingConstructionBuilding = nullptr;
}

void ASFInGamePlayerController::SetSelectedActor(AActor* InActor)
{
    SelectedActor = InActor;
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

void ASFInGamePlayerController::
ConfigureMouseInput()
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

void ASFInGamePlayerController::
RegisterBaseMappingContext()
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
    // ★CHANGED: 선택만 담당한다. 이동 명령은 HandleMoveInput 으로 옮겼다.
    FHitResult Hit;
    const ETraceTypeQuery TraceType = UEngineTypes::ConvertToTraceType(ECC_Visibility);

    AActor* NewSelected = nullptr;
    if (GetHitResultUnderCursorByChannel(TraceType, false, Hit))
    {
        AActor* HitActor = Hit.GetActor();
        if (IsValid(HitActor) &&
            HitActor->GetClass()->ImplementsInterface(USFSelectable::StaticClass()))
        {
            NewSelected = HitActor;
        }
    }

    SetSelectedActor(NewSelected);   // 빈 곳을 클릭하면 선택 해제된다
    OnSelectInput.Broadcast();
}

void ASFInGamePlayerController::HandleMoveInput()
{
    FHitResult Hit;
    if (!GetHitResultUnderCursor(ECC_LANDSCAPE, false, Hit)) return;

    OnMoveInput.Broadcast(Hit.Location);

    // ★ADD: 선택된 액터에게 이동 명령 (HandleSelectInput 에서 옮겨옴)
    AActor* Target = SelectedActor.Get();
    if (!IsValid(Target)) return;
    if (!Target->GetClass()->ImplementsInterface(UMovable::StaticClass())) return;

    IMovable::Execute_MoveToLocation(Target, Hit.Location);
}

void ASFInGamePlayerController::HandleConfirmBuildInput()
{
    OnConfirmBuildInput.Broadcast();
    UE_LOG(LogTemp, Warning, TEXT("BuildingMode:: Clicked."));

    if (!IsValid(SpawnedBuildingRef))
    {
        return;
    }

    PlacementComponent->ConfirmPlacementMode();

    ASFCommander* Commander = MyCommander.Get();
    if (!IsValid(Commander))
    {
        SF_LOG(Error, "[SFPC] 커맨더가 등록되지 않아 건설을 시작할 수 없습니다.");
        ClearSpawnedBuildingRef();
        SetSFControlMode(ESFControlMode::Gameplay);
        return;
    }

    // ★ADD: 이전 건설 명령이 남아 있으면 먼저 취소한다.
    CancelPendingConstruction();

    // ★ADD: 고스트 → 대기 슬롯으로 소유권 이전.
    //        SpawnedBuildingRef 를 비워야 다음 건물을 고를 수 있고,
    //        빌드 패널을 닫아도 ClearSpawnedBuildingRef 에 파괴되지 않는다.
    PendingConstructionBuilding = SpawnedBuildingRef;
    SpawnedBuildingRef = nullptr;

    UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
    if (ensure(NavSys && NavSys->GetDefaultNavDataInstance()))
    {
        bool bCanBuild = false;

        const float DistTolerance = 0.1f;   // 실제 건설위치와 근접한 내비메시 간 건설위치 오차 허용범위
        const FVector QueryExt(100.f, 100.0f, 1000.f);
        FVector BuildInLoc = PendingConstructionBuilding->GetActorLocation();
        FNavLocation BuildCheckLoc;

        do
        {
            // 1. 건설좌표가 NavMesh위에 투영이 가능한가?
            if (!NavSys->ProjectPointToNavigation(BuildInLoc, BuildCheckLoc, QueryExt))
            {
                break;
            }

            // 2. Z축으로 라인트레이싱을 했을 때 유효한 값을 반환하는가?
            FHitResult LandscapeTraceHit;
            if (!GetWorld()->LineTraceSingleByChannel(LandscapeTraceHit, BuildInLoc + FVector::UpVector * 10.f, BuildInLoc - FVector::UpVector * 10.f, ECC_LANDSCAPE))
            {
                break;
            }

            // 3. 실제 건설좌표와 Z축 라인트레이싱 결과값이 오차범위 내인가?
            if (FVector::DistXY(BuildInLoc, LandscapeTraceHit.Location) >= DistTolerance)
            {
                break;
            }

            // TODO : 다른 지형이나 폴리지와의 콜리전 검사를 로직 추가
            //
            
            // 모든 검사를 통과하면 건설가능
            bCanBuild = true;
        } while (false);

        if (bCanBuild)
        {
            // ★CHANGED: SpawnedBuildingRef → PendingConstructionBuilding
            Commander->MoveToBuild(
                PendingConstructionBuilding->GetActorLocation(),
                PendingConstructionBuilding->GetBuildBounds(),
                PendingConstructionBuilding->GetActorRotation().Yaw);
        }
        else
        {
            // 추후 UI 메세지 출력으로 수정
            GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Red, TEXT("Build Error!!!"));
        }
    }

    SetSFControlMode(ESFControlMode::Gameplay);
}

void ASFInGamePlayerController::HandleCancelBuildInput()
{
    OnCancelBuildInput.Broadcast();

    // ★ADD: 배치 중이던 고스트 정리 + 컨트롤 모드 복귀
    //        IA_SFCancelBuild 는 IMC_SFBuildPlacement 에만 있으므로
    //        배치 중에만 발동한다. 확정된 건설은 건드리지 않는다.
    PlacementComponent->ConfirmPlacementMode();
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