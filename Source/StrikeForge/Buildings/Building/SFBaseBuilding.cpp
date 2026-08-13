#include "SFBaseBuilding.h"
#include "ActorComponent/Combat/CombatTargetComponent.h"
#include "ActorComponent/HP/HealthComponent.h"
#include "ActorComponent/Progress/ProgressComponent.h"
#include "Game/GameState/SFGameState.h"
#include "Game/Subsystem/SFDataTableManager.h"
#include "Kismet/GameplayStatics.h"

ASFBaseBuilding::ASFBaseBuilding(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;

	HealthComponent = CreateDefaultSubobject<UHealthComponent>("HealthComponent");
	ProgressComponent = CreateDefaultSubobject<UProgressComponent>("ProgressComponent");
	CombatTargetComponent = CreateDefaultSubobject<UCombatTargetComponent>("CombatTargetComponent");
	//CombatTargetComponent->SetTargetingEnabled(false);
}

void ASFBaseBuilding::RefreshVisual()
{
	UStaticMesh* const ResolvedMesh = ResolveMesh();
	if (!IsValid(ResolvedMesh))
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] 적용할 메시가 없습니다. State=%d, Row=%s"),
			*GetName(),
			static_cast<int32>(CurrentBuildingState),
			*RowName.ToString());
		return;
	}

	if (IsValid(StaticMeshComponent) && StaticMeshComponent->GetStaticMesh() != ResolvedMesh)
	{
		StaticMeshComponent->SetStaticMesh(ResolvedMesh);
	}
}

void ASFBaseBuilding::TakeDamaged_Implementation(float InDamage)
{
	if (IsValid(HealthComponent))
	{
		HealthComponent->ApplyDamage(InDamage);
	}
}

FSFSelectableInfo ASFBaseBuilding::GetSelectableInfo_Implementation()
{
	FSFSelectableInfo BuildingInfo = Super::GetSelectableInfo_Implementation();
	BuildingInfo.DisplayName = DisplayName;
	BuildingInfo.CurrentHealth = HealthComponent->GetCurrentHealth();
	BuildingInfo.MaximumHealth = HealthComponent->GetMaxHealth();
	BuildingInfo.SelectableType = ESFSelectableType::None;
	BuildingInfo.bShowAttackPower = false;
	BuildingInfo.bShowMoveSpeed = false;
	BuildingInfo.bShowHealth = true;
	BuildingInfo.MoveSpeed = 0.f;
	return BuildingInfo;
}

const FHealthData ASFBaseBuilding::GetHealthData_Implementation()
{
	FHealthData Data;
	Data.DisplayName = DisplayName;
	Data.CurrentTier = CurrentTier;
	Data.HealthComponent = HealthComponent;
	Data.UpgradeCost = CurrentUpgradeBuildCost;
	return Data;
}

void ASFBaseBuilding::HandleHealthChanged(float, float, float)
{
	if (CurrentBuildingState == EBuildingState::Completed)
	{
		RefreshVisual();
	}
}

void ASFBaseBuilding::HandleHealthZero()
{
	SetBuildingState(EBuildingState::Destroyed);
}

void ASFBaseBuilding::ApplySpawnMode()
{
	switch (SpawnMode)
	{
		// build completed
	case ESFBuildingSpawnMode::ViewPortPlacedMode:
		FinalizeTierActivation(); // called when building completed.
		break;
		// wait player build
	case ESFBuildingSpawnMode::PlayerPlacedMode:
		SetBuildingState(EBuildingState::Preview);
		break;

	default:
		break;
	}
}

bool ASFBaseBuilding::TryStartConstruction()
{
	if (CurrentBuildingState != EBuildingState::Preview || !CanStartPreparedConstruction())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[%s] 건설을 시작할 수 없습니다. State=%d, PendingTier=%d"),
			*GetName(),
			static_cast<int32>(CurrentBuildingState),
			PendingTier);
		return false;
	}

	SetBuildingState(EBuildingState::UnderConstruction);
	return true;
}

void ASFBaseBuilding::EnterPreviewState()
{
	//CombatTargetComponent->SetTargetingEnabled(false);
	RefreshVisual();
	ApplyPreviewMaterial(true);
}

