#include "SFInGameHUDWidget.h"

#include "InGame/Controller/SFInGamePlayerController.h"
#include "Interfaces/Units/Targetable.h"
#include "InGame/HUD/Tutorial/SFStageIntroWidget.h"
#include "Units/Commander/SFCommander.h"
#include "Units/Unit/UnitBase.h"
#include "InGame/HUD/Tutorial/SFTutorialSubsystem.h"
#include "Interfaces/Building/Productable.h"
#include "InputCoreTypes.h"
#include "Buildings/Building/ProductBuilding/SFProductionBuilding.h"
#include "SFSelectInfo/SFSelectable.h"
#include "SFSelectInfo/SFSelectionPanelWidget.h"
#include "SFSelectInfo/SFSelectionUIDataRow.h"
#include "SFSelectInfo/SFMultiSelectionPanelWidget.h"

#include "HUD/Production/SFProductionBuildingPanelWidget.h"
#include "UI/ResultWidget.h"

#include "HUD/Portrait/SFUnitPortraitActor.h"
#include "HUD/ControlGroup/SFControlGroupWidget.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/Widget.h"

#include "Game/Subsystem/SFDataTableManager.h"
#include "Buildings/Building/SFBaseBuilding.h"
#include "ActorComponent/HP/HealthComponent.h"
#include "ActorComponent/Combat/CombatTargetComponent.h"

#include "CommonActivatableWidget.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

#include "Algo/Reverse.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Buildings/Resource/ResourceBase.h"
#include "Buildings/Resource/Interface/SFMinable.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/DecalComponent.h"
#include "Components/StaticMeshComponent.h"

#include "Engine/EngineTypes.h"
#include "EngineUtils.h"
#include "Engine/DataTable.h"

#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Game/GameInstance/SFGameInstance.h"
#include "Game/GameState/SFGameState.h"

#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Math/RotationMatrix.h"

#include "Kismet/GameplayStatics.h"

#include "TimerManager.h"

#include "Logging/SFLog.h"


void USFInGameHUDWidget::NativeConstruct()
{
    Super::NativeConstruct();

    ControlGroups.SetNum(6);

    ASFInGamePlayerController* PlayerController =
        Cast<ASFInGamePlayerController>(
            GetOwningPlayer()
        );

    if (!IsValid(PlayerController))
    {
        SF_LOG(
            Error,
            "[SFInGameHUDWidget] "
            "SFInGamePlayerController is invalid."
        );
        return;
    }

    UnitPortraitActor =
        Cast<ASFUnitPortraitActor>(
            UGameplayStatics::GetActorOfClass(
                GetWorld(),
                ASFUnitPortraitActor::StaticClass()
            )
        );

    if (!IsValid(UnitPortraitActor))
    {
        SF_LOG(
            Warning,
            "[SFInGameHUDWidget] "
            "UnitPortraitActor was not found."
        );
    }

    if (IsValid(ProductionBuildingPanel))
    {
        ProductionBuildingPanel->OnProductionResourceWarningRequested.RemoveDynamic(this, &USFInGameHUDWidget::HandleProductionResourceWarningRequested);

        ProductionBuildingPanel->OnProductionResourceWarningRequested.AddUniqueDynamic(this, &USFInGameHUDWidget::HandleProductionResourceWarningRequested);
    }

    PlayerController->OnSelectInput.AddUniqueDynamic(
        this,
        &USFInGameHUDWidget::HandleSelectInput
    );

    PlayerController->OnSelectionDragStarted.RemoveDynamic(
        this,
        &USFInGameHUDWidget::HandleSelectionDragStarted
    );

    PlayerController->OnSelectionDragStarted.AddUniqueDynamic(
        this,
        &USFInGameHUDWidget::HandleSelectionDragStarted
    );

    PlayerController->OnSelectionDragCompleted.RemoveDynamic(
        this,
        &USFInGameHUDWidget::HandleSelectionDragCompleted
    );

    PlayerController->OnSelectionDragCompleted.AddUniqueDynamic(
        this,
        &USFInGameHUDWidget::HandleSelectionDragCompleted
    );

    if (IsValid(SelectionDragBox))
    {
        SelectionDragBox->SetVisibility(
            ESlateVisibility::Collapsed
        );
    }
    RefreshBuildModeOpenButtonVisibility();
    UnbindSelectedProductionBuilding();

    if (IsValid(SelectionPanel))
    {
        SelectionPanel->OnTierUpgradeRequested.RemoveDynamic(
            this,
            &USFInGameHUDWidget::HandleTierUpgradeRequested
        );

        SelectionPanel->OnTierUpgradeRequested.AddUniqueDynamic(
            this,
            &USFInGameHUDWidget::HandleTierUpgradeRequested
        );

        SelectionPanel->OnRallyPointRequested.RemoveDynamic(
            this,
            &USFInGameHUDWidget::HandleRallyPointRequested
        );

        SelectionPanel->OnRallyPointRequested.AddUniqueDynamic(
            this,
            &USFInGameHUDWidget::HandleRallyPointRequested
        );

        SF_LOG(
            Log,
            "[SFInGameHUDWidget] "
            "Tier upgrade delegate binding completed."
        );
    }

    if (IsValid(MultiSelectionPanel))
    {
        MultiSelectionPanel->OnPrimarySelectionRequested.RemoveDynamic(
            this,
            &USFInGameHUDWidget::HandlePrimarySelectionRequested
        );

        MultiSelectionPanel->OnPrimarySelectionRequested.AddUniqueDynamic(
            this,
            &USFInGameHUDWidget::HandlePrimarySelectionRequested
        );

        MultiSelectionPanel->OnRemoveSelectionRequested.RemoveDynamic(
            this,
            &USFInGameHUDWidget::HandleMultiSelectionRemoveRequested
        );

        MultiSelectionPanel->OnRemoveSelectionRequested.AddUniqueDynamic(
            this,
            &USFInGameHUDWidget::HandleMultiSelectionRemoveRequested
        );

        MultiSelectionPanel->ClearSelections();

        SF_LOG(Log, "[SFInGameHUDWidget] Multi selection delegate binding completed.");
    }

    // 튜토리얼이 목표 대기에서 돌아올 때 인트로를 다시 여는 것은 HUD 의 일이다.
    // 여는 절차(HUD 숨김·일시정지·입력 모드)가 전부 여기 있기 때문이다.
    //
    // 인트로를 시작하기 전에 걸어둔다. 첫 노드가 곧바로 대기 노드일 수도 있다.
    if (USFTutorialSubsystem* TutorialSubsystem = USFTutorialSubsystem::Get(this))
    {
        TutorialSubsystem->OnResumeRequested.RemoveDynamic(
            this,
            &USFInGameHUDWidget::HandleTutorialResumeRequested
        );

        TutorialSubsystem->OnResumeRequested.AddUniqueDynamic(
            this,
            &USFInGameHUDWidget::HandleTutorialResumeRequested
        );
    }

    // 인트로는 로딩이 끝난 뒤에 연다.
    //
    // 예전에는 여기서 곧바로 다음 프레임 타이머를 걸었는데, 그 시점은 아직
    // 스테이지 에셋을 비동기로 로딩하는 중이다. 로딩 화면 위로 인트로가
    // 겹치고, 인트로가 게임을 일시정지시키므로 로딩까지 멈출 수 있다.
    BoundStageGameState = GetWorld()
        ? GetWorld()->GetGameState<ASFGameState>()
        : nullptr;

    if (IsValid(BoundStageGameState))
    {
        BoundStageGameState->OnStageStateChanged.AddUniqueDynamic(
            this,
            &USFInGameHUDWidget::HandleStageStateChanged
        );

        if (BoundStageGameState->IsStagePlaying())
        {
            // HUD 가 늦게 만들어져 이미 시작된 경우.
            // 이걸 안 보면 그 판은 인트로가 통째로 안 나온다.
            HandleStageStateChanged(ESFStageState::Playing);
        }
        else if (bHideHUDUntilStagePlaying)
        {
            // 원래 가시성을 기억해 두고 숨긴다.
            // 고정 값으로 되돌리면 WBP 가 설정한 클릭 판정이 바뀐다.
            VisibilityBeforeStageLoading = GetVisibility();
            SetVisibility(ESlateVisibility::Collapsed);
        }
    }
    else
    {
        // GameState 를 못 얻는 판(전투 테스트 맵 등)에서는 예전처럼 바로 연다.
        SF_LOG(
            Warning,
            "[SFInGameHUDWidget] GameState 를 찾지 못해 인트로를 즉시 시작합니다."
        );

        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().SetTimerForNextTick(
                FTimerDelegate::CreateUObject(
                    this,
                    &USFInGameHUDWidget::TryStartStageIntro
                )
            );
        }
    }
}

void USFInGameHUDWidget::NativeDestruct()
{
    HideAllSelectionIndicators();

    if (IsValid(ActiveBuildingModeWidget))
    {
        ActiveBuildingModeWidget
            ->OnDeactivated()
            .RemoveAll(this);
    }

    UnbindSelectedActorsDestroyed();

    UnbindControlGroupActorsDestroyed();

    UnbindSelectedHealthDelegate();

    ASFInGamePlayerController* PlayerController =
        Cast<ASFInGamePlayerController>(
            GetOwningPlayer()
        );

    if (IsValid(PlayerController))
    {
        PlayerController->OnSelectInput.RemoveDynamic(
            this,
            &USFInGameHUDWidget::HandleSelectInput
        );

        PlayerController->OnSelectionDragStarted.RemoveDynamic(
            this,
            &USFInGameHUDWidget::HandleSelectionDragStarted
        );

        PlayerController->OnSelectionDragCompleted.RemoveDynamic(
            this,
            &USFInGameHUDWidget::HandleSelectionDragCompleted
        );
    }

    if (IsValid(SelectionPanel))
    {
        SelectionPanel->OnTierUpgradeRequested.RemoveDynamic(
            this,
            &USFInGameHUDWidget::HandleTierUpgradeRequested
        );
    }

    if (IsValid(ActiveSettingsWidget))
    {
        ActiveSettingsWidget->OnDeactivated().RemoveAll(this);
    }

    if (IsValid(MultiSelectionPanel))
    {
        MultiSelectionPanel->OnPrimarySelectionRequested.RemoveDynamic(
            this,
            &USFInGameHUDWidget::HandlePrimarySelectionRequested
        );

        MultiSelectionPanel->OnRemoveSelectionRequested.RemoveDynamic(
            this,
            &USFInGameHUDWidget::HandleMultiSelectionRemoveRequested
        );
    }

    if (IsValid(ActiveStageIntroWidget))
    {
        ActiveStageIntroWidget->OnDeactivated().RemoveAll(this);
        ActiveStageIntroWidget->RemoveFromParent();
        ActiveStageIntroWidget = nullptr;
    }

    if (IsValid(BoundStageGameState))
    {
        BoundStageGameState->OnStageStateChanged.RemoveDynamic(
            this,
            &USFInGameHUDWidget::HandleStageStateChanged
        );
    }

    BoundStageGameState = nullptr;

    // HUD 가 사라지면 인트로를 다시 열어줄 사람이 없다.
    // 대기까지 취소해야 배너도 같이 정리된다.
    if (USFTutorialSubsystem* TutorialSubsystem = USFTutorialSubsystem::Get(this))
    {
        TutorialSubsystem->OnResumeRequested.RemoveDynamic(
            this,
            &USFInGameHUDWidget::HandleTutorialResumeRequested
        );

        TutorialSubsystem->CancelObjectiveWait();
    }

    Super::NativeDestruct();
}

void USFInGameHUDWidget::HandleSelectInput()
{
    SF_LOG(
        Log,
        "[SFInGameHUDWidget] Select input received."
    );

    if (IsPointerOverHUDPanel())
    {
        SF_LOG(
            Log,
            "[SFInGameHUDWidget] "
            "Selection ignored because cursor is over HUD panel."
        );

        return;
    }

    CloseBuildingMode();
    PerformSelectionTrace();
}

void USFInGameHUDWidget::HandleSelectedActorDestroyed(AActor* DestroyedActor)
{
    if (DestroyedActor == nullptr)
    {
        return;
    }

    const bool bWasPrimary =
        DestroyedActor == SelectedActor.Get();

    HideSelectionIndicator(DestroyedActor);

    SelectedActors.RemoveAll(
        [DestroyedActor](const TObjectPtr<AActor>& ActorPtr)
        {
            return
                ActorPtr.Get() == DestroyedActor ||
                !IsValid(ActorPtr.Get());
        }
    );

    if (SelectedActors.IsEmpty())
    {
        ClearSelection();
        return;
    }

    if (bWasPrimary)
    {
        UnbindSelectedHealthDelegate();

        SelectedActor = nullptr;
        SelectedInfo = FSFSelectableInfo();

        AActor* NewPrimaryActor =
            SelectedActors[0].Get();

        if (IsValid(NewPrimaryActor))
        {
            HandlePrimarySelectionRequested(
                NewPrimaryActor
            );
        }

        return;
    }

    RefreshRallyPointIndicator();
    RefreshMultiSelectionPanel();

    SF_LOG(
        Log,
        "[SFInGameHUDWidget] Destroyed actor removed from selection | Remaining: %d",
        SelectedActors.Num()
    );
}

