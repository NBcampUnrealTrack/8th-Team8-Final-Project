#include "SFBaseBuilding.h"
#include "ActorComponent/HP/HealthComponent.h"
#include "ActorComponent/Progress/ProgressComponent.h"
#include "ActorComponent/Combat/CombatTargetComponent.h"
#include "ActorComponent/Product/ProductComponent.h"
#include "Datas/Building/BuildingAppearance.h"
#include "Game/GameState/SFGameState.h"
#include "Game/Subsystem/SFDataTableManager.h"
#include "Kismet/GameplayStatics.h"

ASFBaseBuilding::ASFBaseBuilding(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
	
	HealthComponent = CreateDefaultSubobject<UHealthComponent>("HealthComponent");
	ProgressComponent = CreateDefaultSubobject<UProgressComponent>("ProgressComponent");
	CombatTargetComponent = CreateDefaultSubobject<UCombatTargetComponent>("CombatTargetComponent");
}

void ASFBaseBuilding::InitializeBuildingData()
{
	USFDataTableManager* Subsystem = GetGameInstance()->GetSubsystem<USFDataTableManager>();
	if (!IsValid(Subsystem)) return;
	
	const FBuildingAppearance* Row = Subsystem->GetDataRow<FBuildingAppearance>(EDataTableType::Building, RowName);
	if (!Row)
	{
		UE_LOG(LogTemp, Warning, TEXT("Building RowName is INVALID"));
		return;
	}
	
	InitInfo(Row);
	LoadTierData(CurrentTier);
	// LoadTierRatioList(CurrentTier); // get ratio at datatable
	// LoadTierMeshList(CurrentTier);
	/////////////////////////////////////////////////////////////////
	///
	// ApplyPreviewMesh(); // set preview mesh
	// ApplyPreviewMaterial(true); // true = green false = red;
}

void ASFBaseBuilding::RefreshVisual()
{
	UStaticMesh* Mesh = ResolveMesh();
	if (IsValid(StaticMeshComponent) && StaticMeshComponent->GetStaticMesh() == Mesh)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[%s] RefreshVisual: 해당 상태(%d)에 적용할 메시가 없습니다. Row=%s"),
			*GetName(), static_cast<int32>(CurrentBuildingState), *RowName.ToString());
		return;
	}

	SetMesh(Mesh);
}

void ASFBaseBuilding::TakeDamaged_Implementation(float InDamage)
{
	HealthComponent->ApplyDamage(InDamage);
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
	Data.UpgradeCost = CurrentUpgradeBuildCost; //cost of upgrade - current tier
	return Data;
}

void ASFBaseBuilding::OnTakeAttacking(float InBeforeHP, float InCurrentHP, float InMaxHP)
{
	const FBuildingTier* Tier = GetCurrentTier();
	if (!Tier || Tier->DamageStages.Num() == 0) return;

	const float Ratio = (InMaxHP > 0.f) ? (InCurrentHP / InMaxHP) : 0.f;
	UE_LOG(LogTemp, Warning, TEXT("%s ::Current HP: %f / %f"),*GetName(), InCurrentHP, InMaxHP);
	// ★CHANGED: FindStageIndex → FindDamageStageIndex ("이하" 규칙)
	const int32 NewIndex = FBuildingAppearance::FindDamageStageIndex(Tier->DamageStages, Ratio);

	if (NewIndex == CurrentDamageIndex) return;           // ★CHANGED: INDEX_NONE 도 유효한 값이다

	CurrentDamageIndex = NewIndex;

	// 건설 중이면 ResolveMesh 가 무시한다. 완성 시점에 한 번에 반영된다.
	RefreshVisual();
}

void ASFBaseBuilding::HandleHealthZero()
{
	UE_LOG(LogTemp, Warning, TEXT("ABuildingBase::OnDeath"));
	
	// play sound
	// destroy mesh
	// collision unenable
	// stop progress
	
	// do not destroy()
	
	// virtual function
	SetBuildingState(EBuildingState::Destroyed);
	//OnBuildingDestroyed();
}