void ASFBaseBuilding::BeginConstruction()
{
	if (!CanStartPreparedConstruction())
	{
		UE_LOG(LogTemp, Error, TEXT("[%s] 준비된 건설 티어가 없습니다."), *GetName());
		return;
	}

	// Active가 없으면 최초 건설이다.
	// 업그레이드 중에는 기존 체력을 유지해야 하므로 초기화하지 않는다.
	if (ActiveTierAssets.TierIndex == INDEX_NONE)
	{
		const FBuildingTier* const InitialTier = CachedDefinition.FindTier(PendingTier);

		if (!InitialTier || !IsValid(HealthComponent))
		{
			return;
		}

		HealthComponent->InitHealth(InitialTier->Stats.MaxHealth);
	}

	CurrentStageIndex = 0;
	StaticMeshComponent->SetMaterial(0, nullptr);
	RefreshVisual();

	if (IsValid(PendingTierAssets.ConstructionStartSound))
	{
		UGameplayStatics::PlaySoundAtLocation(
			GetWorld(),
			PendingTierAssets.ConstructionStartSound,
			GetActorLocation());
	}

	//CombatTargetComponent->SetTargetingEnabled(true);
	ProgressComponent->StartProgress();
}

void ASFBaseBuilding::EnterCompletedState()
{
	//CombatTargetComponent->SetTargetingEnabled(true);
	StaticMeshComponent->SetMaterial(0, nullptr);
	RefreshVisual();
}

void ASFBaseBuilding::EnterDestroyedState()
{
	//CombatTargetComponent->SetTargetingEnabled(false);

	if (IsValid(ProgressComponent))
	{
		ProgressComponent->StopProgress();
	}
	// Deactivate Ability
	DeactivateActiveTier();
	
	ApplyDestroyedCollision();
	RefreshVisual();
	OnBuildingDestroyed();
}

void ASFBaseBuilding::ApplyDestroyedCollision()
{
	if (!IsValid(StaticMeshComponent)) return;

	StaticMeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	StaticMeshComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	StaticMeshComponent->SetCanEverAffectNavigation(false);
}

void ASFBaseBuilding::ApplyPreviewMaterial(bool bCanPlace)
{
	if (!IsValid(BuildingPreviewMaterialInstance))
	{
		return;
	}
	// make once
	if (!IsValid(OverlayMID))
	{
		OverlayMID = UMaterialInstanceDynamic::Create(BuildingPreviewMaterialInstance, this);
	}

	if (!IsValid(OverlayMID) || !IsValid(StaticMeshComponent))
	{
		return;
	}

	const FLinearColor PreviewColor = bCanPlace
		? FLinearColor(0.048881f, 1.f, 0.f, 1.f)
		: FLinearColor(0.7f, 0.f, 0.f, 1.f);

	OverlayMID->SetVectorParameterValue(TEXT("PreviewColor"), PreviewColor);
	StaticMeshComponent->SetMaterial(0, OverlayMID);
}

UStaticMesh* ASFBaseBuilding::ResolveMesh() const
{
	switch (CurrentBuildingState)
	{
	case EBuildingState::Preview:
		return PendingTierAssets.CompletedMesh;

	case EBuildingState::UnderConstruction:
	{
		if (PendingTierAssets.ConstructionMeshes.IsValidIndex(CurrentStageIndex))
		{
			UStaticMesh* ConstructionMesh =	PendingTierAssets.ConstructionMeshes[CurrentStageIndex];

			if (IsValid(ConstructionMesh))
			{
				return ConstructionMesh;
			}
		}

		return PendingTierAssets.CompletedMesh;
	}

	case EBuildingState::Completed:
		return ResolveDamageMesh(ActiveTierAssets);

	case EBuildingState::Destroyed:
		{
			const FBuildingTierRuntimeAssets& DestroyedAssets =
				ActiveTierAssets.TierIndex != INDEX_NONE
					? ActiveTierAssets
					: PendingTierAssets;

			return ResolveDamageMesh(DestroyedAssets);
		}
	default:
		return nullptr;
	}
}

UStaticMesh* ASFBaseBuilding::ResolveDamageMesh(const FBuildingTierRuntimeAssets& Assets) const
{
	const FBuildingTier* Tier =
	CachedDefinition.FindTier(Assets.TierIndex);

	if (!Tier || !IsValid(HealthComponent))
	{
		return Assets.CompletedMesh;
	}

	const int32 DamageIndex =
		Tier->Visual.FindDamageStageIndex(
			HealthComponent->GetHealthPercent());

	if (Assets.DamageMeshes.IsValidIndex(DamageIndex))
	{
		UStaticMesh* DamageMesh =
			Assets.DamageMeshes[DamageIndex];

		if (IsValid(DamageMesh))
		{
			return DamageMesh;
		}
	}

	return Assets.CompletedMesh;
}

const FBuildingTier* ASFBaseBuilding::GetCurrentTier() const
{
	return CachedDefinition.FindTier(CurrentTier);
}