void USFInGameHUDWidget::PerformSelectionTrace()
{
    APlayerController* PlayerController =
        GetOwningPlayer();

    if (!IsValid(PlayerController))
    {
        ClearSelection();
        return;
    }

    FHitResult HitResult;

    const ETraceTypeQuery SelectionTraceType =
        UEngineTypes::ConvertToTraceType(
            ECC_Visibility
        );

    const bool bHit =
        PlayerController->GetHitResultUnderCursorByChannel(
            SelectionTraceType,
            false,
            HitResult
        );

    if (!bHit)
    {
        ClearSelection();
        return;
    }

    AActor* HitActor =
        HitResult.GetActor();

    if (!IsValid(HitActor))
    {
        ClearSelection();
        return;
    }

    if (
        !HitActor->GetClass()->ImplementsInterface(
            USFSelectable::StaticClass()
        )
        )
    {
        ClearSelection();
        return;
    }

    if (TryHandleUnitDoubleClick(HitActor))
    {
        return;
    }

    const bool bShiftPressed =
        PlayerController->IsInputKeyDown(EKeys::LeftShift) ||
        PlayerController->IsInputKeyDown(EKeys::RightShift);

    if (bShiftPressed)
    {
        if (IsOwnedByLocalPlayer(HitActor))
        {
            AddActorToCurrentSelection(HitActor);
        }

        return;
    }

    SelectActor(HitActor);
}

void USFInGameHUDWidget::ApplySelection(
    AActor* NewSelectedActor,
    const FSFSelectableInfo& NewSelectedInfo
)
{
    if (!IsValid(NewSelectedActor))
    {
        ClearSelection();
        return;
    }

    const bool bIsBuilding =
        NewSelectedInfo.SelectableType == ESFSelectableType::MainBuilding ||
        NewSelectedInfo.SelectableType == ESFSelectableType::NormalBuilding ||
        NewSelectedInfo.SelectableType == ESFSelectableType::ProductionBuilding ||
        NewSelectedInfo.SelectableType == ESFSelectableType::ResourceBuilding;

    if (bIsBuilding)
    {
        CloseBuildingMode();
    }

    UnbindSelectedActorsDestroyed();
    UnbindSelectedHealthDelegate();
    HideAllSelectionIndicators();
    HideRallyPointIndicator();
    UnbindSelectedProductionBuilding();

    SelectedActor = NewSelectedActor;
    RefreshRallyPointIndicator();

    BindSelectedProductionBuilding(
        SelectedActor.Get()
    );

    SelectedActor->OnDestroyed.AddUniqueDynamic(
        this,
        &USFInGameHUDWidget::HandleSelectedActorDestroyed
    );

    const bool bIsCommandableUnit =
        NewSelectedInfo.SelectableType ==
        ESFSelectableType::Unit ||
        NewSelectedInfo.SelectableType ==
        ESFSelectableType::Commander;

    const bool bIsMyUnit =
        IsOwnedByLocalPlayer(
            NewSelectedActor
        );

    SelectionPanel->SetCommandButtonsVisible(
        bIsCommandableUnit && bIsMyUnit
    );

    ShowSelectionIndicator(NewSelectedActor);

    SelectedActors.Reset();
    SelectedActors.Add(NewSelectedActor);
    SelectedInfo = NewSelectedInfo;

    if (ASFInGamePlayerController* PlayerController = Cast<ASFInGamePlayerController>(GetOwningPlayer()))
    {
        PlayerController->SetSelectedActor(SelectedActor.Get());
    }

    BindSelectedHealthDelegate(SelectedActor.Get());

    RefreshBuildModeOpenButtonVisibility();
    RefreshSelectionPanels();
    RefreshMultiSelectionPanel();
    RefreshSelectedPortrait();
}

void USFInGameHUDWidget::ApplySelectionUIData(
    AActor* SourceActor,
    FSFSelectableInfo& InOutSelectableInfo
) {
    if (InOutSelectableInfo.DataRowName.IsNone())
    {
        return;
    }

    UGameInstance* GameInstance =
        GetGameInstance();

    if (!IsValid(GameInstance))
    {
        return;
    }

    USFDataTableManager* DataTableManager =
        GameInstance->GetSubsystem<USFDataTableManager>();

    if (!IsValid(DataTableManager))
    {
        return;
    }

    const FSFSelectionUIDataRow* UIData =
        DataTableManager->GetDataRow<FSFSelectionUIDataRow>(
            EDataTableType::UI,
            InOutSelectableInfo.DataRowName,
            TEXT("ApplySelectionUIData")
        );

    if (!UIData)
    {
        return;
    }

    InOutSelectableInfo.DisplayName =
        UIData->DisplayName;

    const ESFRace ActorRace =
        ResolveActorRace(SourceActor);

    InOutSelectableInfo.PortraitImage =
        ResolvePortraitImage(
            *UIData,
            ActorRace,
            InOutSelectableInfo.Tier
        );

    InOutSelectableInfo.SelectionIcon =
        UIData->SelectionIcon.LoadSynchronous();

    InOutSelectableInfo.Description =
        UIData->Description;

    InOutSelectableInfo.bShowHealth =
        UIData->bShowHealth;

    InOutSelectableInfo.bShowAttackPower =
        UIData->bShowAttackPower;

    InOutSelectableInfo.bShowMoveSpeed =
        UIData->bShowMoveSpeed;

    InOutSelectableInfo.bShowSkillIcons =
        UIData->bShowSkillIcons;
}

ESFRace USFInGameHUDWidget::ResolveActorRace(
    AActor* Actor
) const
{
    if (!IsValid(Actor))
    {
        return ESFRace::None;
    }

    const UCombatTargetComponent* CombatTarget =
        Actor->FindComponentByClass<UCombatTargetComponent>();

    if (!IsValid(CombatTarget))
    {
        return ESFRace::None;
    }

    const ASFGameState* GameState =
        GetWorld()->GetGameState<ASFGameState>();

    if (!IsValid(GameState))
    {
        return ESFRace::None;
    }

    const USFTeamState* TeamState =
        GameState->GetTeamState(
            CombatTarget->GetTeamIndex()
        );

    if (!IsValid(TeamState))
    {
        return ESFRace::None;
    }

    return TeamState->GetRace();
}

UTexture2D* USFInGameHUDWidget::ResolvePortraitImage(
    const FSFSelectionUIDataRow& UIData,
    ESFRace Race,
    int32 Tier
) const
{
    const int32 SafeTier =
        FMath::Max(1, Tier);

    // 종족과 티어가 모두 일치하는 이미지
    for (const FSFSelectionPortraitVariant& Variant :
        UIData.PortraitVariants)
    {
        if (
            Variant.Race == Race &&
            Variant.Tier == SafeTier &&
            !Variant.PortraitImage.IsNull()
            )
        {
            return Variant.PortraitImage.LoadSynchronous();
        }
    }

    // 해당 티어가 없으면 같은 종족의 1티어 이미지
    for (const FSFSelectionPortraitVariant& Variant :
        UIData.PortraitVariants)
    {
        if (
            Variant.Race == Race &&
            Variant.Tier == 1 &&
            !Variant.PortraitImage.IsNull()
            )
        {
            return Variant.PortraitImage.LoadSynchronous();
        }
    }

    // 최종 fallback
    return UIData.PortraitImage.IsNull()
        ? nullptr
        : UIData.PortraitImage.LoadSynchronous();
}

void USFInGameHUDWidget::RefreshSelectionPanels()
{
    const bool bHasValidSelection =
        IsValid(SelectedActor) &&
        SelectedInfo.SelectableType != ESFSelectableType::None;

    if (IsValid(SelectionPanel))
    {
        if (bHasValidSelection)
        {
            SelectionPanel->ShowSelection(SelectedInfo);
        }
        else
        {
            SelectionPanel->ClearSelection();
        }

        const bool bCanTierUp =
            bHasValidSelection &&
            (
                SelectedInfo.SelectableType ==
                ESFSelectableType::MainBuilding ||
                SelectedInfo.SelectableType ==
                ESFSelectableType::ProductionBuilding
                ) &&
            IsOwnedByLocalPlayer(SelectedActor.Get());

        SelectionPanel->SetTierUpButtonVisible(
            bCanTierUp
        );

        FResources UpgradeCost{};
        bool bCanAffordUpgrade = false;

        if (bCanTierUp)
        {
            if (ASFBaseBuilding* SelectedBuilding =
                Cast<ASFBaseBuilding>(SelectedActor.Get()))
            {
                UpgradeCost =
                    SelectedBuilding->GetUpgradeCost();

                if (ASFGameState* GameState =
                    GetWorld()->GetGameState<ASFGameState>())
                {
                    bCanAffordUpgrade =
                        GameState->CanAffordResource(
                            SelectedBuilding->GetTeamIndex(),
                            UpgradeCost
                        );
                }
            }
        }

        SelectionPanel->SetUpgradeCost(
            UpgradeCost
        );

        SelectionPanel->SetUpgradeAffordability(
            bCanAffordUpgrade
        );

        const bool bCanSetRallyPoint =
            bHasValidSelection &&
            SelectedInfo.SelectableType ==
            ESFSelectableType::ProductionBuilding &&
            IsOwnedByLocalPlayer(SelectedActor.Get());

        SelectionPanel->SetRallyPointButtonVisible(
            bCanSetRallyPoint
        );
    }

    RefreshSelectedResourceInfo();

    FResources UpgradeCost{};

    if (ASFBaseBuilding* SelectedBuilding =
        Cast<ASFBaseBuilding>(SelectedActor.Get()))
    {
        UpgradeCost =
            SelectedBuilding->GetUpgradeCost();
    }

    if (IsValid(SelectionPanel))
    {
        SelectionPanel->SetUpgradeCost(
            UpgradeCost
        );
    }

    if (!IsValid(ProductionBuildingPanel))
    {
        return;
    }

    const bool bIsBuildingModeActive =
        IsValid(ActiveBuildingModeWidget) &&
        ActiveBuildingModeWidget->IsActivated();

    if (bIsBuildingModeActive)
    {
        ProductionBuildingPanel->HideProductionBuilding();
        return;
    }

    if (SelectedActors.Num() > 1)
    {
        ProductionBuildingPanel->HideProductionBuilding();
        return;
    }

    if (
        bHasValidSelection &&
        SelectedInfo.SelectableType ==
        ESFSelectableType::ProductionBuilding &&
        IsOwnedByLocalPlayer(SelectedActor.Get())
        )
    {
        ProductionBuildingPanel->ShowProductionBuilding(
            SelectedActor.Get()
        );
        return;
    }

    ProductionBuildingPanel->HideProductionBuilding();
}

void USFInGameHUDWidget::RefreshSelectedResourceInfo()
{
    if (!IsValid(SelectionPanel))
    {
        return;
    }

    AActor* Actor = SelectedActor.Get();

    if (!IsValid(Actor))
    {
        SelectionPanel->ClearMiningResourceAmount();
        return;
    }

    // 광물/에테르 광맥
    if (AResourceBase* ResourceNode =
        Cast<AResourceBase>(Actor))
    {
        SelectionPanel->SetMiningResourceAmount(
            ResourceNode->GetResourceDeposit()
        );

        return;
    }

    // 광산 건물
    if (Actor->GetClass()->ImplementsInterface(
        UMinable::StaticClass()
    ))
    {
        if (IMinable* Minable = Cast<IMinable>(Actor))
        {
            const FMinableData MinableData =
                Minable->GetMinableData();

            SelectionPanel->SetMiningResourceAmount(
                MinableData.ResourceLeftAmount
            );

            return;
        }
    }

    SelectionPanel->ClearMiningResourceAmount();
}

void USFInGameHUDWidget::RefreshSelectedPortrait()
{
    if (!IsValid(UnitPortraitActor))
    {
        return;
    }

    const bool bUses3DPortrait =
        SelectedInfo.SelectableType == ESFSelectableType::Commander ||
        SelectedInfo.SelectableType == ESFSelectableType::Unit;

    if (!bUses3DPortrait)
    {
        UnitPortraitActor->ClearPortrait();
        return;
    }

    ACharacter* SelectedCharacter =
        Cast<ACharacter>(SelectedActor.Get());

    if (!IsValid(SelectedCharacter))
    {
        UnitPortraitActor->ClearPortrait();
        return;
    }

    USkeletalMeshComponent* SelectedMeshComponent =
        SelectedCharacter->GetMesh();

    if (!IsValid(SelectedMeshComponent))
    {
        UnitPortraitActor->ClearPortrait();
        return;
    }

    USkeletalMesh* SelectedSkeletalMesh =
        SelectedMeshComponent->GetSkeletalMeshAsset();

    if (!IsValid(SelectedSkeletalMesh))
    {
        UnitPortraitActor->ClearPortrait();
        return;
    }

    UnitPortraitActor->ApplyPortrait(
        SelectedSkeletalMesh,
        SelectedInfo.PortraitAnimation
    );
}