void ASFBaseBuilding::ApplySpawnMode()
{
	// ApplySpawnMode() 맨 앞
	UE_LOG(LogTemp, Warning, TEXT("[%s] ApplySpawnMode: SpawnMode=%d, Time=%f"), *GetName(), (int32)SpawnMode, GetWorld()->GetTimeSeconds());
	
	switch (SpawnMode)
	{
	case ESFBuildingSpawnMode::ViewPortPlacedMode:
		{
			// 건설 연출(진행 게이지, 사운드) 없이 곧장 완성 상태로 진입한다.
			SetBuildingState(EBuildingState::Completed);

			// 생산 건물의 스폰 가능 유닛 목록 채우기 등,
			// "건설이 끝났을 때" 필요한 후크는 연출 유무와 무관하게 실행해 준다.
			OnConstructionFinished();
			break;
		}

	case ESFBuildingSpawnMode::PlayerPlacedMode:
		{
			// 고스트 메시 + 초록 프리뷰 머티리얼로 시작한다.
			// 실제 진행(UnderConstruction)은 배치가 확정될 때 TryStartConstruction() 이 담당한다.
			SetBuildingState(EBuildingState::Preview);
			break;
		}

	default:
		break;
	}
}

bool ASFBaseBuilding::TryStartConstruction()
{
	if (CurrentBuildingState != EBuildingState::Preview)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("ASFBaseBuilding::TryStartConstruction: Preview 상태가 아니어서 거부됩니다. Current=%d"),
			static_cast<int32>(CurrentBuildingState));
		return false;
	}

	SetBuildingState(EBuildingState::UnderConstruction);
	return true;

}

// void ASFBaseBuilding::LoadTierRatioList(int32 InCurrentTier)
// {
// 	if (!AllTierMeshesSoftArray.TierBuildingState.IsValidIndex(InCurrentTier))
// 	{
// 		UE_LOG(LogTemp, Error, TEXT("ASFBaseBuilding::LoadTierRatioList: Invalid Tier %d / %d"),
// 			InCurrentTier, AllTierMeshesSoftArray.TierBuildingState.Num());
// 		return;
// 	}
// 	
// 	int32 CountKeys = AllTierMeshesSoftArray.TierBuildingState[InCurrentTier].Ratio_Mesh.GetKeys(CurrentTierRatios);
// 	
// 	CurrentTierRatios.Sort([](const float A, const float B)
// 	{
// 		return A < B;
// 	});
// 	
// 	if (CountKeys <= 0)
// 	{
// 		UE_LOG(LogTemp, Warning, TEXT("ABuildingBase::GetTierRatioList: TMap_ratio is Invalid"));
// 		return;
// 	}
// 	
// 	ProgressComponent->SetQuarterOfRatios(CurrentTierRatios);
// 	BuildingCompleteSound = CachedTiersInfo.[InCurrentTier].CompleteSoundBase;
// }

void ASFBaseBuilding::EnterPreviewState()
{
	//SetBuildingState(EBuildingState::Preview);

	RefreshVisual();
	ApplyPreviewMaterial(true);
}

void ASFBaseBuilding::BeginConstruction()
{
	CurrentStageIndex = 0;            // ★MOVED: LoadTierData 앞으로
	LoadTierData(CurrentTier);

	StaticMeshComponent->SetMaterial(0, nullptr);
	RefreshVisual();

	if (IsValid(BuildingStartSound))   // ★ADD
	{
		UGameplayStatics::PlaySoundAtLocation(GetWorld(), BuildingStartSound, GetActorLocation());
	}

	//
	ProgressComponent->StartProgress();
}

void ASFBaseBuilding::EnterCompletedState()
{ // set last mesh completed
	StaticMeshComponent->SetMaterial(0, nullptr);
	// set targetable yes
	
	RefreshVisual();    
}