void ASFBaseBuilding::BindComponentDelegates()
{
	if (!ProgressComponent->OnChangedRatio.IsAlreadyBound(this, &ThisClass::ChangeMeshByRatio))
	{
		ProgressComponent->OnChangedRatio.AddDynamic(this, &ThisClass::ChangeMeshByRatio);
	}

	if (!ProgressComponent->OnCompletedBuilding.IsAlreadyBound(this, &ThisClass::HandleCompleteConstruction))
	{
		ProgressComponent->OnCompletedBuilding.AddDynamic(this, &ThisClass::HandleCompleteConstruction);
	}

	if (!HealthComponent->OnHealthValueChanged.IsAlreadyBound(this, &ThisClass::HandleHealthChanged))
	{
		HealthComponent->OnHealthValueChanged.AddDynamic(this, &ThisClass::HandleHealthChanged);
	}

	if (!HealthComponent->OnHealthIsZero.IsAlreadyBound(this, &ThisClass::HandleHealthZero))
	{
		HealthComponent->OnHealthIsZero.AddDynamic(this, &ThisClass::HandleHealthZero);
	}
}

void ASFBaseBuilding::UnbindComponentDelegates()
{
	ProgressComponent->OnChangedRatio.RemoveDynamic(this, &ThisClass::ChangeMeshByRatio);
	ProgressComponent->OnCompletedBuilding.RemoveDynamic(this, &ThisClass::HandleCompleteConstruction);
	HealthComponent->OnHealthValueChanged.RemoveDynamic(this, &ThisClass::HandleHealthChanged);
	HealthComponent->OnHealthIsZero.RemoveDynamic(this, &ThisClass::HandleHealthZero);
}

void ASFBaseBuilding::ChangeMeshByRatio(int32 InCurrentRatioIndex)
{
	if (!PendingTierAssets.ConstructionMeshes.IsValidIndex(
				InCurrentRatioIndex))
	{
		return;
	}

	CurrentStageIndex = InCurrentRatioIndex;
	RefreshVisual();
}

void ASFBaseBuilding::PlayConstructionCompleteSound()
{
	if (!IsValid(ActiveTierAssets.ConstructionCompleteSound))
	{
		return;
	}

	UGameplayStatics::PlaySoundAtLocation(
		GetWorld(),
		ActiveTierAssets.ConstructionCompleteSound,
		GetActorLocation());
}

void ASFBaseBuilding::SetBuildingState(const EBuildingState InState)
{
	if (CurrentBuildingState == InState)
	{
		return;
	}

	CurrentBuildingState = InState;
	ApplyBuildingState(InState);
}

void ASFBaseBuilding::ApplyBuildingState(const EBuildingState InState)
{
	switch (InState)
	{
	case EBuildingState::Preview:
		EnterPreviewState();
		break;

	case EBuildingState::UnderConstruction:
		BeginConstruction();
		break;

	case EBuildingState::Completed:
		EnterCompletedState();
		break;
	case EBuildingState::Destroyed:
		EnterDestroyedState();
		break;

	default:
		break;
	}
}

void ASFBaseBuilding::UpgradeBuilding(int32 UpgradeValue)
{
	if (CurrentBuildingState != EBuildingState::Completed ||
		UpgradeValue <= 0 ||
		PendingTier != INDEX_NONE)
	{
		return;
	}

	const int32 NextTier = CurrentTier + UpgradeValue;
	if (!CachedDefinition.Tiers.IsValidIndex(NextTier))
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] 업그레이드 티어가 유효하지 않습니다. Tier=%d, Count=%d"),
			*GetName(),
			NextTier, CachedDefinition.Tiers.Num());
		return;
	}

	// ASFGameState* const GameState = GetWorld()->GetGameState<ASFGameState>();
	// if (!IsValid(GameState) ||
	// 	!GameState->CanAfford(CurrentUpgradeBuildCost.Mineral, CurrentUpgradeBuildCost.Ether))
	// {
	// 	return;
	// }

	if (!PrepareTier(NextTier))
	{
		return;
	}

	// if (!GameState->TrySpendCost(CurrentUpgradeBuildCost.Mineral, CurrentUpgradeBuildCost.Ether))
	// {
	// 	PendingTierAssets.Reset();
	// 	PendingTier = INDEX_NONE;
	// 	return;
	// }

	SetBuildingState(EBuildingState::UnderConstruction);
}

void ASFBaseBuilding::HandleCompleteConstruction()
{
	if (!FinalizeTierActivation())
	{
		UE_LOG(	LogTemp,Error, TEXT("[%s] 티어 활성화에 실패했습니다."),
			*GetName()
		);
		return;
	}

	PlayConstructionCompleteSound();
}