void USFInGameHUDWidget::ClearSelection()
{

    HideAllSelectionIndicators();
    HideRallyPointIndicator();

    if (IsValid(UnitPortraitActor))
    {
        UnitPortraitActor->ClearPortrait();
    }

    UnbindSelectedActorsDestroyed();
    UnbindSelectedHealthDelegate();
    UnbindSelectedProductionBuilding();

    SelectedActor = nullptr;
    SelectedActors.Reset();
    SelectedInfo = FSFSelectableInfo();

    if (ASFInGamePlayerController* PlayerController = Cast<ASFInGamePlayerController>(GetOwningPlayer()))
    {
        PlayerController->SetSelectedActor(nullptr);
    }

    RefreshBuildModeOpenButtonVisibility();

    if (IsValid(SelectionPanel))
    {
        SelectionPanel->ClearSelection();
    }

    if (IsValid(ProductionBuildingPanel))
    {
        ProductionBuildingPanel->HideProductionBuilding();
    }

    if (IsValid(MultiSelectionPanel))
    {
        MultiSelectionPanel->ClearSelections();
    }
}

void USFInGameHUDWidget::BindSelectedActorsDestroyed()
{
    for (const TObjectPtr<AActor>& ActorPtr : SelectedActors)
    {
        AActor* Actor = ActorPtr.Get();

        if (!IsValid(Actor))
        {
            continue;
        }

        Actor->OnDestroyed.AddUniqueDynamic(
            this,
            &USFInGameHUDWidget::HandleSelectedActorDestroyed
        );
    }
}

void USFInGameHUDWidget::UnbindSelectedActorsDestroyed()
{
    for (const TObjectPtr<AActor>& ActorPtr : SelectedActors)
    {
        AActor* Actor = ActorPtr.Get();

        if (!IsValid(Actor))
        {
            continue;
        }

        Actor->OnDestroyed.RemoveDynamic(
            this,
            &USFInGameHUDWidget::HandleSelectedActorDestroyed
        );
    }

    if (IsValid(SelectedActor))
    {
        SelectedActor->OnDestroyed.RemoveDynamic(
            this,
            &USFInGameHUDWidget::HandleSelectedActorDestroyed
        );
    }
}

void USFInGameHUDWidget::ToggleBuildingMode()
{
    const bool bIsActive =
        IsValid(ActiveBuildingModeWidget) &&
        ActiveBuildingModeWidget->IsActivated();

    if (bIsActive)
    {
        CloseBuildingMode();
        return;
    }

    OpenBuildingMode();
}

void USFInGameHUDWidget::OpenBuildingMode()
{
    if (
        !IsValid(BuildingUIStack) ||
        !BuildingModeWidgetClass
        )
    {
        return;
    }

    const bool bIsSelectedBuilding =
        SelectedInfo.SelectableType == ESFSelectableType::MainBuilding ||
        SelectedInfo.SelectableType == ESFSelectableType::NormalBuilding ||
        SelectedInfo.SelectableType == ESFSelectableType::ProductionBuilding ||
        SelectedInfo.SelectableType == ESFSelectableType::ResourceBuilding;

    if (bIsSelectedBuilding &&
        !IsOwnedByLocalPlayer(SelectedActor.Get()))
    {
        return;
    }

    if (
        IsValid(ActiveBuildingModeWidget) &&
        ActiveBuildingModeWidget->IsActivated()
        )
    {
        return;
    }

    ActiveBuildingModeWidget =
        BuildingUIStack->AddWidget<UCommonActivatableWidget>(
            BuildingModeWidgetClass
        );

    if (!IsValid(ActiveBuildingModeWidget))
    {
        return;
    }

    if (IsValid(BuildModeOpenSound))
    {
        UGameplayStatics::PlaySound2D(
            this,
            BuildModeOpenSound
        );
    }

    ActiveBuildingModeWidget->OnDeactivated().RemoveAll(this);

    ActiveBuildingModeWidget->OnDeactivated().AddUObject(
        this,
        &USFInGameHUDWidget::HandleBuildingModeDeactivated
    );

    RefreshBuildModeOpenButtonVisibility();
    RefreshSelectionPanels();
    RefreshMultiSelectionPanel();
}

void USFInGameHUDWidget::CloseBuildingMode()
{
    if (!IsValid(ActiveBuildingModeWidget))
    {
        return;
    }

    ActiveBuildingModeWidget->DeactivateWidget();
}

void USFInGameHUDWidget::HandleBuildingModeDeactivated()
{
    if (IsValid(BuildModeCloseSound))
    {
        UGameplayStatics::PlaySound2D(
            this,
            BuildModeCloseSound
        );
    }

    if (IsValid(ActiveBuildingModeWidget))
    {
        ActiveBuildingModeWidget->OnDeactivated().RemoveAll(this);
    }

    ActiveBuildingModeWidget = nullptr;

    RefreshBuildModeOpenButtonVisibility();
    RefreshSelectionPanels();
    RefreshMultiSelectionPanel();

    ASFInGamePlayerController* PlayerController =
        Cast<ASFInGamePlayerController>(
            GetOwningPlayer()
        );

    if (!IsValid(PlayerController))
    {
        return;
    }

    PlayerController->ClearSpawnedBuildingRef();
    PlayerController->SetSFControlMode(
        ESFControlMode::Gameplay
    );
}

void USFInGameHUDWidget::RefreshBuildModeOpenButtonVisibility()
{
    if (!IsValid(BuildModeOpenButton))
    {
        return;
    }

    const bool bIsBuildingSelected =
        SelectedInfo.SelectableType ==
        ESFSelectableType::MainBuilding ||
        SelectedInfo.SelectableType ==
        ESFSelectableType::NormalBuilding ||
        SelectedInfo.SelectableType ==
        ESFSelectableType::ProductionBuilding ||
        SelectedInfo.SelectableType ==
        ESFSelectableType::ResourceBuilding;

    const bool bIsBuildingModeActive =
        IsValid(ActiveBuildingModeWidget) &&
        ActiveBuildingModeWidget->IsActivated();

    BuildModeOpenButton->SetVisibility(
        bIsBuildingSelected ||
        bIsBuildingModeActive
        ? ESlateVisibility::Collapsed
        : ESlateVisibility::Visible
    );
}

void USFInGameHUDWidget::ShowResultWidget(
    bool bIsWin
)
{
    if (!IsValid(ResultWidget))
    {
        if (!ResultWidgetClass)
        {
            SF_LOG(
                Error,
                "[SFInGameHUDWidget] "
                "ResultWidgetClass is not assigned."
            );
            return;
        }

        ResultWidget =
            CreateWidget<UResultWidget>(
                GetOwningPlayer(),
                ResultWidgetClass
            );

        if (!IsValid(ResultWidget))
        {
            SF_LOG(
                Error,
                "[SFInGameHUDWidget] "
                "Failed to create ResultWidget."
            );
            return;
        }

        ResultWidget->AddToViewport(100);
    }

    ResultWidget->SetResultState(
        bIsWin
    );

    SetVisibility(
        ESlateVisibility::Collapsed
    );
}

void USFInGameHUDWidget::SelectActor(AActor* Actor)
{
    if (!IsValid(Actor))
    {
        return;
    }

    if (!Actor->GetClass()->ImplementsInterface(USFSelectable::StaticClass()))
    {
        return;
    }

    FSFSelectableInfo SelectableInfo = ISFSelectable::Execute_GetSelectableInfo(Actor);

    // const bool bIsBuilding =
    //     SelectableInfo.SelectableType == ESFSelectableType::MainBuilding ||
    //     SelectableInfo.SelectableType == ESFSelectableType::ProductionBuilding ||
    //     SelectableInfo.SelectableType == ESFSelectableType::ResourceBuilding;
    //
    // if (bIsBuilding)
    // {
    //     const FHealthData HealthData = ISFSelectable::Execute_GetHealthData(Actor);
    //     SelectableInfo.Tier = HealthData.CurrentTier + 1;
    // }

    ApplySelectionUIData(Actor, SelectableInfo);
    ApplySelection(Actor, SelectableInfo);

    // 선택음은 여기서 한 번만 울린다.
    //
    // 원래는 ASFInGamePlayerController::PlaySelectionSound 를 부르면 되지만
    // 그 함수가 private 이라 C++ 에서는 접근할 수 없다. UFUNCTION 이라
    // BP 에서만 보이는 상태다. 컨트롤러는 담당자 파일이라 손대지 않고,
    // 같은 내용을 여기서 직접 수행한다.
    //
    // 위에서 ISFSelectable 구현 여부를 이미 걸렀으므로 다시 검사하지 않는다.
    // DT_Units 의 Selection Sound 를 비워둔 유닛은 소리가 null 이라 조용히
    // 넘어간다 — 소리를 지정하지 않은 유닛이 무음인 것은 정상 동작이다.
    if (USoundBase* SelectionSound =
        ISFSelectable::Execute_GetSelectionSound(Actor))
    {
        UGameplayStatics::PlaySoundAtLocation(
            this,
            SelectionSound,
            Actor->GetActorLocation()
        );
    }
}

void USFInGameHUDWidget::SelectAllOwnedUnits()
{
    UWorld* World = GetWorld();

    if (!IsValid(World))
    {
        return;
    }

    TArray<AActor*> FoundUnits;

    for (TActorIterator<AUnitBase> Iterator(World); Iterator; ++Iterator)
    {
        AUnitBase* Unit = *Iterator;

        if (!IsValid(Unit) || !IsOwnedByLocalPlayer(Unit))
        {
            continue;
        }

        if (!Unit->GetClass()->ImplementsInterface(USFSelectable::StaticClass()))
        {
            continue;
        }

        if (Unit->GetCombatState() == EUnitCombatState::Dead)
        {
            continue;
        }

        const FSFSelectableInfo SelectableInfo =
            ISFSelectable::Execute_GetSelectableInfo(Unit);

        if (SelectableInfo.SelectableType != ESFSelectableType::Unit &&
            SelectableInfo.SelectableType != ESFSelectableType::Commander)
        {
            continue;
        }

        FoundUnits.Add(Unit);
    }

    FoundUnits.Sort(
        [](const AActor& Left, const AActor& Right)
        {
            return Left.GetName() < Right.GetName();
        }
    );

    if (FoundUnits.IsEmpty())
    {
        ClearSelection();
        SF_LOG(Log, "[SFInGameHUDWidget] Select-all found no living units or commander.");
        return;
    }

    ApplyMultipleSelection(FoundUnits);

    SF_LOG(
        Log,
        "[SFInGameHUDWidget] All owned units selected | Count: %d",
        FoundUnits.Num()
    );
}

bool USFInGameHUDWidget::TryHandleUnitDoubleClick(
    AActor* ClickedActor
)
{
    AUnitBase* ClickedUnit =
        Cast<AUnitBase>(ClickedActor);

    UWorld* World = GetWorld();

    if (
        !IsValid(ClickedUnit) ||
        !IsValid(World) ||
        !IsOwnedByLocalPlayer(ClickedUnit)
        )
    {
        LastClickedUnit = nullptr;
        LastUnitClickTime = -1.0f;
        return false;
    }

    const float CurrentTime =
        World->GetTimeSeconds();

    const bool bIsDoubleClick =
        LastClickedUnit.Get() == ClickedUnit &&
        CurrentTime - LastUnitClickTime <=
        DoubleClickInterval;

    LastClickedUnit = ClickedUnit;
    LastUnitClickTime = CurrentTime;

    if (!bIsDoubleClick)
    {
        return false;
    }

    SelectNearbyUnitsOfSameType(ClickedUnit);

    LastClickedUnit = nullptr;
    LastUnitClickTime = -1.0f;

    return true;
}

void USFInGameHUDWidget::SelectNearbyUnitsOfSameType(
    AUnitBase* CenterUnit
)
{
    if (!IsValid(CenterUnit))
    {
        return;
    }

    UWorld* World = GetWorld();

    if (!IsValid(World))
    {
        return;
    }

    TArray<AActor*> FoundUnits;

    const FVector CenterLocation =
        CenterUnit->GetActorLocation();

    const FName CenterUnitRowName =
        CenterUnit->GetUnitRowName();

    const float SearchRadiusSquared =
        FMath::Square(SameUnitSearchRadius);

    for (
        TActorIterator<AUnitBase> Iterator(World);
        Iterator;
        ++Iterator
        )
    {
        AUnitBase* CandidateUnit = *Iterator;

        if (!IsValid(CandidateUnit))
        {
            continue;
        }

        if (!IsOwnedByLocalPlayer(CandidateUnit))
        {
            continue;
        }

        if (
            CandidateUnit->GetUnitRowName() !=
            CenterUnitRowName
            )
        {
            continue;
        }

        if (
            FVector::DistSquared(
                CenterLocation,
                CandidateUnit->GetActorLocation()
            ) > SearchRadiusSquared
            )
        {
            continue;
        }

        if (
            CandidateUnit->GetCombatState() ==
            EUnitCombatState::Dead
            )
        {
            continue;
        }

        FoundUnits.Add(CandidateUnit);
    }

    if (FoundUnits.IsEmpty())
    {
        FoundUnits.Add(CenterUnit);
    }

    ApplyMultipleSelection(FoundUnits);

    SF_LOG(
        Log,
        "[SFInGameHUDWidget] Same unit double click selection | Type: %s | Count: %d",
        *CenterUnitRowName.ToString(),
        FoundUnits.Num()
    );
}