void ASFBaseBuilding::EnterDestroyedState()
{
	if (IsValid(ProgressComponent))
	{
		ProgressComponent->StopProgress();      // 건설 중 파괴 시 진행 정지
	}

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

// void ASFBaseBuilding::ApplyPreviewMesh()
// {
// 	if (CurrentTierMeshes.IsEmpty())
// 	{
// 		UE_LOG(LogTemp, Error, TEXT("ASFBaseBuilding::ApplyPreviewMesh:CurrentTierMeshes is Empty"));
// 		return;
// 	}
// 	UStaticMesh* InMesh = CurrentTierMeshes[CurrentTierMeshes.Num() - 1];
// 	StaticMeshComponent->SetStaticMesh(InMesh);
// }

void ASFBaseBuilding::ApplyPreviewMaterial(const bool CanPlaced)
{
	// ApplyPreviewMaterial() 맨 앞
	UE_LOG(LogTemp, Warning, TEXT("[%s] ApplyPreviewMaterial(%d), Time=%f"), *GetName(), CanPlaced, GetWorld()->GetTimeSeconds());
	if (!IsValid(BuildingPreviewMaterialInstance))
	{
		UE_LOG(LogTemp, Error, TEXT("PreviewMeshMaterial is None"));
		return;
	}
	
	if (CanPlaced)
	{
		OverlayMID = UMaterialInstanceDynamic::Create(BuildingPreviewMaterialInstance, this);
		OverlayMID->SetVectorParameterValue("PreviewColor", FLinearColor(0.048881,1.000000,0.000000,1.000000));
		StaticMeshComponent->SetMaterial(0, OverlayMID);
	}
	else
	{
		OverlayMID = UMaterialInstanceDynamic::Create(BuildingPreviewMaterialInstance, this);
		OverlayMID->SetVectorParameterValue("PreviewColor", FLinearColor(0.700000,0.000000,0.000000,1.000000));
		StaticMeshComponent->SetMaterial(0, OverlayMID);
	}
}

UStaticMesh* ASFBaseBuilding::ResolveMesh() const
{
	switch (CurrentBuildingState)
	{
	case EBuildingState::Preview:
		return GetCompletedMesh();

	case EBuildingState::UnderConstruction:
		return CurrentConstructionMeshes.IsValidIndex(CurrentStageIndex)
			? CurrentConstructionMeshes[CurrentStageIndex]
			: nullptr;

	case EBuildingState::Completed:
	case EBuildingState::Destroyed:
		if (CurrentDamageMeshes.IsValidIndex(CurrentDamageIndex))
		{
			return CurrentDamageMeshes[CurrentDamageIndex];
		}
		return GetCompletedMesh();       // DamageStages 미입력 시 기존 동작

	default:
		return nullptr;
	}
}

UStaticMesh* ASFBaseBuilding::GetCompletedMesh() const
{
	return CurrentConstructionMeshes.Num() > 0 ? CurrentConstructionMeshes.Last() : nullptr;
}

const FBuildingTier* ASFBaseBuilding::GetCurrentTier() const
{
	return CachedTiersInfo.IsValidIndex(CurrentTier) ? &CachedTiersInfo[CurrentTier] : nullptr;
}

void ASFBaseBuilding::BindFunctionWithComponent()
{
	if (false == ProgressComponent->OnChangedRatio.IsAlreadyBound(this, &ThisClass::ChangeMeshByRatio))
	{
		ProgressComponent->OnChangedRatio.AddDynamic(this, &ThisClass::ChangeMeshByRatio);
	}
	if (false == ProgressComponent->OnCompletedBuilding.IsAlreadyBound(this, &ThisClass::HandleCompleteConstruction))
	{
		ProgressComponent->OnCompletedBuilding.AddDynamic(this, &ThisClass::HandleCompleteConstruction);
	}
	
	if (false == HealthComponent->OnHealthValueChanged.IsAlreadyBound(this, &ThisClass::OnTakeAttacking))
	{
		HealthComponent->OnHealthValueChanged.AddDynamic(this, &ThisClass::OnTakeAttacking);
	}
	if (false == HealthComponent->OnHealthIsZero.IsAlreadyBound(this, &ThisClass::HandleHealthZero))
	{
		HealthComponent->OnHealthIsZero.AddDynamic(this, &ThisClass::HandleHealthZero);
	}
	
}

void ASFBaseBuilding::UnBindFunctionWithComponent()
{
	if (true == ProgressComponent->OnChangedRatio.IsAlreadyBound(this, &ThisClass::ChangeMeshByRatio))
	{
		ProgressComponent->OnChangedRatio.RemoveDynamic(this, &ThisClass::ChangeMeshByRatio);
	}
	if (true == ProgressComponent->OnCompletedBuilding.IsAlreadyBound(this, &ThisClass::HandleCompleteConstruction))
	{
		ProgressComponent->OnCompletedBuilding.RemoveDynamic(this, &ThisClass::HandleCompleteConstruction);
	}
	
	if (true == HealthComponent->OnHealthValueChanged.IsAlreadyBound(this, &ThisClass::OnTakeAttacking))
	{
		HealthComponent->OnHealthValueChanged.RemoveDynamic(this, &ThisClass::OnTakeAttacking);
	}
	if (true == HealthComponent->OnHealthIsZero.IsAlreadyBound(this, &ThisClass::HandleHealthZero))
	{
		HealthComponent->OnHealthIsZero.RemoveDynamic(this, &ThisClass::HandleHealthZero);
	}
}

void ASFBaseBuilding::ChangeMeshByRatio(int32 InCurrentRatioIndex)
{
	if (!CurrentConstructionMeshes.IsValidIndex(InCurrentRatioIndex))
	{
		UE_LOG(LogTemp, Error, TEXT("ASFBaseBuilding::ChangeMeshByRatio: Invalid Index %d / %d"),
			InCurrentRatioIndex, CurrentConstructionMeshes.Num());
		return;
	}

	CurrentStageIndex = InCurrentRatioIndex;    // ★CHANGED
	RefreshVisual();
}

void ASFBaseBuilding::PlayBuildingEffect()
{
	if (!IsValid(BuildingCompleteSound))                          // ★ADD
	{
		UE_LOG(LogTemp, Warning, TEXT("PlayBuildingEffect: CompleteSound is None. RowName=%s"),
			*RowName.ToString());
		return;
	}
	
	UGameplayStatics::PlaySoundAtLocation(
		GetWorld(),
		BuildingCompleteSound,
		GetActorLocation(),
		FRotator::ZeroRotator);

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

// void ASFBaseBuilding::LoadTierMeshList(int32 InCurrentTier)
// {
// 	// load > will change assetmanager
// 	CurrentTierMeshes.Empty();
// 	
// 	if (!AllTierMeshesSoftArray.TierBuildingState.IsValidIndex(InCurrentTier))
// 	{
// 		UE_LOG(LogTemp, Error, TEXT("ASFBaseBuilding::LoadTierMeshList: Invalid Tier %d / %d"),
// 			InCurrentTier, AllTierMeshesSoftArray.TierBuildingState.Num());
// 		return;
// 	}
// 	
// 	for ( auto Key : CurrentTierRatios)
// 	{
// 		UStaticMesh* LoadMesh = AllTierMeshesSoftArray.TierBuildingState[InCurrentTier].Ratio_Mesh[Key].LoadSynchronous();
// 		CurrentTierMeshes.Add(LoadMesh);
// 	}
// }

void ASFBaseBuilding::InitInfo(const FBuildingAppearance* InData)
{
	if (!InData)
	{
		UE_LOG(LogTemp, Warning, TEXT("ASFBaseBuilding::InitInfo: InData is invalid."));
		return;
	}

	// TODO: FindStageIndex 는 Threshold 오름차순을 전제한다.
	//       현재는 데이터가 정렬되어 입력된다고 가정. 검증/정렬은 추후 추가.
	
	// appearance에서 1회만 적용되는 값(초기화)
	CachedTiersInfo    = InData->BuildingMeshesByTiers;
	CachedGridSize = InData->GridSize;
	DisplayName    = InData->DisplayName;

	BuildingPreviewMaterialInstance = InData->PreviewMeshMaterial.LoadSynchronous();   // ★CHANGED
	UE_LOG(LogTemp, Warning, TEXT("InitInfo_SetMeshes:: Completed."));
}

void ASFBaseBuilding::LoadTierData(int32 InCurrentTier)
{ // 티어별 메쉬 로딩
	CurrentConstructionMeshes.Reset();
	CurrentDamageMeshes.Reset();
	BuildingStartSound = nullptr;
	BuildingCompleteSound = nullptr;

	if (!CachedTiersInfo.IsValidIndex(InCurrentTier))
	{
		UE_LOG(LogTemp, Error, TEXT("ASFBaseBuilding::LoadTierData: Invalid Tier %d / %d"),
			InCurrentTier, CachedTiersInfo.Num());
		return;
	}

	const FBuildingTier& Tier = CachedTiersInfo[InCurrentTier];
	
	CurrentConstructBuildCost = Tier.ConstructCost; // 나중에 복구값으로 + 초기값 ui
	CurrentUpgradeBuildCost = Tier.UpgradeCost; // 업그레이드시 ui
	
	// 건설 단계 — 
	TArray<float> Thresholds;
	Thresholds.Reserve(Tier.ConstructionStages.Num());
	CurrentConstructionMeshes.Reserve(Tier.ConstructionStages.Num());

	for (const FBuildingStage& Stage : Tier.ConstructionStages)
	{ // FBuildingStage == float , ustatic mesh
		Thresholds.Add(Stage.Threshold);
		CurrentConstructionMeshes.Add(Stage.Mesh.LoadSynchronous());
	}

	// 피해 단계
	CurrentDamageMeshes.Reserve(Tier.DamageStages.Num());
	for (const FBuildingStage& Stage : Tier.DamageStages)
	{
		CurrentDamageMeshes.Add(Stage.Mesh.LoadSynchronous());
	}

	BuildingStartSound = Tier.StartConstructSound.LoadSynchronous();
	BuildingCompleteSound       = Tier.CompleteSound.LoadSynchronous();

	if (IsValid(ProgressComponent))
	{
		// not start 미리 duration set
		ProgressComponent->SetQuarterOfRatios(Thresholds);
		ProgressComponent->SetDuration(Tier.ConstructionDuration);
	}

	// 티어 업그레이드 = 완전 회복.
	// 메시를 다 채운 뒤에 불러야 한다 — 브로드캐스트가 OnTakeAttacking → RefreshVisual 로 이어진다.
	if (IsValid(HealthComponent))
	{	// maxhealth초기화
		HealthComponent->InitHealth(Tier.MaxHealth);
	}
}

void ASFBaseBuilding::UpgradeBuilding(int32 UpgradeValue)
{
	// 업그레이드는 건물건설완료일때만
	if (CurrentBuildingState != EBuildingState::Completed) return;

	// 업그레이드는 다음단계가 있을때만 (최종티어는 x)
	const int32 NextTier = CurrentTier + UpgradeValue;
	if (!CachedTiersInfo.IsValidIndex(NextTier))     // ★CHANGED
	{
		UE_LOG(LogTemp, Error, TEXT("ASFBaseBuilding::UpgradeBuilding: Excess Index %d / %d"),
			NextTier, CachedTiersInfo.Num());
		return;
	}
#pragma region CheckResources
	// // TODO: CachedTiers[CurrentTier].UpgradeCost 로 자원 검사·차감
	// gamestate resource check
	// ASFGameState* SFGameState = Cast<ASFGameState>(GetWorld()->GetGameState());
	// if (!IsValid(SFGameState)) return;
	// if (!SFGameState->TrySpendCost(CurrentUpgradeBuildCost.Mineral, CurrentUpgradeBuildCost.Ether))
	// 	return;
#pragma endregion 
	
	CurrentTier = NextTier;
	SetBuildingState(EBuildingState::UnderConstruction);
}

void ASFBaseBuilding::SetMesh(UStaticMesh* InMesh)
{
	// SetMesh() 맨 앞
	UE_LOG(LogTemp, Warning, TEXT("[%s] SetMesh: %s, Time=%f"), *GetName(), InMesh ? *InMesh->GetName() : TEXT("null"), GetWorld()->GetTimeSeconds());
	
	if (!InMesh)
	{
		UE_LOG(LogTemp, Warning, TEXT("ABuildingBase::SetMesh: InMesh is None"));
		return;
	}
	StaticMeshComponent->SetStaticMesh(InMesh);
}

void ASFBaseBuilding::HandleCompleteConstruction()
{
	SetBuildingState(EBuildingState::Completed);
	PlayBuildingEffect();
	// product begin in childclass
	OnConstructionFinished();
}

// int32 ASFBaseBuilding::GetTeamIndex() const
// {
// 	if (!IsValid(CombatTargetComponent))
// 	{
// 		UE_LOG(LogTemp, Error, TEXT("ASFBaseBuilding::GetTeamIndex:CombatTargetComponent is INVALID"));
// 		return Targetable::NeutralTeam;
// 	}
// 	
// 	return CombatTargetComponent->GetTeamIndex();
// }

FBoxSphereBounds ASFBaseBuilding::GetBuildBounds() const
{
	const UStaticMesh* Mesh = StaticMeshComponent ? StaticMeshComponent->GetStaticMesh() : nullptr;
	return Mesh ? Mesh->GetBounds() : FBoxSphereBounds(ForceInit);
}

void ASFBaseBuilding::BeginPlay()
{
	Super::BeginPlay();
	
	BindFunctionWithComponent(); // bind progress
	
	InitializeBuildingData(); // hp full, init info , loadmesh
	// repec ================================================
	ApplySpawnMode(); // complete or preview
	// repec ================================================
}

void ASFBaseBuilding::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnBindFunctionWithComponent();
	Super::EndPlay(EndPlayReason);
	
}