void ASFBaseBuilding::BeginPlay()
{
	Super::BeginPlay();
	// delegate bind - healthcomp, progresscomp
	BindComponentDelegates();
	// try take info at data table
	if (!LoadBuildingDefinition()) return;
	// if load failed return
	if (!PrepareTier(CurrentTier)) return;
	// set viewport mode or controller construct mode
	ApplySpawnMode();
}

void ASFBaseBuilding::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DeactivateActiveTier();
	UnbindComponentDelegates();
	Super::EndPlay(EndPlayReason);
}

bool ASFBaseBuilding::LoadBuildingDefinition()
{
	const USFDataTableManager* DataManager = GetGameInstance()->GetSubsystem<USFDataTableManager>();
	if (!IsValid(DataManager)) return false;

	const FBuildingAppearance* Definition = DataManager->GetDataRow<FBuildingAppearance>(
		EDataTableType::Building,
		RowName,
		TEXT("ASFBaseBuilding::LoadBuildingDefinition"));
	// CHECK data table empty 
	if (!Definition || Definition->Tiers.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("[%s] Tiers가 비어 있습니다. Row=%s"),
			*GetName(),
			*RowName.ToString());
		return false;
	}
	// CACHED building appearance about all tiers
	CachedDefinition = *Definition;
	
	// FORLOOP building's Tiers / Sort By Ratio
	for (FBuildingTier& Tier : CachedDefinition.Tiers) 
	{   
		Tier.Visual.ConstructionStages.Sort(
			[](const FBuildingStage& Left, const FBuildingStage& Right)
			{
				return Left.Threshold < Right.Threshold;
			});

		Tier.Visual.DamageStages.Sort(
			[](const FBuildingStage& Left, const FBuildingStage& Right)
			{
				return Left.Threshold < Right.Threshold;
			});
	}
	
	// CACHED once in allgameplay = never changed in runtime
	DisplayName = CachedDefinition.DisplayName;
	CachedGridSize = CachedDefinition.GridSize;
	BuildingPreviewMaterialInstance = CachedDefinition.PreviewMaterial.LoadSynchronous();
	
	return true;
}

bool ASFBaseBuilding::PrepareTier(int32 TierIndex)
{
	// Getter TierInfo by currentTier
	const FBuildingTier* const Tier = CachedDefinition.FindTier(TierIndex);
	if (!Tier || !IsValid(ProgressComponent))
	{
		UE_LOG(LogTemp, Error, TEXT("[%s] 티어를 준비할 수 없습니다. Tier=%d, Count=%d"),
			*GetName(),
			TierIndex,
			CachedDefinition.Tiers.Num());
		return false;
	}
	
	CurrentConstructBuildCost = Tier->Construction.ConstructCost;
	CurrentUpgradeBuildCost = Tier->Construction.UpgradeCost;
	
	// CACHED LoadAsset by currentTier
	FBuildingTierRuntimeAssets NewAssets;
	NewAssets.TierIndex = TierIndex;

	const FBuildingVisualData& Visual = Tier->Visual;
	// CHECK data table empty
	if (Visual.ConstructionStages.IsEmpty() || Visual.DamageStages.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("[%s] Tier %d의 건설 또는 피해 단계가 비어 있습니다."),
			*RowName.ToString(),
			TierIndex);
		return false;
	}
	if (Visual.CompletedMesh.IsNull())
	{
		UE_LOG(LogTemp, Error, TEXT("[%s] Tier %d CompletedMesh가 없습니다."),
			*RowName.ToString(),
			TierIndex);
		return false;
	}

	// DEFINE lamda function CACHED Meshes
	auto LoadStageMeshes = [this, TierIndex](
		const TArray<FBuildingStage>& Stages,
		TArray<TObjectPtr<UStaticMesh>>& OutMeshes, 
		const TCHAR* StageName)
	{
		bool bAllLoaded = true;
		OutMeshes.Reserve(Stages.Num());

		for (int32 Index = 0; Index < Stages.Num(); ++Index)
		{
			UStaticMesh* const Mesh = Stages[Index].Mesh.LoadSynchronous();
			OutMeshes.Add(Mesh);

			if (!IsValid(Mesh))
			{
				bAllLoaded = false;
				UE_LOG(LogTemp, Error, TEXT("[%s] Tier %d %s[%d] 로드 실패"),
					*RowName.ToString(), TierIndex, StageName, Index);
			}
		}

		return bAllLoaded;
	};

	// CACHED ratio(construct)
	TArray<float> ConstructionThresholds;
	ConstructionThresholds.Reserve(Visual.ConstructionStages.Num());
	for (const FBuildingStage& Stage : Visual.ConstructionStages)
	{
		ConstructionThresholds.Add(Stage.Threshold);
	}
	
	// CACHED construct_meshes using lamda function
	const bool bConstructionMeshesLoaded = LoadStageMeshes(
		Visual.ConstructionStages,
		NewAssets.ConstructionMeshes,
		TEXT("ConstructionStage"));
	// CACHED damaged_meshes using lamda function
	const bool bDamageMeshesLoaded = LoadStageMeshes(
		Visual.DamageStages,
		NewAssets.DamageMeshes,
		TEXT("DamageStage"));
	
	// CACHED completed_mesh
	NewAssets.CompletedMesh = Visual.CompletedMesh.LoadSynchronous();
	// CACHED failed return false
	if (!bConstructionMeshesLoaded ||
		!bDamageMeshesLoaded ||
		!IsValid(NewAssets.CompletedMesh))
	{
		return false;
	}

	// CACHED sounds
	NewAssets.ConstructionStartSound = Tier->Audio.ConstructionStart.LoadSynchronous();
	NewAssets.ConstructionCompleteSound = Tier->Audio.ConstructionComplete.LoadSynchronous();
	
	// CACHED load assets
	PendingTierAssets = MoveTemp(NewAssets);
	PendingTier = TierIndex;

	ProgressComponent->SetQuarterOfRatios(ConstructionThresholds);
	ProgressComponent->SetDuration(Tier->Construction.Duration);

	CurrentStageIndex = 0;

	return true;
}