void USFInGameHUDWidget::HandleTierUpgradeRequested()
{
    if (!IsValid(SelectedActor))
    {
        SF_LOG(
            Warning,
            "[SFInGameHUDWidget] "
            "Tier upgrade failed: "
            "SelectedActor is invalid."
        );
        return;
    }

    const bool bCanTierUpgrade =
        SelectedInfo.SelectableType ==
        ESFSelectableType::MainBuilding ||
        SelectedInfo.SelectableType ==
        ESFSelectableType::ProductionBuilding;

    if (!bCanTierUpgrade)
    {
        SF_LOG(
            Warning,
            "[SFInGameHUDWidget] "
            "Tier upgrade failed: "
            "Invalid selectable type."
        );
        return;
    }

    ASFBaseBuilding* SelectedBuilding =
        Cast<ASFBaseBuilding>(
            SelectedActor.Get()
        );

    if (!IsValid(SelectedBuilding))
    {
        SF_LOG(
            Error,
            "[SFInGameHUDWidget] "
            "Tier upgrade failed: "
            "SelectedActor is not SFBaseBuilding."
        );
        return;
    }

    ASFGameState* GameState =
        GetWorld()
        ? GetWorld()->GetGameState<ASFGameState>()
        : nullptr;

    if (!IsValid(GameState))
    {
        SF_LOG(
            Error,
            "[SFInGameHUDWidget] "
            "Upgrade failed: GameState is invalid."
        );

        return;
    }

    const FResources UpgradeCost =
        SelectedBuilding->GetUpgradeCost();

    const bool bCanAffordUpgrade =
        GameState->CanAffordResource(
            SelectedBuilding->GetTeamIndex(),
            UpgradeCost
        );

    if (!bCanAffordUpgrade)
    {
        SF_LOG(
            Warning,
            "[SFInGameHUDWidget] "
            "Upgrade failed: insufficient resources."
        );

        PlayResourceWarningSound();
        return;
    }

    SF_LOG(
        Log,
        "[SFInGameHUDWidget] "
        "Tier upgrade requested | Building: %s",
        *SelectedBuilding->GetName()
    );

    SelectedBuilding->UpgradeBuilding(1);
}

void USFInGameHUDWidget::BindSelectedHealthDelegate(AActor* Actor)
{
    UnbindSelectedHealthDelegate();

    if (!IsValid(Actor) || !Actor->GetClass()->ImplementsInterface(USFSelectable::StaticClass()))
    {
        return;
    }

    UHealthComponent* HealthComponent = Actor->FindComponentByClass<UHealthComponent>();
    
    if (!IsValid(HealthComponent))
    {
        SF_LOG(Warning, "[SFInGameHUDWidget] Selected actor has no HealthComponent.");
        return;
    }

    SelectedHealthComponent = HealthComponent;

    HealthComponent->OnHealthValueChanged.RemoveDynamic(
        this,
        &USFInGameHUDWidget::HandleSelectedHealthChanged
    );

    HealthComponent->OnHealthValueChanged.AddUniqueDynamic(
        this,
        &USFInGameHUDWidget::HandleSelectedHealthChanged
    );

    SF_LOG(Log, "[SFInGameHUDWidget] Health delegate binding completed.");
}

void USFInGameHUDWidget::UnbindSelectedHealthDelegate()
{
    UHealthComponent* HealthComponent = SelectedHealthComponent.Get();

    if (IsValid(HealthComponent))
    {
        HealthComponent->OnHealthValueChanged.RemoveDynamic(
            this,
            &USFInGameHUDWidget::HandleSelectedHealthChanged
        );
    }

    SelectedHealthComponent.Reset();
}

void USFInGameHUDWidget::HandleSelectedHealthChanged(float BeforeHP, float CurrentHP, float MaxHP)
{
    if (!SelectedHealthComponent.IsValid())
    {
        return;
    }

    SelectedInfo.CurrentHealth = CurrentHP;
    SelectedInfo.MaximumHealth = MaxHP;

    if (IsValid(SelectionPanel))
    {
        SelectionPanel->ShowSelection(SelectedInfo);
    }

    SF_LOG(Log, "[SFInGameHUDWidget] Health changed | %.1f -> %.1f / %.1f", BeforeHP, CurrentHP, MaxHP);
}

void USFInGameHUDWidget::ToggleSettings()
{
    if (IsValid(ActiveSettingsWidget) && ActiveSettingsWidget->IsActivated())
    {
        ActiveSettingsWidget->DeactivateWidget();
        return;
    }

    if (!IsValid(SettingsUIStack) || !SettingsWidgetClass)
    {
        SF_LOG(Error, "[SFInGameHUDWidget] SettingsUIStack or SettingsWidgetClass is invalid.");
        return;
    }

    ActiveSettingsWidget = SettingsUIStack->AddWidget<UCommonActivatableWidget>(SettingsWidgetClass);

    if (!IsValid(ActiveSettingsWidget))
    {
        return;
    }

    ActiveSettingsWidget->OnDeactivated().RemoveAll(this);
    ActiveSettingsWidget->OnDeactivated().AddUObject(this,&USFInGameHUDWidget::HandleSettingsDeactivated);

    UGameplayStatics::SetGamePaused(this,true);

    APlayerController* PlayerController = GetOwningPlayer();

    if (IsValid(PlayerController))
    {
        FInputModeGameAndUI InputMode;
        InputMode.SetWidgetToFocus(ActiveSettingsWidget->TakeWidget());
        InputMode.SetHideCursorDuringCapture(false);
        InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::LockAlways);

        PlayerController->SetInputMode(InputMode);
        PlayerController->bShowMouseCursor = true;
    }

    SF_LOG(Log, "[SFInGameHUDWidget] Settings opened.");
}

void USFInGameHUDWidget::HandleSettingsDeactivated()
{
    if (IsValid(ActiveSettingsWidget))
    {
        ActiveSettingsWidget->OnDeactivated().RemoveAll(this);
    }

    ActiveSettingsWidget = nullptr;

    UGameplayStatics::SetGamePaused(this, false);

    APlayerController* PlayerController = GetOwningPlayer();
    if (IsValid(PlayerController))
    {
        FInputModeGameAndUI InputMode;
        InputMode.SetHideCursorDuringCapture(false);
        InputMode.SetLockMouseToViewportBehavior(
            EMouseLockMode::LockAlways
        );

        PlayerController->SetInputMode(InputMode);
        PlayerController->bShowMouseCursor = true;
    }

    SF_LOG(
        Log,
        "[SFInGameHUDWidget] "
        "Settings closed. Game resumed."
    );
}

void USFInGameHUDWidget::ShowSelectionIndicator(AActor* Actor)
{
    if (!IsValid(Actor))
    {
        return;
    }

    if (!IsValid(SelectionIndicatorMaterial))
    {
        SF_LOG(Warning, "[SFInGameHUDWidget] SelectionIndicatorMaterial is invalid.");
        return;
    }

    if (SelectionIndicatorComponents.Contains(Actor))
    {
        return;
    }

    USceneComponent* RootComponent = Actor->GetRootComponent();
    if (!IsValid(RootComponent))
    {
        return;
    }

    const bool bIsBuilding = Actor->IsA<ASFBaseBuilding>();


    const bool bIsProductionBuilding =
        Actor->IsA<ASFProductionBuilding>();

    const bool bOnlyCollidingComponents =
        !bIsBuilding ||
        bIsProductionBuilding;

    FVector Origin;
    FVector Extent;

    Actor->GetActorBounds(
        bOnlyCollidingComponents,
        Origin,
        Extent
    );

    const float UnitBaseRadius = FMath::Max(Extent.X, Extent.Y);
    const float BuildingBaseRadius = FVector2D(Extent.X, Extent.Y).Size();

    const float RingRadius = bIsBuilding
        ? FMath::Clamp(BuildingBaseRadius * 1.05f, 100.f, 1500.f)
        : FMath::Clamp(UnitBaseRadius * 0.7f, 32.f, 150.f);

    const FVector IndicatorLocation(
        Origin.X,
        Origin.Y,
        Origin.Z - Extent.Z + 8.f
    );

    UDecalComponent* Indicator = NewObject<UDecalComponent>(Actor);
    if (!IsValid(Indicator))
    {
        return;
    }

    Actor->AddInstanceComponent(Indicator);
    Indicator->SetupAttachment(RootComponent);
    Indicator->SetAbsolute(false, true, true);
    Indicator->SetMobility(EComponentMobility::Movable);
    Indicator->SetDecalMaterial(SelectionIndicatorMaterial);
    Indicator->DecalSize = FVector(32.f, RingRadius, RingRadius);
    Indicator->SetSortOrder(10);
    Indicator->RegisterComponent();
    Indicator->SetWorldLocation(IndicatorLocation);
    Indicator->SetWorldRotation(FRotator(-90.f, 0.f, 0.f));

    SelectionIndicatorComponents.Add(Actor, Indicator);

    SF_LOG(
        Log,
        "[SFInGameHUDWidget] Selection indicator created | Actor: %s | Radius: %.1f",
        *Actor->GetName(),
        RingRadius
    );
}

void USFInGameHUDWidget::HideSelectionIndicator(AActor* Actor)
{
    if (Actor == nullptr)
    {
        return;
    }

    TObjectPtr<UDecalComponent>* IndicatorPtr =
        SelectionIndicatorComponents.Find(Actor);

    if (IndicatorPtr == nullptr)
    {
        return;
    }

    UDecalComponent* Indicator = IndicatorPtr->Get();

    if (IsValid(Indicator))
    {
        Indicator->DestroyComponent();
    }

    SelectionIndicatorComponents.Remove(Actor);
}

void USFInGameHUDWidget::HideAllSelectionIndicators()
{
    for (TPair<TObjectPtr<AActor>, TObjectPtr<UDecalComponent>>& Pair : SelectionIndicatorComponents)
    {
        if (IsValid(Pair.Value))
        {
            Pair.Value->DestroyComponent();
        }
    }

    SelectionIndicatorComponents.Empty();
}

void USFInGameHUDWidget::HandleSelectionDragStarted(
    FVector2D StartScreenPosition
)
{
    APlayerController* PlayerController =
        GetOwningPlayer();

    bAdditiveSelectionDrag =
        IsValid(PlayerController) &&
        (
            PlayerController->IsInputKeyDown(EKeys::LeftShift) ||
            PlayerController->IsInputKeyDown(EKeys::RightShift)
            );

    SelectionDragStartPosition = StartScreenPosition;
    bIsSelectionDragging = true;

    if (IsValid(SelectionDragBox))
    {
        SelectionDragBox->SetVisibility(
            ESlateVisibility::HitTestInvisible
        );
        UpdateSelectionDragBox(StartScreenPosition);
    }
}

void USFInGameHUDWidget::HandleSelectionDragCompleted(
    FVector2D StartScreenPosition,
    FVector2D EndScreenPosition
)
{
    bIsSelectionDragging = false;

    if (IsValid(SelectionDragBox))
    {
        UpdateSelectionDragBox(EndScreenPosition);
        SelectionDragBox->SetVisibility(
            ESlateVisibility::Collapsed
        );
    }

    const float DragDistance = FVector2D::Distance(
        StartScreenPosition,
        EndScreenPosition
    );

    const bool bAddToExistingSelection =
        bAdditiveSelectionDrag;

    bAdditiveSelectionDrag = false;

    if (DragDistance <= 8.f)
    {
        return;
    }

    CloseBuildingMode();

    PerformDragSelection(
        StartScreenPosition,
        EndScreenPosition,
        bAddToExistingSelection
    );
}

void USFInGameHUDWidget::NativeTick(
    const FGeometry& MyGeometry,
    float InDeltaTime
)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    RefreshSelectedResourceInfo();

    if (!bIsSelectionDragging || !IsValid(SelectionDragBox))
    {
        return;
    }

    APlayerController* PlayerController = GetOwningPlayer();
    if (!IsValid(PlayerController))
    {
        return;
    }

    FVector2D CurrentScreenPosition;
    if (PlayerController->GetMousePosition(
        CurrentScreenPosition.X,
        CurrentScreenPosition.Y
    ))
    {
        UpdateSelectionDragBox(CurrentScreenPosition);
    }
}

void USFInGameHUDWidget::UpdateSelectionDragBox(
    FVector2D CurrentScreenPosition
)
{
    if (!IsValid(SelectionDragBox))
    {
        return;
    }

    UCanvasPanelSlot* CanvasSlot =
        Cast<UCanvasPanelSlot>(SelectionDragBox->Slot);

    if (!IsValid(CanvasSlot))
    {
        return;
    }

    const float ViewportScale =
        UWidgetLayoutLibrary::GetViewportScale(this);

    const float SafeViewportScale =
        FMath::Max(ViewportScale, KINDA_SMALL_NUMBER);

    const FVector2D LocalStart =
        SelectionDragStartPosition / SafeViewportScale;

    const FVector2D LocalCurrent =
        CurrentScreenPosition / SafeViewportScale;

    const FVector2D TopLeft(
        FMath::Min(LocalStart.X, LocalCurrent.X),
        FMath::Min(LocalStart.Y, LocalCurrent.Y)
    );

    const FVector2D BoxSize(
        FMath::Abs(LocalCurrent.X - LocalStart.X),
        FMath::Abs(LocalCurrent.Y - LocalStart.Y)
    );

    CanvasSlot->SetPosition(TopLeft);
    CanvasSlot->SetSize(BoxSize);
}

void USFInGameHUDWidget::PerformDragSelection(
    FVector2D StartScreenPosition,
    FVector2D EndScreenPosition,
    bool bAddToExistingSelection
)
{
    APlayerController* PlayerController =
        GetOwningPlayer();

    UWorld* World = GetWorld();

    if (!IsValid(PlayerController) || !IsValid(World))
    {
        return;
    }

    const FVector2D RectangleMin(
        FMath::Min(
            StartScreenPosition.X,
            EndScreenPosition.X
        ),
        FMath::Min(
            StartScreenPosition.Y,
            EndScreenPosition.Y
        )
    );

    const FVector2D RectangleMax(
        FMath::Max(
            StartScreenPosition.X,
            EndScreenPosition.X
        ),
        FMath::Max(
            StartScreenPosition.Y,
            EndScreenPosition.Y
        )
    );

    const FBox2D SelectionRectangle(
        RectangleMin,
        RectangleMax
    );

    TArray<AActor*> FoundActors;
    TArray<AActor*> EnemyActors;

    for (TActorIterator<AActor> Iterator(World); Iterator; ++Iterator)
    {
        AActor* Actor = *Iterator;

        if (!IsValid(Actor))
        {
            continue;
        }

        if (!Actor->GetClass()->ImplementsInterface(
            USFSelectable::StaticClass()
        ))
        {
            continue;
        }

        const bool bIsOwnedByLocalPlayer =
            IsOwnedByLocalPlayer(Actor);

        FVector BoundsOrigin;
        FVector BoundsExtent;
        Actor->GetActorBounds(true, BoundsOrigin, BoundsExtent);

        const FSFSelectableInfo SelectableInfo =
            ISFSelectable::Execute_GetSelectableInfo(Actor);

        FVector2D ActorScreenPosition;

        const bool bProjected =
            PlayerController->ProjectWorldLocationToScreen(
                BoundsOrigin,
                ActorScreenPosition,
                true
            );

        if (!bProjected)
        {
            continue;
        }

        const bool bUsesBoundsOverlap =
            SelectableInfo.SelectableType == ESFSelectableType::Commander ||
            SelectableInfo.SelectableType == ESFSelectableType::Unit;

        if (!bUsesBoundsOverlap)
        {
            if (!SelectionRectangle.IsInsideOrOn(ActorScreenPosition))
            {
                continue;
            }
        }
        else
        {
            FVector2D ScreenBoundsMin = ActorScreenPosition;
            FVector2D ScreenBoundsMax = ActorScreenPosition;
            bool bHasProjectedCorner = false;

            for (int32 CornerIndex = 0; CornerIndex < 8; ++CornerIndex)
            {
                const FVector CornerLocation = BoundsOrigin + FVector(
                    (CornerIndex & 1) != 0 ? BoundsExtent.X : -BoundsExtent.X,
                    (CornerIndex & 2) != 0 ? BoundsExtent.Y : -BoundsExtent.Y,
                    (CornerIndex & 4) != 0 ? BoundsExtent.Z : -BoundsExtent.Z
                );

                FVector2D CornerScreenPosition;

                if (!PlayerController->ProjectWorldLocationToScreen(
                    CornerLocation,
                    CornerScreenPosition,
                    true
                ))
                {
                    continue;
                }

                ScreenBoundsMin.X = FMath::Min(
                    ScreenBoundsMin.X,
                    CornerScreenPosition.X
                );
                ScreenBoundsMin.Y = FMath::Min(
                    ScreenBoundsMin.Y,
                    CornerScreenPosition.Y
                );
                ScreenBoundsMax.X = FMath::Max(
                    ScreenBoundsMax.X,
                    CornerScreenPosition.X
                );
                ScreenBoundsMax.Y = FMath::Max(
                    ScreenBoundsMax.Y,
                    CornerScreenPosition.Y
                );

                bHasProjectedCorner = true;
            }

            if (!bHasProjectedCorner)
            {
                continue;
            }

            const bool bOverlapsSelection =
                ScreenBoundsMin.X <= RectangleMax.X &&
                ScreenBoundsMax.X >= RectangleMin.X &&
                ScreenBoundsMin.Y <= RectangleMax.Y &&
                ScreenBoundsMax.Y >= RectangleMin.Y;

            if (!bOverlapsSelection)
            {
                continue;
            }
        }

        if (bIsOwnedByLocalPlayer)
        {
            FoundActors.Add(Actor);
        }
        else
        {
            EnemyActors.Add(Actor);
        }
    }

    FoundActors.Sort(
        [this](const AActor& Left, const AActor& Right)
        {
            const int32 LeftPriority =
                GetSelectionPriority(
                    const_cast<AActor*>(&Left)
                );

            const int32 RightPriority =
                GetSelectionPriority(
                    const_cast<AActor*>(&Right)
                );

            if (LeftPriority != RightPriority)
            {
                return LeftPriority < RightPriority;
            }

            return Left.GetName() < Right.GetName();
        }
    );

    if (FoundActors.Num() > 30)
    {
        FoundActors.SetNum(30);
    }

    if (bAddToExistingSelection)
    {
        TArray<AActor*> CombinedActors;

        for (const TObjectPtr<AActor>& ExistingActor :
            SelectedActors)
        {
            AActor* Actor = ExistingActor.Get();

            if (IsValid(Actor) &&
                IsOwnedByLocalPlayer(Actor))
            {
                CombinedActors.AddUnique(Actor);
            }
        }

        for (AActor* Actor : FoundActors)
        {
            if (IsValid(Actor))
            {
                CombinedActors.AddUnique(Actor);
            }
        }

        if (CombinedActors.Num() > 30)
        {
            CombinedActors.SetNum(30);
        }

        FoundActors = MoveTemp(CombinedActors);
    }

    if (!FoundActors.IsEmpty())
    {
        ApplyMultipleSelection(FoundActors);
        return;
    }

    if (bAddToExistingSelection)
    {
        return;
    }

    if (!EnemyActors.IsEmpty())
    {
        EnemyActors.Sort(
            [](const AActor& Left, const AActor& Right)
            {
                return Left.GetName() < Right.GetName();
            }
        );

        SelectActor(EnemyActors[0]);

        SF_LOG(
            Log,
            "[SFInGameHUDWidget] Enemy drag selection reduced to one actor: %s",
            *EnemyActors[0]->GetName()
        );

        return;
    }

    ClearSelection();
}

int32 USFInGameHUDWidget::GetSelectionPriority(
    AActor* Actor
) const
{
    if (!IsValid(Actor))
    {
        return 100;
    }

    if (!Actor->GetClass()->ImplementsInterface(
        USFSelectable::StaticClass()
    ))
    {
        return 100;
    }

    const FSFSelectableInfo SelectableInfo =
        ISFSelectable::Execute_GetSelectableInfo(Actor);

    switch (SelectableInfo.SelectableType)
    {
    case ESFSelectableType::Commander:
        return 0;

    case ESFSelectableType::MainBuilding:
    case ESFSelectableType::NormalBuilding:
    case ESFSelectableType::ProductionBuilding:
    case ESFSelectableType::ResourceBuilding:
        return 1;

    case ESFSelectableType::Unit:
        // 우선순위 2는 스킬 보유 유닛에 사용할 예정
        return 3;

    default:
        return 100;
    }
}

void USFInGameHUDWidget::ApplyMultipleSelection(
    const TArray<AActor*>& NewSelectedActors
)
{
    if (NewSelectedActors.IsEmpty())
    {
        ClearSelection();
        return;
    }

    CloseBuildingMode();

    AActor* PrimaryActor = NewSelectedActors[0];

    if (!IsValid(PrimaryActor))
    {
        ClearSelection();
        return;
    }

    SelectActor(PrimaryActor);

    SelectedActors.Reset();
    HideAllSelectionIndicators();

    for (AActor* Actor : NewSelectedActors)
    {
        if (!IsValid(Actor))
        {
            continue;
        }

        SelectedActors.Add(Actor);
        ShowSelectionIndicator(Actor);
    }

    RefreshRallyPointIndicator();
    BindSelectedActorsDestroyed();
    RefreshMultiSelectionPanel();

    SF_LOG(
        Log,
        "[SFInGameHUDWidget] Multiple selection applied | Count: %d | Primary: %s",
        SelectedActors.Num(),
        *PrimaryActor->GetName()
    );
}

void USFInGameHUDWidget::RefreshMultiSelectionPanel()
{
    if (!IsValid(MultiSelectionPanel))
    {
        return;
    }

    const bool bIsBuildingModeActive =
        IsValid(ActiveBuildingModeWidget) &&
        ActiveBuildingModeWidget->IsActivated();

    if (bIsBuildingModeActive)
    {
        MultiSelectionPanel->ClearSelections();
        return;
    }

    if (SelectedActors.Num() <= 1)
    {
        MultiSelectionPanel->ClearSelections();
        return;
    }

    TArray<TObjectPtr<AActor>> ValidActors;
    TArray<FSFSelectableInfo> SelectableInfos;

    ValidActors.Reserve(SelectedActors.Num());
    SelectableInfos.Reserve(SelectedActors.Num());

    for (const TObjectPtr<AActor>& ActorPtr : SelectedActors)
    {
        AActor* Actor = ActorPtr.Get();

        if (!IsValid(Actor))
        {
            continue;
        }

        if (!Actor->GetClass()->ImplementsInterface(USFSelectable::StaticClass()))
        {
            continue;
        }

        FSFSelectableInfo SelectableInfo =
            ISFSelectable::Execute_GetSelectableInfo(Actor);

        ApplySelectionUIData(Actor, SelectableInfo);

        ValidActors.Add(Actor);
        SelectableInfos.Add(SelectableInfo);
    }

    SelectedActors = MoveTemp(ValidActors);

    if (SelectedActors.Num() <= 1)
    {
        MultiSelectionPanel->ClearSelections();
        return;
    }

    if (IsValid(ProductionBuildingPanel))
    {
        ProductionBuildingPanel->HideProductionBuilding();
    }

    MultiSelectionPanel->ShowSelections(
        SelectedActors,
        SelectableInfos
    );
}

void USFInGameHUDWidget::HandlePrimarySelectionRequested(AActor* Actor)
{
    if (!IsValid(Actor))
    {
        return;
    }

    const int32 RequestedIndex = SelectedActors.IndexOfByPredicate(
        [Actor](const TObjectPtr<AActor>& ActorPtr)
        {
            return ActorPtr.Get() == Actor;
        }
    );

    if (RequestedIndex == INDEX_NONE)
    {
        SF_LOG(Warning, "[SFInGameHUDWidget] Requested actor is not in SelectedActors.");
        return;
    }

    if (RequestedIndex > 0)
    {
        TObjectPtr<AActor> PromotedActor = SelectedActors[RequestedIndex];
        SelectedActors.RemoveAt(RequestedIndex);
        SelectedActors.Insert(PromotedActor, 0);
    }

    UnbindSelectedHealthDelegate();

    SelectedActor = Actor;

    FSFSelectableInfo NewSelectedInfo =
        ISFSelectable::Execute_GetSelectableInfo(Actor);

    // const bool bIsBuilding =
    //     NewSelectedInfo.SelectableType == ESFSelectableType::MainBuilding ||
    //     NewSelectedInfo.SelectableType == ESFSelectableType::NormalBuilding ||
    //     NewSelectedInfo.SelectableType == ESFSelectableType::ProductionBuilding ||
    //     NewSelectedInfo.SelectableType == ESFSelectableType::ResourceBuilding;
    //
    // if (bIsBuilding)
    // {
    //     const FHealthData HealthData =
    //         ISFSelectable::Execute_GetHealthData(Actor);
    //
    //     NewSelectedInfo.Tier = HealthData.CurrentTier + 1;
    // }

    ApplySelectionUIData(
        Actor,
        NewSelectedInfo
    );
    SelectedInfo = NewSelectedInfo;

    SelectedActor->OnDestroyed.AddUniqueDynamic(
        this,
        &USFInGameHUDWidget::HandleSelectedActorDestroyed
    );

    if (ASFInGamePlayerController* PlayerController =
        Cast<ASFInGamePlayerController>(GetOwningPlayer()))
    {
        PlayerController->SetSelectedActor(SelectedActor.Get());
    }

    BindSelectedHealthDelegate(SelectedActor.Get());
    RefreshBuildModeOpenButtonVisibility();

    if (IsValid(SelectionPanel))
    {
        SelectionPanel->ShowSelection(SelectedInfo);
    }

    if (IsValid(ProductionBuildingPanel))
    {
        ProductionBuildingPanel->HideProductionBuilding();
    }

    RefreshSelectedPortrait();
    RefreshRallyPointIndicator();
    RefreshMultiSelectionPanel();

    SF_LOG(
        Log,
        "[SFInGameHUDWidget] Primary selection changed | Actor: %s",
        *Actor->GetName()
    );
}