bool ASFBaseBuilding::CanStartPreparedConstruction() const
{
	return PendingTierAssets.IsPreparedFor(PendingTier) &&
		IsValid(HealthComponent) &&
		IsValid(ProgressComponent) &&
		IsValid(StaticMeshComponent);
}

bool ASFBaseBuilding::CommitPreparedTier()
{// MOVE Current <- Pending
	
	if (!PendingTierAssets.IsPreparedFor(PendingTier) || !IsValid(HealthComponent))
	{
		UE_LOG(LogTemp, Error, TEXT("[%s] 확정할 티어가 준비되지 않았습니다."), *GetName());
		return false;
	}

	const FBuildingTier* const Tier = CachedDefinition.FindTier(PendingTier);
	if (!Tier)
	{
		return false;
	}

	const int32 CommittedTier = PendingTier;
	ActiveTierAssets = MoveTemp(PendingTierAssets);
	PendingTierAssets.Reset();
	PendingTier = INDEX_NONE;

	CurrentTier = CommittedTier;
	CurrentConstructBuildCost = Tier->Construction.ConstructCost;
	CurrentUpgradeBuildCost = Tier->Construction.UpgradeCost;

	HealthComponent->InitHealth(Tier->Stats.MaxHealth);

	return true;
}

bool ASFBaseBuilding::FinalizeTierActivation()
{
	if (!CommitPreparedTier())
	{
		return false;
	}

	const FBuildingTier* ActivatedTier = GetCurrentTier();
	if (!ActivatedTier)
	{
		return false;
	}

	SetBuildingState(EBuildingState::Completed);
	ActivatedGameplayTier = CurrentTier;
	OnTierActivated(*ActivatedTier);

	return true;
}

void ASFBaseBuilding::DeactivateActiveTier()
{
	if (ActivatedGameplayTier == INDEX_NONE)
	{
		return;
	}

	const int32 TierToDeactivate = ActivatedGameplayTier;

	// 가상 함수 안에서 Destroy 등이 다시 발생해도
	// 두 번 회수되지 않도록 먼저 초기화한다.
	ActivatedGameplayTier = INDEX_NONE;

	const FBuildingTier* DeactivatedTier = CachedDefinition.FindTier(TierToDeactivate);

	if (!DeactivatedTier)
	{
		UE_LOG(	LogTemp,Warning,TEXT("[%s] 비활성화할 티어를 찾을 수 없습니다. Tier=%d"),
			*GetName(),
			TierToDeactivate
		);
		return;
	}

	OnTierDeactivated(*DeactivatedTier);
}

#pragma region Getter
FBoxSphereBounds ASFBaseBuilding::GetBuildBounds() const
{
	const UStaticMesh* Mesh = StaticMeshComponent ? StaticMeshComponent->GetStaticMesh() : nullptr;
	return Mesh ? Mesh->GetBounds() : FBoxSphereBounds(ForceInit);
}


#pragma endregion Getter