bool USFInGameHUDWidget::IsOwnedByLocalPlayer(AActor* Actor) const
{
    if (!IsValid(Actor))
    {
        return false;
    }


    const ASFInGamePlayerController* PlayerController =
        Cast<ASFInGamePlayerController>(GetOwningPlayer());

    if (IsValid(PlayerController) &&
        Actor == PlayerController->GetMyCommander())
    {
        return true;
    }

    const UCombatTargetComponent* CombatTarget =
        Actor->FindComponentByClass<UCombatTargetComponent>();

    if (!IsValid(CombatTarget))
    {
        return false;
    }

    return CombatTarget->GetTeamIndex() == Targetable::PlayerTeam;
}

void USFInGameHUDWidget::ShowMoveDestinationIndicator(
    const FVector& WorldLocation,
    bool bIsAttack
)
{
    UMaterialInterface* IndicatorMaterial =
        bIsAttack && IsValid(AttackDestinationIndicatorMaterial)
        ? AttackDestinationIndicatorMaterial
        : MoveDestinationIndicatorMaterial;

    if (!IsValid(IndicatorMaterial))
    {
        SF_LOG(
            Warning,
            "[SFInGameHUDWidget] Destination indicator material is invalid."
        );
        return;
    }

    UWorld* World = GetWorld();
    if (!IsValid(World))
    {
        return;
    }

    UMaterialInstanceDynamic* DynamicMaterial =
        UMaterialInstanceDynamic::Create(
            IndicatorMaterial,
            this
        );

    if (!IsValid(DynamicMaterial))
    {
        return;
    }

    DynamicMaterial->SetScalarParameterValue(
        TEXT("StartTime"),
        World->GetTimeSeconds()
    );

    DynamicMaterial->SetScalarParameterValue(
        TEXT("AnimDuration"),
        MoveDestinationIndicatorLifeTime
    );

    UDecalComponent* Indicator =
        UGameplayStatics::SpawnDecalAtLocation(
            this,
            DynamicMaterial,
            MoveDestinationIndicatorSize,
            WorldLocation + FVector(0.f, 0.f, 8.f),
            FRotator(-90.f, 0.f, 0.f),
            MoveDestinationIndicatorLifeTime
        );

    if (!IsValid(Indicator))
    {
        return;
    }

    Indicator->SetSortOrder(20);

    const float FadeStartDelay = FMath::Max(
        0.f,
        MoveDestinationIndicatorLifeTime -
        MoveDestinationIndicatorFadeDuration
    );

    Indicator->SetFadeOut(
        FadeStartDelay,
        MoveDestinationIndicatorFadeDuration,
        false
    );

    SF_LOG(
        Log,
        "[SFInGameHUDWidget] "
        "Move destination indicator shown | Location: %s",
        *WorldLocation.ToString()
    );
}

FName USFInGameHUDWidget::ResolveStageIntroRowName() const
{
    if (UWorld* World = GetWorld())
    {
        if (ASFGameState* GameState = World->GetGameState<ASFGameState>())
        {
            const FName StageRowName = GameState->GetCurrentStageRowName();

            if (!StageRowName.IsNone())
            {
                return StageRowName;
            }
        }
    }

    if (const USFGameInstance* GameInstance = Cast<USFGameInstance>(GetGameInstance()))
    {
        const FName StageRowName = GameInstance->GetSelectedStageRowName();

        if (!StageRowName.IsNone())
        {
            return StageRowName;
        }
    }

    return StageIntroFallbackRowName;
}

void USFInGameHUDWidget::HandleStageStateChanged(ESFStageState NewState)
{
    /*
     * 초기화가 실패하면 LoadingWidget 이 로비로 보낸다.
     * 다만 그 사이 HUD 가 숨겨진 채로 남으면 화면에 아무것도 없어서
     * "게임이 멈췄다" 로 보이므로 가시성만 되돌려 둔다.
     */
    if (NewState == ESFStageState::InitializationFailed)
    {
        if (bHideHUDUntilStagePlaying)
        {
            SetVisibility(VisibilityBeforeStageLoading);
        }

        SF_LOG(
            Warning,
            "[SFInGameHUDWidget] 스테이지 초기화가 실패해 인트로를 열지 않습니다."
        );

        return;
    }

    if (NewState != ESFStageState::Playing)
    {
        return;
    }

    if (bStageIntroRequested)
    {
        return;
    }

    bStageIntroRequested = true;

    /*
     * 인트로를 열기 전에 가시성을 먼저 되돌려야 한다.
     *
     * OpenStageIntro 가 VisibilityBeforeStageIntro 에 현재 값을 저장했다가
     * 인트로가 끝나면 그대로 복구한다. 순서를 뒤집으면 Collapsed 가 저장돼
     * 인트로가 끝난 뒤 HUD 가 사라진 채로 남는다.
     */
    if (bHideHUDUntilStagePlaying)
    {
        SetVisibility(VisibilityBeforeStageLoading);
    }

    SF_LOG(Log, "[SFInGameHUDWidget] 스테이지가 시작되어 인트로를 엽니다.");

    TryStartStageIntro();
}

void USFInGameHUDWidget::TryStartStageIntro()
{
    SF_LOG(Log, "[SFInGameHUDWidget] TryStartStageIntro called.");

    // 어느 행을 열지만 정하고, 실제로 여는 절차는 OpenStageIntro 가 전부 맡는다.
    // 목표 대기에서 돌아올 때도 같은 함수를 타야 HUD 가시성·일시정지 같은 상태가
    // 어긋나지 않는다.
    //
    // 예전에는 여기서 폴백 행을 그대로 열었다. 그래서 2스테이지로 넘어가도
    // 1스테이지 튜토리얼이 다시 떴다. 지금은 스테이지를 물어보고 연다.
    const FName RowName = ResolveStageIntroRowName();

    // 튜토리얼이 없는 스테이지가 정상적으로 존재한다.
    // 그대로 OpenStageIntro 로 넘기면 매 판 Error 로그가 남으므로 여기서 거른다.
    // 반대로 재개(OnResumeRequested) 경로에서 행이 없는 것은 진짜 오류이므로
    // OpenStageIntro 쪽 Error 는 그대로 둔다.
    if (!IsValid(StageIntroDataTable))
    {
        SF_LOG(Log, "[SFInGameHUDWidget] StageIntroDataTable is not set. Tutorial is skipped.");
        return;
    }

    if (RowName.IsNone() || !StageIntroDataTable->GetRowNames().Contains(RowName))
    {
        SF_LOG(
            Log,
            "[SFInGameHUDWidget] This stage has no tutorial, so it is skipped | Row: %s",
            *RowName.ToString()
        );
        return;
    }

    // INDEX_NONE 은 "행에 지정된 StartNodeIndex 부터" 라는 뜻이다.
    OpenStageIntro(
        StageIntroDataTable,
        RowName,
        INDEX_NONE
    );
}

bool USFInGameHUDWidget::OpenStageIntro(
    UDataTable* InDataTable,
    FName InRowName,
    int32 InNodeIndex
)
{
    // 이미 떠 있으면 열지 않는다.
    //
    // 두 번 열면 위젯이 겹칠 뿐 아니라, 두 번째 것이 닫힐 때
    // HandleStageIntroDeactivated 가 첫 번째가 저장해둔 가시성·일시정지 상태를
    // 되돌려버려서 HUD 가 숨겨진 채로 남는다.
    if (IsValid(ActiveStageIntroWidget))
    {
        SF_LOG(
            Warning,
            "[SFInGameHUDWidget] Stage intro is already open. The request was ignored."
        );
        return false;
    }

    if (!StageIntroWidgetClass)
    {
        SF_LOG(Error, "[SFInGameHUDWidget] StageIntroWidgetClass is invalid.");
        return false;
    }

    if (!IsValid(InDataTable))
    {
        SF_LOG(Error, "[SFInGameHUDWidget] StageIntroDataTable is invalid.");
        return false;
    }

    if (InRowName.IsNone())
    {
        SF_LOG(Error, "[SFInGameHUDWidget] StageIntro row name is None.");
        return false;
    }

    if (!InDataTable->GetRowNames().Contains(InRowName))
    {
        SF_LOG(
            Error,
            "[SFInGameHUDWidget] StageIntro row was not found: %s",
            *InRowName.ToString()
        );
        return false;
    }

    APlayerController* PlayerController = GetOwningPlayer();

    if (!IsValid(PlayerController))
    {
        SF_LOG(Error, "[SFInGameHUDWidget] OwningPlayer is invalid.");
        return false;
    }

    ActiveStageIntroWidget = CreateWidget<USFStageIntroWidget>(
        PlayerController,
        StageIntroWidgetClass
    );

    if (!IsValid(ActiveStageIntroWidget))
    {
        SF_LOG(Error, "[SFInGameHUDWidget] Failed to create StageIntroWidget.");
        return false;
    }

    ActiveStageIntroWidget->OnDeactivated().AddUObject(
        this,
        &USFInGameHUDWidget::HandleStageIntroDeactivated
    );

    ActiveStageIntroWidget->AddToViewport(500);
    ActiveStageIntroWidget->ActivateWidget();

    // 가시성과 일시정지 상태는 재생을 시작하기 전에 저장해야 한다.
    //
    // 첫 노드가 곧바로 대기 노드이면 StartStageIntroAt 안에서 그대로
    // DeactivateWidget 이 불리고, HandleStageIntroDeactivated 가 이 값들을
    // 읽는다. 아래에서 저장하면 그때 이전 인트로의 값이 남아 있다.
    VisibilityBeforeStageIntro = GetVisibility();
    bWasGamePausedBeforeStageIntro =
        UGameplayStatics::IsGamePaused(this);

    SetVisibility(ESlateVisibility::Collapsed);

    if (!bWasGamePausedBeforeStageIntro)
    {
        UGameplayStatics::SetGamePaused(this, true);
    }

    FInputModeGameAndUI InputMode;
    InputMode.SetWidgetToFocus(
        ActiveStageIntroWidget->TakeWidget()
    );
    InputMode.SetHideCursorDuringCapture(false);
    InputMode.SetLockMouseToViewportBehavior(
        EMouseLockMode::LockAlways
    );

    PlayerController->SetInputMode(InputMode);
    PlayerController->bShowMouseCursor = true;

    if (!ActiveStageIntroWidget->StartStageIntroAt(
        InDataTable,
        InRowName,
        InNodeIndex
    ))
    {
        SF_LOG(Error, "[SFInGameHUDWidget] StartStageIntroAt failed.");

        // 여기까지 왔으면 이미 HUD 를 숨기고 게임을 멈춘 뒤다.
        // 위젯만 걷어내면 조작 불가 상태로 남으므로 정식 경로로 되돌린다.
        HandleStageIntroDeactivated();
        return false;
    }

    SF_LOG(
        Log,
        "[SFInGameHUDWidget] StageIntro started | Row: %s | Node: %d",
        *InRowName.ToString(),
        InNodeIndex
    );

    return true;
}

void USFInGameHUDWidget::HandleTutorialResumeRequested(
    UDataTable* InDataTable,
    FName InRowName,
    int32 InNodeIndex
)
{
    // 대기에 들어갈 때 넘겼던 값이 그대로 돌아온다.
    // 다만 서브시스템이 재시작됐거나 하는 경우를 대비해 폴백을 둔다.
    UDataTable* ResumeDataTable =
        IsValid(InDataTable) ? InDataTable : StageIntroDataTable.Get();

    const FName ResumeRowName =
        InRowName.IsNone() ? StageIntroFallbackRowName : InRowName;

    SF_LOG(
        Log,
        "[SFInGameHUDWidget] Tutorial resume requested | Row: %s | Node: %d",
        *ResumeRowName.ToString(),
        InNodeIndex
    );

    OpenStageIntro(ResumeDataTable, ResumeRowName, InNodeIndex);
}

void USFInGameHUDWidget::HandleStageIntroDeactivated()
{
    if (IsValid(ActiveStageIntroWidget))
    {
        ActiveStageIntroWidget->OnDeactivated().RemoveAll(this);
        ActiveStageIntroWidget->RemoveFromParent();
    }

    ActiveStageIntroWidget = nullptr;

    SetVisibility(VisibilityBeforeStageIntro);

    if (!bWasGamePausedBeforeStageIntro)
    {
        UGameplayStatics::SetGamePaused(this, false);
    }

    bWasGamePausedBeforeStageIntro = false;

    APlayerController* PlayerController = GetOwningPlayer();

    if (IsValid(PlayerController))
    {
        FInputModeGameAndUI InputMode;
        InputMode.SetHideCursorDuringCapture(false);
        InputMode.SetLockMouseToViewportBehavior(
            EMouseLockMode::LockAlways
        );

        PlayerController->SetInputMode(InputMode);
        PlayerController->bShowMouseCursor = true;
    }

    SF_LOG(
        Log,
        "[SFInGameHUDWidget] Stage intro finished."
    );
}

bool USFInGameHUDWidget::RequestProductionBySlot(
    int32 SlotIndex
)
{
    if (!IsValid(ProductionBuildingPanel))
    {
        return false;
    }

    TArray<ASFProductionBuilding*> SelectedProductionBuildings;

    for (const TObjectPtr<AActor>& ActorPtr : SelectedActors)
    {
        ASFProductionBuilding* ProductionBuilding =
            Cast<ASFProductionBuilding>(ActorPtr.Get());

        if (IsValid(ProductionBuilding))
        {
            SelectedProductionBuildings.AddUnique(
                ProductionBuilding
            );
        }
    }

    if (SelectedProductionBuildings.IsEmpty())
    {
        ASFProductionBuilding* FallbackProductionBuilding =
            Cast<ASFProductionBuilding>(
                SelectedProductionBuilding.Get()
            );

        if (!IsValid(FallbackProductionBuilding))
        {
            FallbackProductionBuilding =
                Cast<ASFProductionBuilding>(
                    ProductionBuildingPanel->GetProductionBuilding()
                );
        }

        if (IsValid(FallbackProductionBuilding))
        {
            SelectedProductionBuildings.Add(
                FallbackProductionBuilding
            );
        }
    }

    if (SelectedProductionBuildings.IsEmpty())
    {
        return false;
    }

    // Q/W/E 슬롯은 주 선택 생산 건물 기준
    ASFProductionBuilding* PrimaryProductionBuilding =
        Cast<ASFProductionBuilding>(
            SelectedActor.Get()
        );

    if (!IsValid(PrimaryProductionBuilding))
    {
        PrimaryProductionBuilding =
            SelectedProductionBuildings[0];
    }

    if (!IsValid(PrimaryProductionBuilding))
    {
        return false;
    }

    const FProductableData ProductableData =
        IProductable::Execute_GetProductableData(
            PrimaryProductionBuilding
        );

    if (!ProductableData.ProductableUnitFNames.IsValidIndex(SlotIndex))
    {
        return false;
    }

    const FName UnitRowName =
        ProductableData.ProductableUnitFNames[SlotIndex];

    if (UnitRowName.IsNone())
    {
        return false;
    }

    bool bRequestedProduction = false;

    // SelectedActors에 들어있는 순서대로 생산 요청
    for (
        ASFProductionBuilding* ProductionBuilding :
        SelectedProductionBuildings
        )
    {
        if (!IsValid(ProductionBuilding))
        {
            continue;
        }

        bRequestedProduction =
            ProductionBuildingPanel->
            RequestProductionOnBuilding(
                ProductionBuilding,
                UnitRowName
            ) || bRequestedProduction;
    }

    return bRequestedProduction;
}

void USFInGameHUDWidget::PlayBuildStartSound()
{
    if (!IsValid(BuildStartSound))
    {
        return;
    }

    UGameplayStatics::PlaySound2D(
        this,
        BuildStartSound
    );
}

void USFInGameHUDWidget::PlayResourceWarningSound()
{
    if (!IsValid(ResourceWarningSound))
    {
        return;
    }

    UGameplayStatics::PlaySound2D(
        this,
        ResourceWarningSound
    );
}

void USFInGameHUDWidget::
HandleProductionResourceWarningRequested()
{
    PlayResourceWarningSound();
}

void USFInGameHUDWidget::HandleRallyPointRequested()
{
    ASFInGamePlayerController* PlayerController =
        Cast<ASFInGamePlayerController>(GetOwningPlayer());

    if (IsValid(PlayerController))
    {
        PlayerController->BeginRallyPointCommand();
    }
}

void USFInGameHUDWidget::BindSelectedProductionBuilding(
    AActor* Actor
)
{
    UnbindSelectedProductionBuilding();

    if (!IsValid(Actor))
    {
        return;
    }

    // 모든 건물 공통 티어 델리게이트 연결
    ASFBaseBuilding* BaseBuilding =
        Cast<ASFBaseBuilding>(Actor);

    if (IsValid(BaseBuilding))
    {
        SelectedBuildingForTierDelegate =
            BaseBuilding;

        SelectedBuildingForTierDelegate->OnBuildingTierActivated.AddUniqueDynamic(this, &USFInGameHUDWidget::HandleSelectedBuildingTierActivated);
    }

    // 생산 건물 전용 델리게이트 연결
    ASFProductionBuilding* ProductionBuilding = Cast<ASFProductionBuilding>(Actor);

    if (IsValid(ProductionBuilding))
    {
        SelectedProductionBuilding = ProductionBuilding;

        SelectedProductionBuilding->OnSpawnableUnitsChanged.AddUniqueDynamic(this, &USFInGameHUDWidget::HandleSelectedProductionBuildingTierActivated);
    }
}

void USFInGameHUDWidget::UnbindSelectedProductionBuilding()
{
    if (SelectedBuildingForTierDelegate.IsValid())
    {
        SelectedBuildingForTierDelegate->OnBuildingTierActivated.RemoveDynamic(this, &USFInGameHUDWidget::HandleSelectedBuildingTierActivated);
    }

    SelectedBuildingForTierDelegate = nullptr;


    if (SelectedProductionBuilding.IsValid())
    {
        SelectedProductionBuilding->OnSpawnableUnitsChanged.RemoveDynamic(this, &USFInGameHUDWidget::HandleSelectedProductionBuildingTierActivated);
    }

    SelectedProductionBuilding = nullptr;
}
void USFInGameHUDWidget::
HandleSelectedProductionBuildingTierActivated(
    const TArray<FName>& SpawnableRowNames
)
{
    SF_LOG(
        Log,
        "[HUD] Production building tier activated: %d",
        SpawnableRowNames.Num()
    );

    AActor* Actor = SelectedActor.Get();

    if (!IsValid(Actor))
    {
        return;
    }

    FSFSelectableInfo UpdatedInfo =
        ISFSelectable::Execute_GetSelectableInfo(Actor);

    // const FHealthData HealthData =
    //     ISFSelectable::Execute_GetHealthData(Actor);
    //
    // UpdatedInfo.Tier =
    //     HealthData.CurrentTier + 1;

    ApplySelectionUIData(
        Actor,
        UpdatedInfo
    );

    SelectedInfo = UpdatedInfo;

    if (IsValid(SelectionPanel))
    {
        SelectionPanel->ShowSelection(
            SelectedInfo
        );

        SelectionPanel->SetTierUpButtonVisible(
            IsOwnedByLocalPlayer(Actor) &&
            SelectedInfo.SelectableType ==
            ESFSelectableType::ProductionBuilding
        );
    }
}

void USFInGameHUDWidget::HandleSelectedBuildingTierActivated(int32 ActivatedTier)
{
    AActor* Actor = SelectedActor.Get();

    if (!IsValid(Actor))
    {
        return;
    }

    FSFSelectableInfo UpdatedInfo =
        ISFSelectable::Execute_GetSelectableInfo(
            Actor
        );

    UpdatedInfo.Tier =
        ActivatedTier + 1;

    ApplySelectionUIData(
        Actor,
        UpdatedInfo
    );

    SelectedInfo =
        UpdatedInfo;

    if (IsValid(SelectionPanel))
    {
        SelectionPanel->ShowSelection(
            SelectedInfo
        );
    }
}

void USFInGameHUDWidget::RefreshRallyPointIndicator()
{
    HideRallyPointIndicator();

    TArray<ASFProductionBuilding*> SelectedProductionBuildings;

    for (const TObjectPtr<AActor>& ActorPtr : SelectedActors)
    {
        ASFProductionBuilding* ProductionBuilding =
            Cast<ASFProductionBuilding>(ActorPtr.Get());

        if (IsValid(ProductionBuilding) &&
            !SelectedProductionBuildings.Contains(ProductionBuilding))
        {
            SelectedProductionBuildings.Add(ProductionBuilding);
        }
    }

    // 단일 선택 경로에서는 SelectedActors가 아직 갱신되기 전에도
    // 이 함수가 호출될 수 있으므로 기본 선택 액터를 보완한다.
    if (SelectedProductionBuildings.IsEmpty())
    {
        ASFProductionBuilding* ProductionBuilding =
            Cast<ASFProductionBuilding>(SelectedActor.Get());

        if (IsValid(ProductionBuilding))
        {
            SelectedProductionBuildings.Add(ProductionBuilding);
        }
    }

    for (ASFProductionBuilding* ProductionBuilding : SelectedProductionBuildings)
    {
        CreateRallyPointIndicatorsForBuilding(ProductionBuilding);
    }
}

void USFInGameHUDWidget::CreateRallyPointIndicatorsForBuilding(
    ASFProductionBuilding* ProductionBuilding
)
{
    if (!IsValid(ProductionBuilding) ||
        !IsOwnedByLocalPlayer(ProductionBuilding) ||
        !ProductionBuilding->HasRallyPoint())
    {
        return;
    }

    UStaticMesh* PlaneMesh =
        RallyIndicatorMesh.Get();

    if (!IsValid(PlaneMesh))
    {
        SF_LOG(
            Error,
            "[SFInGameHUDWidget] RallyIndicatorMesh is invalid."
        );

        return;
    }

    FVector BuildingOrigin;
    FVector BuildingExtent;

    ProductionBuilding->GetActorBounds(
        false,
        BuildingOrigin,
        BuildingExtent
    );

    const FVector RallyPoint =
        ProductionBuilding->GetRallyPoint();

    const float IndicatorZ =
        RallyPoint.Z + 25.f;

    USceneComponent* RootComponent =
        ProductionBuilding->GetRootComponent();

    if (!IsValid(RootComponent))
    {
        return;
    }

    auto CreateRallyMesh =
        [
            ProductionBuilding,
            RootComponent,
            PlaneMesh
        ](
            UMaterialInterface* InMaterial,
            const FVector& InScale,
            const FVector& InLocation,
            const FRotator& InRotation,
            int32 InSortPriority
            ) -> UStaticMeshComponent*
        {
            if (!IsValid(InMaterial))
            {
                return nullptr;
            }

            UStaticMeshComponent* NewIndicator =
                NewObject<UStaticMeshComponent>(
                    ProductionBuilding
                );

            if (!IsValid(NewIndicator))
            {
                return nullptr;
            }

            ProductionBuilding->AddInstanceComponent(
                NewIndicator
            );

            NewIndicator->SetupAttachment(
                RootComponent
            );

            NewIndicator->SetAbsolute(
                true,
                true,
                true
            );

            NewIndicator->SetMobility(
                EComponentMobility::Movable
            );

            NewIndicator->SetStaticMesh(
                PlaneMesh
            );

            NewIndicator->SetMaterial(
                0,
                InMaterial
            );

            NewIndicator->SetCollisionEnabled(
                ECollisionEnabled::NoCollision
            );

            NewIndicator->SetGenerateOverlapEvents(
                false
            );

            NewIndicator->SetCastShadow(
                false
            );

            NewIndicator->SetReceivesDecals(
                false
            );

            NewIndicator->SetTranslucentSortPriority(
                InSortPriority
            );

            NewIndicator->RegisterComponent();

            NewIndicator->SetWorldLocation(
                InLocation
            );

            NewIndicator->SetWorldRotation(
                InRotation
            );

            NewIndicator->SetWorldScale3D(
                InScale
            );

            return NewIndicator;
        };

    /*
     * 1. 랠리 링 Mesh
     *
     * 기본 Plane 크기는 약 100 x 100이므로
     * 120은 월드 기준 약 120 크기다.
     */

    const float RallyRingMeshSize =
        90.f;

    if (UStaticMeshComponent* RallyPointIndicator =
        CreateRallyMesh(
            RallyPointIndicatorMaterial,
            FVector(
                RallyRingMeshSize / 100.f,
                RallyRingMeshSize / 100.f,
                1.f
            ),
            FVector(
                RallyPoint.X,
                RallyPoint.Y,
                IndicatorZ
            ),
            FRotator::ZeroRotator,
            12
        ))
    {
        RallyPointIndicators.Add(RallyPointIndicator);
    }

    /*
     * 2. 건물 중심에서 랠리포인트까지 경로 계산
     */

    const FVector PathStart(
        BuildingOrigin.X,
        BuildingOrigin.Y,
        IndicatorZ
    );

    const FVector PathEnd(
        RallyPoint.X,
        RallyPoint.Y,
        IndicatorZ
    );

    const FVector PathDelta =
        PathEnd - PathStart;

    const float PathLength =
        FVector2D(
            PathDelta.X,
            PathDelta.Y
        ).Size();

    if (PathLength <= KINDA_SMALL_NUMBER)
    {
        return;
    }

    const float PathYaw =
        FMath::RadiansToDegrees(
            FMath::Atan2(
                PathDelta.Y,
                PathDelta.X
            )
        );

    /*
     * 3. 점선 경로 Mesh
     */

    if (UStaticMeshComponent* RallyPathIndicator =
        CreateRallyMesh(
            RallyPathIndicatorMaterial,
            FVector(
                PathLength / 100.f,
                RallyPathIndicatorWidth / 100.f,
                1.f
            ),
            (PathStart + PathEnd) * 0.5f,
            FRotator(
                0.f,
                PathYaw,
                0.f
            ),
            11
        ))
    {
        RallyPathIndicators.Add(RallyPathIndicator);
    }

    SF_LOG(
        Log,
        "[SFInGameHUDWidget] Rally mesh indicators refreshed | Length: %.1f",
        PathLength
    );
}

void USFInGameHUDWidget::HideRallyPointIndicator()
{
    for (UStaticMeshComponent* Indicator : RallyPointIndicators)
    {
        if (IsValid(Indicator))
        {
            Indicator->DestroyComponent();
        }
    }
    RallyPointIndicators.Empty();

    for (UStaticMeshComponent* Indicator : RallyPathIndicators)
    {
        if (IsValid(Indicator))
        {
            Indicator->DestroyComponent();
        }
    }
    RallyPathIndicators.Empty();
}

bool USFInGameHUDWidget::IsPointerOverHUDPanel()
{
    const FGeometry ViewportGeometry =
        UWidgetLayoutLibrary::GetViewportWidgetGeometry(
            this
        );

    const FVector2D MouseViewportPosition =
        UWidgetLayoutLibrary::GetMousePositionOnViewport(
            this
        );

    if (MouseViewportPosition.X < 0.f ||
        MouseViewportPosition.Y < 0.f)
    {
        return false;
    }

    /*
     * GetMousePositionOnViewport()는 뷰포트 로컬 좌표이고,
     * CachedGeometry().IsUnderLocation()은 절대 좌표를 사용한다.
     * 따라서 절대 좌표로 변환해서 비교한다.
     */
    const FVector2D MouseAbsolutePosition =
        ViewportGeometry.LocalToAbsolute(
            MouseViewportPosition
        );

    /*
     * 람다:
     * 이 함수 안에서만 잠깐 사용하는 작은 검사 함수다.
     *
     * Widget이 유효하고 화면에 보이며,
     * 현재 마우스 위치가 위젯 영역 안에 있으면 true를 반환한다.
     */
    auto IsWidgetUnderMouse =
        [&MouseAbsolutePosition](
            const UWidget* Widget
            ) -> bool
        {
            if (!IsValid(Widget))
            {
                return false;
            }

            if (!Widget->IsVisible())
            {
                return false;
            }

            return Widget->GetCachedGeometry()
                .IsUnderLocation(
                    MouseAbsolutePosition
                );
        };

    // 선택 정보 패널
    if (IsWidgetUnderMouse(SelectionPanel))
    {
        return true;
    }

    if (UWidget* SelectedInfoWidget =
        GetWidgetFromName(
            TEXT("SelectedInfoPanel")
        ))
    {
        if (IsWidgetUnderMouse(SelectedInfoWidget))
        {
            return true;
        }
    }

    if (UWidget* SelectionWidget =
        GetWidgetFromName(
            TEXT("SelectionPanel")
        ))
    {
        if (IsWidgetUnderMouse(SelectionWidget))
        {
            return true;
        }
    }

    // 다중 선택 패널
    if (IsWidgetUnderMouse(MultiSelectionPanel))
    {
        return true;
    }

    // 생산 건물 패널
    // ProductionUnitEntry는 이 패널 안에서 동적으로 생성되므로
    // 부모 패널 영역을 검사하면 함께 처리된다.
    if (IsWidgetUnderMouse(ProductionBuildingPanel))
    {
        return true;
    }

    // 가운데 패널의 자식이 아닌 빈 공간도 HUD 입력으로 처리한다.
    if (IsWidgetUnderMouse(CenterPanel))
    {
        return true;
    }

    // BindWidget 연결 전/이름 기반 WBP 구성도 지원한다.
    if (UWidget* CenterPanelWidget =
        GetWidgetFromName(TEXT("CenterPanel")))
    {
        if (IsWidgetUnderMouse(CenterPanelWidget))
        {
            return true;
        }
    }

    // 자원 패널
    if (UWidget* ResourceWidget =
        GetWidgetFromName(
            TEXT("ResourcePanel")
        ))
    {
        if (IsWidgetUnderMouse(ResourceWidget))
        {
            return true;
        }
    }

    // 미니맵 패널
    if (UWidget* MinimapWidget =
        GetWidgetFromName(
            TEXT("MinimapPanel")
        ))
    {
        if (IsWidgetUnderMouse(MinimapWidget))
        {
            return true;
        }
    }

    if (UWidget* MinimapWidget =
        GetWidgetFromName(
            TEXT("WBP_SFMinimapPanel")
        ))
    {
        if (IsWidgetUnderMouse(MinimapWidget))
        {
            return true;
        }
    }

    // 빌드 카탈로그의 실제 패널만 검사한다.
    // ActiveBuildingModeWidget 전체를 검사하면 월드 배치 영역까지 막을 수 있다.
    if (IsValid(ActiveBuildingModeWidget))
    {
        const FName BuildPanelNames[] =
        {
            TEXT("BuildCatalogPanel"),
            TEXT("BuildingModePanelSizeBox")
        };

        for (const FName& BuildPanelName : BuildPanelNames)
        {
            UWidget* BuildPanelWidget =
                ActiveBuildingModeWidget->GetWidgetFromName(
                    BuildPanelName
                );

            if (IsWidgetUnderMouse(BuildPanelWidget))
            {
                return true;
            }
        }
    }

    return false;
}

void USFInGameHUDWidget::SaveCurrentSelectionToControlGroup(
    int32 GroupIndex,
    bool bAppend
)
{
    if (!ControlGroups.IsValidIndex(GroupIndex))
    {
        return;
    }

    // 기존 액터들의 바인딩 제거
    UnbindControlGroupActorsDestroyed();

    FSFControlGroup& ControlGroup =
        ControlGroups[GroupIndex];

    if (!bAppend)
    {
        ControlGroup.Actors.Reset();
    }

    for (const TObjectPtr<AActor>& ActorPtr : SelectedActors)
    {
        AActor* Actor = ActorPtr.Get();

        if (IsValid(Actor) &&
            IsOwnedByLocalPlayer(Actor))
        {
            ControlGroup.Actors.AddUnique(Actor);
        }
    }

    // 액터를 실제로 추가한 뒤 다시 바인딩
    BindControlGroupActorsDestroyed();

    if (!IsValid(ControlGroupWidget))
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT(
                "[SFInGameHUDWidget] "
                "ControlGroupWidget is invalid."
            )
        );
    }
    else
    {
        ControlGroupWidget->UpdateControlGroupSlot(
            GroupIndex,
            SelectedInfo.PortraitImage,
            ControlGroup.Actors.Num()
        );

        UE_LOG(
            LogTemp,
            Warning,
            TEXT(
                "[SFInGameHUDWidget] "
                "Control group UI updated | Index: %d | Count: %d"
            ),
            GroupIndex,
            ControlGroup.Actors.Num()
        );
    }

    SF_LOG(
        Log,
        "[SFInGameHUDWidget] Control group %d saved | Count: %d",
        GroupIndex + 1,
        ControlGroup.Actors.Num()
    );
}

void USFInGameHUDWidget::SelectControlGroup(
    int32 GroupIndex
)
{
    if (!ControlGroups.IsValidIndex(GroupIndex))
    {
        return;
    }

    TArray<AActor*> ValidActors;
    FSFControlGroup& ControlGroup =
        ControlGroups[GroupIndex];

    for (int32 Index = ControlGroup.Actors.Num() - 1;
        Index >= 0;
        --Index)
    {
        AActor* Actor =
            ControlGroup.Actors[Index].Get();

        if (!IsValid(Actor) ||
            !IsOwnedByLocalPlayer(Actor))
        {
            ControlGroup.Actors.RemoveAt(Index);
            continue;
        }

        ValidActors.Add(Actor);
    }

    if (ValidActors.IsEmpty())
    {
        return;
    }

    Algo::Reverse(ValidActors);

    ApplyMultipleSelection(ValidActors);

    SF_LOG(
        Log,
        "[SFInGameHUDWidget] Control group %d selected | Count: %d",
        GroupIndex + 1,
        ValidActors.Num()
    );
}

void USFInGameHUDWidget::AddActorToCurrentSelection(
    AActor* Actor
)
{
    if (!IsValid(Actor))
    {
        return;
    }

    if (!IsOwnedByLocalPlayer(Actor))
    {
        return;
    }

    for (const TObjectPtr<AActor>& ExistingActorPtr : SelectedActors)
    {
        if (ExistingActorPtr.Get() == Actor)
        {
            return;
        }
    }

    TArray<AActor*> CombinedActors;
    CombinedActors.Reserve(SelectedActors.Num() + 1);

    for (const TObjectPtr<AActor>& ExistingActorPtr : SelectedActors)
    {
        AActor* ExistingActor = ExistingActorPtr.Get();

        if (IsValid(ExistingActor))
        {
            CombinedActors.Add(ExistingActor);
        }
    }

    CombinedActors.Add(Actor);

    ApplyMultipleSelection(CombinedActors);

    SF_LOG(
        Log,
        "[SFInGameHUDWidget] Shift-click added actor | Count: %d",
        CombinedActors.Num()
    );
}

void USFInGameHUDWidget::BindControlGroupActorsDestroyed()
{
    for (const FSFControlGroup& Group : ControlGroups)
    {
        for (const TObjectPtr<AActor>& ActorPtr : Group.Actors)
        {
            AActor* Actor = ActorPtr.Get();

            if (!IsValid(Actor))
            {
                continue;
            }

            Actor->OnDestroyed.AddUniqueDynamic(
                this,
                &USFInGameHUDWidget::HandleControlGroupActorDestroyed
            );
        }
    }
}

void USFInGameHUDWidget::UnbindControlGroupActorsDestroyed()
{
    for (const FSFControlGroup& Group : ControlGroups)
    {
        for (const TObjectPtr<AActor>& ActorPtr : Group.Actors)
        {
            AActor* Actor = ActorPtr.Get();

            if (!IsValid(Actor))
            {
                continue;
            }

            Actor->OnDestroyed.RemoveDynamic(
                this,
                &USFInGameHUDWidget::HandleControlGroupActorDestroyed
            );
        }
    }
}

void USFInGameHUDWidget::HandleControlGroupActorDestroyed(
    AActor* DestroyedActor
)
{
    if (DestroyedActor == nullptr)
    {
        return;
    }

    for (
        int32 GroupIndex = 0;
        GroupIndex < ControlGroups.Num();
        ++GroupIndex
        )
    {
        FSFControlGroup& Group =
            ControlGroups[GroupIndex];

        const int32 OldCount =
            Group.Actors.Num();

        Group.Actors.RemoveAll(
            [DestroyedActor](
                const TObjectPtr<AActor>& ActorPtr
                )
            {
                AActor* Actor =
                    ActorPtr.Get();

                return
                    Actor == DestroyedActor ||
                    !IsValid(Actor);
            }
        );

        if (Group.Actors.Num() == OldCount)
        {
            continue;
        }

        if (Group.Actors.IsEmpty())
        {
            if (IsValid(ControlGroupWidget))
            {
                ControlGroupWidget->RemoveControlGroupSlot(
                    GroupIndex
                );
            }

            SF_LOG(
                Log,
                "[SFInGameHUDWidget] "
                "Control group %d became empty.",
                GroupIndex + 1
            );
        }
        else
        {
            if (IsValid(ControlGroupWidget))
            {
                ControlGroupWidget->UpdateControlGroupSlotCount(
                    GroupIndex,
                    Group.Actors.Num()
                );
            }
        }
    }

    DestroyedActor->OnDestroyed.RemoveDynamic(
        this,
        &USFInGameHUDWidget::HandleControlGroupActorDestroyed
    );
}

void USFInGameHUDWidget::HandleMultiSelectionRemoveRequested(
    AActor* Actor
)
{
    if (!IsValid(Actor))
    {
        return;
    }

    const int32 RemovedIndex =
        SelectedActors.IndexOfByPredicate(
            [Actor](const TObjectPtr<AActor>& ActorPtr)
            {
                return ActorPtr.Get() == Actor;
            }
        );

    if (RemovedIndex == INDEX_NONE)
    {
        return;
    }

    const bool bWasPrimary =
        SelectedActor.Get() == Actor;

    Actor->OnDestroyed.RemoveDynamic(
        this,
        &USFInGameHUDWidget::HandleSelectedActorDestroyed
    );

    HideSelectionIndicator(Actor);

    SelectedActors.RemoveAt(RemovedIndex);

    if (SelectedActors.IsEmpty())
    {
        SelectedActor = nullptr;
        SelectedInfo = FSFSelectableInfo();

        ClearSelection();
        return;
    }

    if (bWasPrimary)
    {
        UnbindSelectedHealthDelegate();

        SelectedActor = nullptr;
        SelectedInfo = FSFSelectableInfo();

        // 남은 첫 번째 액터를 내부적으로 새 대표 액터로 설정
        // 사용자가 클릭한 것처럼 포커스하지는 않음
        HandlePrimarySelectionRequested(
            SelectedActors[0].Get()
        );

        return;
    }

    RefreshMultiSelectionPanel();
    RefreshRallyPointIndicator();

    UE_LOG(
        LogTemp,
        Log,
        TEXT(
            "[SFInGameHUDWidget] "
            "Shift-click removed actor | Remaining: %d"
        ),
        SelectedActors.Num()
    );
}