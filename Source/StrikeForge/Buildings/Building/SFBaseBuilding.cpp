#include "SFBaseBuilding.h"
#include "Buildings/Building/HouseBuilding/Nexus/SFNexusBuilding.h"
#include "Logging/SFLog.h"
#include "ActorComponent/Combat/CombatTargetComponent.h"
#include "ActorComponent/Stat/HealthComponent.h"
#include "ActorComponent/Progress/ProgressComponent.h"
#include "ActorComponent/Buff/SFBuffComponent.h"
#include "Game/GameState/SFGameState.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraSystem.h"
#include "ActorComponent/Niagara/PlayVFXComponent.h"
#include "ActorComponent/UI/SFOverheadUIComponent.h"
#include "Buildings/GC/SFBuildingDestructionActor.h"
#include "ActorComponent/UI/SFOverheadStatusComponent.h"
#include "Components/BoxComponent.h"
#include "Components/TimelineComponent.h"
#include "Datas/Building/SFBuildingDefinition.h"
#include "Game/Subsystem/SFBuildingGridSubsystem.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollection/GeometryCollectionObject.h" //gc
#include "InGame/Controller/SFInGamePlayerController.h"
#include "Engine/GameInstance.h"
#include "Game/Subsystem/SFBuildingAssetSubsystem.h"
#include "Game/Subsystem/SFCombatSubsystem.h"
#include "Game/Subsystem/SFOverheadUISubsystem.h"

ASFBaseBuilding::ASFBaseBuilding(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Stat
	HealthComponent = CreateDefaultSubobject<UHealthComponent>("HealthComponent");
	ProgressComponent = CreateDefaultSubobject<UProgressComponent>("ProgressComponent");
	CombatTargetComponent = CreateDefaultSubobject<UCombatTargetComponent>("CombatTargetComponent");
	// CombatTargetComponent->SetTargetingEnabled(false);
	BuffComponent = CreateDefaultSubobject<USFBuffComponent>(TEXT("BuffComponent"));
	// OverHead UI	
	OverheadUI = CreateDefaultSubobject<USFOverheadUIComponent>(TEXT("OverheadUI"));
	// UWidgetComponent를 상속하므로 씬 계층에 붙여야 한다.
	OverheadUI->SetupAttachment(GetRootComponent());
		
	// Hit React
	HitReactionTimeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("HitReactionTimeline"));
	// effects	
	PlayVFXComponent = CreateDefaultSubobject<UPlayVFXComponent>(TEXT("PlayVFXComponent"));
	// 머리 위 UI가 볼 상태를 모으는 곳. 값의 출처를 여기서 정규화한다.
	StatusComponent = CreateDefaultSubobject<USFOverheadStatusComponent>(TEXT("OverheadStatus"));
}

void ASFBaseBuilding::RefreshVisual()
{
	UStaticMesh* const ResolvedMesh = ResolveMesh();
	if (!IsValid(ResolvedMesh))
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] 적용할 메시가 없습니다. State=%d, Definition=%s"),
			*GetName(),
			static_cast<int32>(CurrentBuildingState),
			*GetNameSafe(BuildingDefinition));
		return;
	}

	if (IsValid(StaticMeshComponent) && StaticMeshComponent->GetStaticMesh() != ResolvedMesh)
	{
		StaticMeshComponent->SetStaticMesh(ResolvedMesh);
	}
}

void ASFBaseBuilding::TakeDamaged_Implementation(const FSFDamageInfo& DamageInfo)
{	// HP있고 건설중, 건설완료상태이면 대미지 적용
	if (IsValid(HealthComponent) && CanReceiveDamage())
	{
		// 손상 단계와 Field 위치 계산 전에 원위치 복구
		StopHitReaction();

		HealthComponent->ApplyDamage(DamageInfo.Damage);

		/*
		 * 체력 0에서는 BaseBuilding이 제거되므로
		 * 최종 파괴 연출이 HitReact를 대신한다.
		 */
		FVector HitDirection = FVector::ForwardVector;
		if (CurrentBuildingState !=	EBuildingState::Destroyed)
		{
			if (DamageInfo.HitActor.IsValid())
			{
				HitDirection = GetActorLocation() -	DamageInfo.HitActor->GetActorLocation();
			}
			PlayHitReaction(HitDirection);
		}
	}
}

FSFSelectableInfo ASFBaseBuilding::GetSelectableInfo_Implementation()
{
	FSFSelectableInfo BuildingInfo = Super::GetSelectableInfo_Implementation();
	BuildingInfo.DisplayName = DisplayName;
	BuildingInfo.CurrentHealth = HealthComponent->GetCurrentHealth();
	BuildingInfo.MaximumHealth = HealthComponent->GetMaxHealth();
	BuildingInfo.SelectableType = ESFSelectableType::None;
	BuildingInfo.Tier = CurrentTier + 1;
	BuildingInfo.bShowAttackPower = false;
	BuildingInfo.bShowMoveSpeed = false;
	BuildingInfo.bShowHealth = true;
	BuildingInfo.MoveSpeed = 0.f;
	return BuildingInfo;
}

// const FHealthData ASFBaseBuilding::GetHealthData_Implementation()
// {
// 	FHealthData Data;
// 	Data.DisplayName = DisplayName;
// 	Data.CurrentTier = CurrentTier;
// 	Data.HealthComponent = HealthComponent;
// 	Data.ConstructCost = GetConstructCost();
// 	Data.UpgradeCost = GetUpgradeCost();
// 	return Data;
// }

USoundBase* ASFBaseBuilding::GetSelectionSound_Implementation()
{
	switch (CurrentBuildingState)
	{
	case EBuildingState::UnderConstruction:
		return UnderConstructionSelectionSound;

	case EBuildingState::Completed:
		return CompletedSelectionSound;

	case EBuildingState::None:
	case EBuildingState::Preview:
	case EBuildingState::Destroyed:
	default:
		return nullptr;
	}
}

// Damagable 인터페이스 -> CanbeDamaged조건(underconstruct, completed : pass) ->
// HealthComp이동 데미지적용 -> broadcast -> 이함수 실행 -> 이 함수는 체력별 비율을 인덱스로변환하고 인덱스를 메쉬적용함수에 전달한다
// called by 죽기전까지 - 체력은 바뀔때마다 UI에 갱신해줘야함 > 바뀔때마다 broadcast필요
void ASFBaseBuilding::HandleHealthChanged // 이델리게이트는 체력이 0일때도 호출된다
(float InBeforeHP, float InCurrentHP, float InMaxHP)
{
	if (InCurrentHP <= 0.f || InMaxHP <= 0.f)
	{
		return;
	}

	UpdateDamageVisual(
		FMath::Clamp(InBeforeHP / InMaxHP, 0.f, 1.f),
		FMath::Clamp(InCurrentHP / InMaxHP, 0.f, 1.f));
}

void ASFBaseBuilding::UpdateDamageVisual(float PreviousHealthRatio, float CurrentHealthRatio)
{
	if (CurrentBuildingState != EBuildingState::Completed)
	{
		return;
	}

	const FSFBuildingTierData* Tier = GetCurrentTierInfo();

	if (!Tier)
	{
		SF_LOG(	Error, "[%s] Current Tier가 없습니다.", *GetName());
		return;
	}

	/*
	 * GC가 설정된 건물에서만 GC 파괴 단계와
	 * SurfaceFire 단계 값을 사용한다.
	 */
	if (IsValid(ActiveTierAssets.DestructionCollection))
	{
		const int32 PreviousStage =
			Tier->Visual.FindDestructionStageIndex(PreviousHealthRatio);

		const int32 CurrentStage =
			Tier->Visual.FindDestructionStageIndex(CurrentHealthRatio);

		const int32 PreviousFireStep =
			Tier->Visual.FindSurfaceFireStepIndex(PreviousHealthRatio);

		const int32 CurrentFireStep =
			Tier->Visual.FindSurfaceFireStepIndex(CurrentHealthRatio);

		const bool bReachedGCVisual =
			CurrentStage != INDEX_NONE || CurrentFireStep != INDEX_NONE;

		/*
		 * 파괴 또는 불꽃 임계값 전에는
		 * GC의 Root Proxy 상태만 유지한다.
		 */
		if (!bReachedGCVisual)
		{
			return;
		}

		if (EnsureDestructionActor())
		{
			if (CurrentStage != INDEX_NONE && CurrentStage > PreviousStage)
			{
				if (const FBuildingDestruction* StageData =
						Tier->Visual.GetDestructionStage(CurrentStage))
				{
					DestructionActor->ApplyDamageStage(
						CurrentStage, *StageData);
				}
			}

			/*
			 * 한 번의 공격으로 여러 불꽃 임계값을 넘은 경우에도
			 * 각 단계의 불꽃을 순서대로 생성한다.
			 */
			if (CurrentFireStep != INDEX_NONE && CurrentFireStep > PreviousFireStep)
			{
				const int32 FirstEnteredFireStep =
					FMath::Max(PreviousFireStep + 1, 0);

				for (int32 Step = FirstEnteredFireStep; Step <= CurrentFireStep; ++Step)	
				{
					if (const FBuildingSurfaceFireStep* StepData =
							Tier->Visual.GetSurfaceFireStep(Step))
					{
						DestructionActor->SetSurfaceFireStep(Step, StepData->FireCount);
					}
				}
			}

			return;
		}

		/*
		 * GC 생성 실패 시에는 아래 DamageMesh 방식으로
		 * 안전하게 폴백한다.
		 */
	}

	// GC로 대체됨
	// GC가 없는 건물은 기존 DamageMesh를 사용한다.
	// const int32 PreviousMeshStage =
	// 	Tier->Visual.FindDamageStageIndex(PreviousHealthRatio);
	//
	// const int32 CurrentMeshStage =
	// 	Tier->Visual.FindDamageStageIndex(CurrentHealthRatio);
	//
	// if (PreviousMeshStage == CurrentMeshStage)
	// {
	// 	return;
	// }
	//
	// RefreshVisual();
	// PlayBurningEffect(CurrentMeshStage);
	//
	// if (CurrentMeshStage != INDEX_NONE)
	// {
	// 	SF_LOG(Warning, "[%s] Damage mesh stage entered: %d",
	// 		*GetName(),	CurrentMeshStage);
	// }
}

// 체력이 0일시 broadcast되는 델리게이트와 연결됨
void ASFBaseBuilding::HandleHealthZero()
{
	// Set -> Apply -> EnterDestroyed
	SetBuildingState(EBuildingState::Destroyed);
}

void ASFBaseBuilding::HandleConstructionProgress(float PreviousRatio, float CurrentRatio)
{
	// underconstruct일때만 진행도에 따른 메쉬 적용
	if (CurrentBuildingState == EBuildingState::UnderConstruction)
	{
		const FSFBuildingTierData* PendingTierData = GetTierInfo(PendingTier);
		if (!PendingTierData) return;

		// 건설 메시 변경
		const int32 NewStageIndex =	PendingTierData->Visual.FindConstructionStageIndex(CurrentRatio);

		if (NewStageIndex != INDEX_NONE &&
			NewStageIndex != CurrentStageIndex && // 메쉬 인덱스 다름
			PendingTierAssets.ConstructionMeshes.IsValidIndex(NewStageIndex))
		{
			CurrentStageIndex = NewStageIndex;
			RefreshVisual();
			SetBoxExtent();
		}

		// 이번 진행에서 증가한 비율
		const float DeltaRatio = FMath::Clamp(CurrentRatio - PreviousRatio, 0.f, 1.f);
		if (DeltaRatio <= 0.f)	return;

		// 최초 건설과 업그레이드가 동일한 로직 사용
		const float HealthIncrease = ConstructionCurrentHealthIncrease * DeltaRatio;

		HealthComponent->IncreaseCurrentHealth(HealthIncrease);
	}
}

void ASFBaseBuilding::ApplySpawnMode()
{
	switch (SpawnMode)
	{
		// build completed
	case ESFBuildingSpawnMode::ViewPortPlacedMode:
		{
			// 현재티어
			const FSFBuildingTierData* InitialTier = GetTierInfo(PendingTier);
			if (!InitialTier) return;
			HealthComponent->InitializeHealth(InitialTier->Stats.MaxHealth);
			
			if (!TryRegistInitialGrid())
			{
				SF_LOG(
					Warning,
					"[%s] 선배치 격자 등록 실패. 해당 Anchor 건물을 건너뜁니다.",
					*GetName());
				Destroy();
				return;
			}
			
			if (!FinalizeTierActivation())
			{
				SF_LOG(
					Error,
					"[%s] 초기 티어 활성화 실패",
					*GetName());
				Destroy();
				return;
			}

			break;
		}
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
	
	BoxComponent->SetCollisionProfileName("DestroyedBuilding");
	ApplyPreviewMaterial(true);
}

void ASFBaseBuilding::BeginConstruction()
{
	if (!CanStartPreparedConstruction())
	{
		UE_LOG(LogTemp, Error, TEXT("[%s] 준비된 건설 티어가 없습니다."), *GetName());
		return;
	}
	
	ClearDestructionVisual();
	
	const FSFBuildingTierData* PendingTierData = GetTierInfo(PendingTier);
	if (!PendingTierData) return;
	// 최초 건설은 1/1에서 시작
	const bool bIsUpgrade =	ActiveTierAssets.TierIndex != INDEX_NONE;
	const float TargetMaxHealth = FMath::Max(1.f, PendingTierData->Stats.MaxHealth);
	// 최초 건설은 1/TargetMaxHealth에서 시작한다.
	// 업그레이드는 기존 최대체력과 현재체력을 보존해서 시작한다.
	const float StartMaxHealth = bIsUpgrade	?
				HealthComponent->GetMaxHealth()	: 1.f;

	const float StartCurrentHealth = bIsUpgrade	?
				HealthComponent->GetCurrentHealth()	: 1.f;
	// 건설 진행 중 현재체력에 더할 전체 수치
	ConstructionCurrentHealthIncrease = FMath::Max(0.f,TargetMaxHealth - StartMaxHealth);

	// 최대체력은 건설 시작 즉시 새 티어 값 적용
	// 현재체력은 아직 증가시키지 않는다.
	HealthComponent->InitializeHealth(TargetMaxHealth,	StartCurrentHealth);

	CurrentStageIndex = 0;
	StaticMeshComponent->SetMaterial(0, nullptr);
	// mesh
	RefreshVisual();
	BoxComponent->SetCollisionProfileName("SpawnedBuilding");
	SetBoxExtent();
	//
	if (IsValid(PendingTierAssets.ConstructionStartSound))
	{
		UGameplayStatics::PlaySoundAtLocation(
			GetWorld(),
			PendingTierAssets.ConstructionStartSound,
			GetActorLocation());
	}
	// 초기값이면 초기건설 = 업글x, 그외 = 전부 업글판정
	// 이경우일때 만약 건물을 3티어부터 생산할수있게되면 오류가 날 수 있음
	// 업그레이드때문에 건설로직에 들어온다면
	if (bIsUpgrade)
	{	// 생산컴포넌트 pause 로직
		DeactivateActiveTier();
		OnUpgradeStarted();
	}

	Tags.AddUnique(
		FName(TEXT("MinimapBuilding"))
	);

	//CombatTargetComponent->SetTargetingEnabled(true);
	
	PlayVFXComponent->PlayConstructionEffect(ConstructionEffect, StaticMeshComponent);
	// 다음티어 비동기 Request
	RequestNextTierAssetsAsync();
	ProgressComponent->StartProgress();
}

void ASFBaseBuilding::EnterCompletedState()
{
	//CombatTargetComponent->SetTargetingEnabled(true);
	PlayVFXComponent->StopConstructionEffect();
	StaticMeshComponent->SetMaterial(0, nullptr);
	RefreshVisual();
	BoxComponent->SetCollisionProfileName("SpawnedBuilding"); // 완료된 상태에서 시작하는 액터있음
	SetBoxExtent();
	
	if (IsValid(ActiveTierAssets.DestructionCollection))
	{
		EnsureDestructionActor();
	}
	
	UpdateDamageVisual(1.f,HealthComponent->GetHealthPercent());
	
	//성윤 미니맵 마커제작
	Tags.AddUnique(FName(TEXT("MinimapBuilding")));

}

void ASFBaseBuilding::EnterDestroyedState()
{
	// 이동한 GC 위치에서 최종 Strain이 계산되는 것을 방지
	StopHitReaction();
	PlayVFXComponent->StopConstructionEffect();
	//PlayVFXComponent->StopBurningEffects();
	
	SF_LOG(Warning, "EnterDestroyedState");
	//CombatTargetComponent->SetTargetingEnabled(false);

	if (IsValid(ProgressComponent))
	{
		ProgressComponent->StopProgress();
	}
	// 반드시 DeactivateActiveTier() 전에 저장
	// 완료 상태=true, 건설/업그레이드 중=false
	const bool bCanPlayCompletedGC = bIsTierActivated;

	DeactivateActiveTier();

	bool bUseGeometryCollection = false;

	// 완성된 상태에서 파괴될 때만 GC 처리
	if (bCanPlayCompletedGC)
	{
		const FSFBuildingTierData* DestructionTier =
			GetCurrentTierInfo();

		const FBuildingDestruction* FinalStage =
			DestructionTier
				? DestructionTier->Visual.GetFinalDestructionStage()
				: nullptr;

		const int32 FinalStageIndex =
			DestructionTier
				? DestructionTier->Visual.DestructionStages.Num() - 1
				: INDEX_NONE;

		bUseGeometryCollection =
			FinalStage &&
			IsValid(ActiveTierAssets.DestructionCollection) &&
			EnsureDestructionActor();

		if (bUseGeometryCollection)
		{
			ASFBuildingDestructionActor* DebrisActor = DestructionActor;
			
			DebrisActor->ApplyFinalDestruction(
				FinalStageIndex,
				*FinalStage);
			
			// 이후 DebrisActor는 자신의 LifeSpan으로 제거된다.
			DestructionActor = nullptr;
		}
	}

	// if (bUseGeometryCollection)
	// {
	// 	BoxComponent->SetCollisionEnabled(
	// 		ECollisionEnabled::NoCollision);
	// }
	// else if (bCanPlayCompletedGC)
	// {
	// 	// 완성 건물이지만 GC가 없을 때만 파괴 메시 사용
	// 	RefreshVisual();
	// 	BoxComponent->SetCollisionProfileName(
	// 		TEXT("DestroyedBuilding"));
	// 	SetBoxExtent();
	// }
	// else
	// {
	// 	// 건설 중 파괴: 완성 GC와 파괴 완료 메시 모두 사용하지 않는다.
	// 	BoxComponent->SetCollisionEnabled(
	// 		ECollisionEnabled::NoCollision);
	// }
	BoxComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	
	PlayDestroyEffect();
	//성윤 미니맵 마커제작
	Tags.Remove(
		FName(TEXT("MinimapBuilding"))
	);
	// sound effect
	OnBuildingDestroyed();
	Destroy();
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
			if (UStaticMesh* Mesh =
				PendingTierAssets.ConstructionMeshes[CurrentStageIndex])
			{
				return Mesh;
			}
		}

		return PendingTierAssets.CompletedMesh;
	}

	case EBuildingState::Completed:
		//return ResolveDamageMesh(ActiveTierAssets);
		return ActiveTierAssets.CompletedMesh;

	case EBuildingState::Destroyed:
		return nullptr;
		// const FBuildingTierRuntimeAssets& DestroyedAssets =
		// 	ActiveTierAssets.TierIndex != INDEX_NONE
		// 		? ActiveTierAssets
		// 		: PendingTierAssets;
		// if (IsValid(DestroyedAssets.DestroyedMesh))
		// {
		// 	return DestroyedAssets.DestroyedMesh.Get();
		// }

		// 예상치 못한 에셋 누락 시 마지막 피해 메시로 대체한다.
		// return ResolveDamageMesh(DestroyedAssets);
	default:
		return nullptr;
	}
}

const FSFBuildingTierData* ASFBaseBuilding::GetCurrentTierInfo() const
{
	return GetTierInfo(CurrentTier);
}

const FSFBuildingTierData* ASFBaseBuilding::GetTierInfo(int32 TierIndex) const
{
	return CachedCommonTiers.IsValidIndex(TierIndex)
	? &CachedCommonTiers[TierIndex]
	: nullptr;
}

void ASFBaseBuilding::BindComponentDelegates()
{
	if (!ProgressComponent->OnChangedProgressRatio.IsAlreadyBound(this, &ThisClass::HandleConstructionProgress))
	{
		ProgressComponent->OnChangedProgressRatio.AddDynamic(this, &ThisClass::HandleConstructionProgress);
	}

	if (!ProgressComponent->OnCompletedProgress.IsAlreadyBound(this, &ThisClass::HandleCompleteConstruction))
	{
		ProgressComponent->OnCompletedProgress.AddDynamic(this, &ThisClass::HandleCompleteConstruction);
	}

	if (!HealthComponent->OnHealthValueChanged.IsAlreadyBound(this, &ThisClass::HandleHealthChanged))
	{
		HealthComponent->OnHealthValueChanged.AddDynamic(this, &ThisClass::HandleHealthChanged);
	}

	if (!HealthComponent->OnHealthIsZero.IsAlreadyBound(this, &ThisClass::HandleHealthZero))
	{
		HealthComponent->OnHealthIsZero.AddDynamic(this, &ThisClass::HandleHealthZero);
	}
	
	StatusComponent->BindHealth(HealthComponent);
}

void ASFBaseBuilding::UnbindComponentDelegates()
{
	ProgressComponent->OnChangedProgressRatio.RemoveDynamic(this, &ThisClass::HandleConstructionProgress);
	ProgressComponent->OnCompletedProgress.RemoveDynamic(this, &ThisClass::HandleCompleteConstruction);
	HealthComponent->OnHealthValueChanged.RemoveDynamic(this, &ThisClass::HandleHealthChanged);
	HealthComponent->OnHealthIsZero.RemoveDynamic(this, &ThisClass::HandleHealthZero);

	StatusComponent->UnbindHealth(HealthComponent);
}

void ASFBaseBuilding::SetBuildingState(const EBuildingState InState)
{
	if (CurrentBuildingState == InState)
	{
		return;
	}

	CurrentBuildingState = InState;
	
	OverheadUI->SetSuppressed(InState == EBuildingState::Preview);
		
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
	if (CurrentBuildingState !=
			EBuildingState::Completed ||
		UpgradeValue <= 0 ||
		PendingTier != INDEX_NONE ||
		RequestedUpgradeTier != INDEX_NONE ||
		!IsValid(BuildingDefinition))
	{
		return;
	}

	const int32 NextTier =
		CurrentTier + UpgradeValue;

	if (!GetTierInfo(NextTier))
	{
		return;
	}

	ASFGameState* SFGS =
		GetWorld()
		? GetWorld()->GetGameState<ASFGameState>()
		: nullptr;

	if (!IsValid(SFGS))
	{
		return;
	}

	if (!IsA<ASFNexusBuilding>() &&
		NextTier >
			SFGS->GetNexusTier(GetTeamIndex()))
	{
		return;
	}

	const FResources UpgradeCost =
		GetUpgradeCost();

	if (!SFGS->CanAffordResource(
		GetTeamIndex(),
		UpgradeCost))
	{
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
		return;
	}

	RequestedUpgradeTier = NextTier;

	AssetSubsystem->RequestBuildingAssetsAsync(
		BuildingDefinition,
		NextTier,
		FSFOnBuildingAssetsReady::CreateUObject(
			this,
			&ThisClass::HandleUpgradeAssetsReady));
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

void ASFBaseBuilding::SetMyTeamIndex(const int32 InTeamIndex)
{
	TeamIndex = InTeamIndex;
	CombatTargetComponent->SetTeamIndex(TeamIndex);
}

bool ASFBaseBuilding::SetBuildingDefinition(USFBuildingDefinition* InDefinition)
{
	if (!IsValid(InDefinition))
	{
		SF_LOG(
			Error,
			"[%s] BuildingDefinition을 전달받지 못했습니다. ",
			*GetName());

		return false;
	}
	
	BuildingDefinition = InDefinition;
	return true;
}

bool ASFBaseBuilding::SetInitialTier(int32 InTierIndex)
{
	// BeginPlay가 실행된 뒤에는 초기 Tier를 변경할 수 없다.
	if (HasActorBegunPlay() ||
		!IsValid(BuildingDefinition) ||
		InTierIndex < 0 ||
		!BuildingDefinition->FindTier(InTierIndex))
	{
		return false;
	}

	CurrentTier = InTierIndex;
	return true;
}

void ASFBaseBuilding::BeginPlay()
{
	Super::BeginPlay();
		
	OverheadUI->Initialize(StaticMeshComponent);

	// delegate bind - healthcomp, progresscomp
	BindComponentDelegates();
	// try take info at data table
	if (!CacheLoadedDefinitionData())
	{
		Destroy();
		return; // 디폴트값 로딩 실패시
	}
	OnBuildingDefinitionLoaded();
	// if load failed return
	if (!PrepareLoadedTier(CurrentTier))
	{
		Destroy();
		return; // 메쉬준비 실패시
	}
	// set viewport mode or controller construct mode
	ApplySpawnMode();

	if (IsValid(HitReactionCurve))
	{
		// 타임라인전용 line-float 델리게이트 만들고 함수와 연결
		FOnTimelineFloat UpdateDelegate;
		UpdateDelegate.BindUFunction(
			this,
			FName(TEXT("UpdateHitReaction"))
		);
		
		// InterpFloat을 쓸려고 Add
		HitReactionTimeline->AddInterpFloat(
			HitReactionCurve, // CurveFloat
			UpdateDelegate	// CurveFloat라인과 델리게이트 연결
		);
		
		// 타임라인전용 이벤트델리게이트 만들고 함수와 연결
		FOnTimelineEvent FinishedDelegate;
		FinishedDelegate.BindUFunction(
			this,
			FName(TEXT("FinishHitReaction"))
		);
		
		// end delegate
		HitReactionTimeline->SetTimelineFinishedFunc(FinishedDelegate);

		HitReactionTimeline->SetLooping(false); // 반복안함
		HitReactionTimeline->SetTimelineLengthMode(ETimelineLengthMode::TL_LastKeyFrame); 
			//	TL_TimelineLength :: 시간을 다 소비하고 끝냄
			//	TL_LastKeyFrame	  :: Key를 다 지나면 끝냄
	}
}

void ASFBaseBuilding::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopHitReaction();
	
	if (IsValid(DestructionActor))
	{
		DestructionActor->Destroy();
		DestructionActor = nullptr;
	}
	
	if (bGridRegistered)
	{
		if (USFBuildingGridSubsystem* Grid = GetWorld()->GetSubsystem<USFBuildingGridSubsystem>())
		{
			Grid->UnregisterBuildingGrid(this, RegisteredGridInfo);
			bGridRegistered = false;
		}
	}
	
	UnbindComponentDelegates();
	Super::EndPlay(EndPlayReason);
}

void ASFBaseBuilding::OnBuildingDefinitionLoaded()
{
	if (IsValid(BuildingDefinition))
	{
		OverheadUI->ApplySettings(BuildingDefinition->OverheadUISettings);
	}

	const USFCombatSubsystem* Combat = USFCombatSubsystem::Get(this);
	if (!IsValid(Combat))	return;

	// GetTeamIndex()는 내부에서 CombatTargetComponent의 팀을 읽습니다.
	const bool bIsEnemy = USFCombatSubsystem::IsHostileTeam(
		GetTeamIndex(), Combat->GetLocalPlayerTeam());

	// 색도 상태의 일부다. 위젯을 직접 만지지 않는다.
	StatusComponent->SetHealthColor(bIsEnemy
		? SFOverheadUIColors::EnemyHealth
		: SFOverheadUIColors::AllyHealth);
}

bool ASFBaseBuilding::CacheLoadedDefinitionData()
{
	if (!IsValid(BuildingDefinition))
	{
		SF_LOG(
			Error,
			"[%s] BuildingDefinition이 설정되지 않았습니다.",
			*GetName());

		return false;
	}

	const int32 TierCount = BuildingDefinition->GetTierCount();
	if (TierCount <= 0)
	{
		SF_LOG(
			Error,
			"[%s] Definition의 Tier가 비어 있습니다. Definition=%s",
			*GetName(),
			*GetNameSafe(BuildingDefinition));

		return false;
	}

	CachedCommonTiers.Reset(TierCount);

	// ? for문으로 찾아야만하는 이유가 있나?
	// definition에 현재티어의 tierdata를 반환하는 함수를만들고
	// 그걸 호출만해서 데이터를 받아오면되는거아닌가?
	for (int32 TierIndex = 0; TierIndex < TierCount; ++TierIndex)
	{
		const FSFBuildingTierData* SourceTier =	BuildingDefinition->FindTier(TierIndex);
		if (!SourceTier)
		{
			SF_LOG(
				Error,
				"[%s] Definition Tier를 찾지 못했습니다. Tier=%d",
				*GetName(),
				TierIndex);

			CachedCommonTiers.Reset();
			return false;
		}

		/*
		 * 파생 Tier의 공통 부분만 의도적으로 복사한다.
		 * Population, Mining, Production 데이터는 파생 건물이
		 * Definition에서 직접 조회한다.
		 */
		FSFBuildingTierData& CachedTier = CachedCommonTiers.Add_GetRef(*SourceTier);

		CachedTier.Visual.ConstructionStages.Sort([](const FBuildingStage& Left, const FBuildingStage& Right)
			{
				return Left.Threshold < Right.Threshold;
			});
#pragma region GC로대체
		// CachedTier.Visual.DamageStages.Sort([](const FBuildingStage& Left, const FBuildingStage& Right)
		// 	{
		// 		return Left.Threshold > Right.Threshold;
		// 	});
#pragma endregion 
		CachedTier.Visual.DestructionStages.Sort([](const FBuildingDestruction& Left, const FBuildingDestruction& Right)
			{
				return Left.HealthThreshold >  Right.HealthThreshold;
			});
		CachedTier.Visual.SurfaceFireSteps.Sort([](const FBuildingSurfaceFireStep& Left, const FBuildingSurfaceFireStep& Right)
			{
				return Left.HealthThreshold > Right.HealthThreshold;
			});
		
	}
	
	DisplayName = BuildingDefinition->DisplayName;
	// grid
	ConfiguredGridSize = BuildingDefinition->GridSize;
	// sound
	UnderConstructionSelectionSound = BuildingDefinition->UnderConstructionSelectionSound.Get();
	CompletedSelectionSound = BuildingDefinition->CompletedSelectionSound.Get();
	DestroyedSound = BuildingDefinition->DestroyedSound.Get();
	// niagara
	ConstructionEffect = BuildingDefinition->ConstructEffect.Get();
	DestroyedEffect = BuildingDefinition->DestroyEffect.Get();
	DamagedBurningEffect = BuildingDefinition->DamagedBurningEffect.Get();
	// preview ghost
	BuildingPreviewMaterialInstance = BuildingDefinition->PreviewMaterial.Get();

	return true;
}

bool ASFBaseBuilding::PrepareLoadedTier(int32 TierIndex)
{
	// appear -> tier[0,1,2] -> tier[0] -> construct[0,1,2], dmg[0,1,2]
	// Getter TierInfo by currentTier
	const FSFBuildingTierData* const Tier = GetTierInfo(TierIndex);
	if (!Tier || !IsValid(ProgressComponent))
	{
		UE_LOG(LogTemp, Error, TEXT("[%s] 티어를 준비할 수 없습니다. Tier=%d, Count=%d"),
			*GetName(),
			TierIndex,
			CachedCommonTiers.Num());
		return false;
	}
	
	// CACHED LoadAsset by currentTier
	FBuildingTierRuntimeAssets NewAssets;
	NewAssets.TierIndex = TierIndex;

	const FBuildingVisualData& Visual = Tier->Visual;
	// CHECK data table empty
	// support no construct mesh
	if (Visual.CompletedMesh.IsNull())
	{
		UE_LOG(LogTemp, Error, TEXT("[%s] Tier %d CompletedMesh가 없습니다."),
			*GetNameSafe(BuildingDefinition),
			TierIndex);
		return false;
	}
#pragma region GC로대체
	// if (Visual.DestroyedMesh.IsNull())
	// {
	// 	UE_LOG(LogTemp, Error, TEXT("[%s] Tier %d DestroyedMesh가 없습니다."),
	// 		*GetNameSafe(BuildingDefinition),
	// 		TierIndex);
	// 	return false;
	// }
#pragma endregion 
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
			UStaticMesh* const Mesh = Stages[Index].Mesh.Get();
			OutMeshes.Add(Mesh);

			if (!IsValid(Mesh))
			{
				bAllLoaded = false;
				UE_LOG(LogTemp, Error, TEXT("[%s] Tier %d %s[%d] 로드 실패"),
					*GetNameSafe(BuildingDefinition), TierIndex, StageName, Index);
			}
		}

		return bAllLoaded;
	};
	
	// CACHED construct_meshes using lamda function(LoadStageMeshes)
	const bool bConstructionMeshesLoaded = LoadStageMeshes(
		Visual.ConstructionStages,
		NewAssets.ConstructionMeshes,
		TEXT("ConstructionStage"));
	
	// CACHED damaged_meshes using lamda function(LoadStageMeshes)
	// GC로 대체
	// const bool bDamageMeshesLoaded = LoadStageMeshes(
	// 	Visual.DamageStages,
	// 	NewAssets.DamageMeshes,
	// 	TEXT("DamageStage"));
	
	// CACHED completed_mesh
	NewAssets.CompletedMesh = Visual.CompletedMesh.Get();
	//GC로대체
	//NewAssets.DestroyedMesh = Visual.DestroyedMesh.Get();
	
	
	NewAssets.DestructionCollection = Visual.DestructionCollection.Get();

	if (Visual.ConstructionStages.IsEmpty() ||
		!bConstructionMeshesLoaded ||
		!IsValid(NewAssets.CompletedMesh) ||
		!IsValid(NewAssets.DestructionCollection))
	{
		SF_LOG(Error,
			"[%s] Tier %d 준비 실패: "
			"건설 단계 메시, 완성 메시 또는 GC가 유효하지 않습니다.",
			*GetNameSafe(BuildingDefinition),
			TierIndex);

		return false;
	}

	// CACHED sounds
	NewAssets.ConstructionStartSound = Tier->Audio.ConstructionStart.Get();
	NewAssets.ConstructionCompleteSound = Tier->Audio.ConstructionComplete.Get();

	// CACHED load assets
	PendingTierAssets = MoveTemp(NewAssets);
	PendingTier = TierIndex;

	ProgressComponent->InitProgressWorks(Tier->Construction.RequiredWork, 1.f);

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

// tier index ++
bool ASFBaseBuilding::CommitPreparedTier()
{// MOVE Current <- Pending
	
	if (!PendingTierAssets.IsPreparedFor(PendingTier) || !IsValid(HealthComponent))
	{
		UE_LOG(LogTemp, Error, TEXT("[%s] 확정할 티어가 준비되지 않았습니다."), *GetName());
		return false;
	}

	const FSFBuildingTierData* const Tier = GetTierInfo(PendingTier);
	if (!Tier)
	{
		return false;
	}

	const int32 CommittedTier = PendingTier;
	ActiveTierAssets = MoveTemp(PendingTierAssets);
	PendingTierAssets.Reset();
	PendingTier = INDEX_NONE;

	CurrentTier = CommittedTier;

	return true;
}

bool ASFBaseBuilding::FinalizeTierActivation()
{
	if (!CommitPreparedTier())
	{
		return false;
	}

	if (!GetCurrentTierInfo())
	{
		return false;
	}

	SetBuildingState(EBuildingState::Completed);

	// 가상 함수 실행 전에 내부 상태 확정
	bIsTierActivated = true;
	OnTierActivated();

	OnBuildingTierActivated.Broadcast(CurrentTier);

	return true;
}
// enterdestorystate call this function
void ASFBaseBuilding::DeactivateActiveTier()
{

	if (!bIsTierActivated)
	{
		return;
	}

	// 파생 훅에서 재진입해도 중복 비활성화되지 않도록 먼저 변경
	bIsTierActivated = false;
	OnTierDeactivated();
}

#pragma region Sound
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

void ASFBaseBuilding::PlayDestroyEffect()
{
	const FTransform EffectLocation = GetActorTransform();
	
	SF_LOG(Warning, "EffectLocation : %s", *EffectLocation.GetLocation().ToString());
	
	const FVector MeshCompScale = StaticMeshComponent->GetComponentScale();
	if (IsValid(PlayVFXComponent))
	{
		PlayVFXComponent->PlayDestroyNiagaraEffect(
			DestroyedEffect,
			EffectLocation,
			MeshCompScale);
	}
	
	if (DestroyedSound)
		UGameplayStatics::PlaySoundAtLocation(
			this,
			DestroyedSound,
			EffectLocation.GetLocation());
}
#pragma endregion

bool ASFBaseBuilding::CanReceiveDamage() const
{	// 건설중, 건설완료 상태일때만 true -> TakeDamaged 인터페이스에서 조건으로 활용
	return CurrentBuildingState == EBuildingState::UnderConstruction ||
	   CurrentBuildingState == EBuildingState::Completed;
}

void ASFBaseBuilding::SetBoxExtent()
{
	FBoxSphereBounds BoundsInParent = StaticMeshComponent->CalcBounds(
			StaticMeshComponent->GetRelativeTransform());
	
	FVector DownSizeBoxExtent = BoundsInParent.BoxExtent * 0.85f;
	float SubBoxExtentZ = BoundsInParent.BoxExtent.Z - DownSizeBoxExtent.Z;
	
	FVector CustomOrigin = BoundsInParent.Origin;
	CustomOrigin.Z -= SubBoxExtentZ;
	
	BoxComponent->SetBoxExtent(DownSizeBoxExtent);
	BoxComponent->SetRelativeLocation(CustomOrigin);
	
	// 메시가 바뀌면 바운즈도 바뀌므로 위젯도 같이 맞춘다.
	OverheadUI->RefreshTransform();
}

#pragma region Getter
FBoxSphereBounds ASFBaseBuilding::GetBuildBounds() const
{
	return StaticMeshComponent->CalcBounds(
		StaticMeshComponent->GetRelativeTransform());
}
#pragma endregion Getter

#pragma region Widget
void ASFBaseBuilding::NotifyActorBeginCursorOver()
{
	Super::NotifyActorBeginCursorOver();

	if (CurrentBuildingState == EBuildingState::Preview) return;

	OverheadUI->SetElementVisible(ESFOverheadElement::Health, true);
}

void ASFBaseBuilding::NotifyActorEndCursorOver()
{
	OverheadUI->SetElementVisible(ESFOverheadElement::Health, false);
	
	Super::NotifyActorEndCursorOver();
}
#pragma endregion 

#pragma region HitReact
void ASFBaseBuilding::UpdateHitReaction(float CurveValue)
{
	USceneComponent* Target =
		ActiveHitReactionTarget.Get();

	if (!IsValid(Target))
	{
		return;
	}

	const FVector LocalOffset =
		HitLocalDirection *
		HitMoveDistance *
		CurveValue;

	const FRotator LocalRotationOffset(
		HitRotationAmount * CurveValue,
		0.f,
		-HitRotationAmount * CurveValue);

	const FQuat BaseRotation =
		OriginalHitReactionTransform.GetRotation();

	FTransform TargetTransform =
		OriginalHitReactionTransform;

	TargetTransform.SetLocation(
		OriginalHitReactionTransform.GetLocation() +
		BaseRotation.RotateVector(LocalOffset));

	TargetTransform.SetRotation(
		(BaseRotation *
		 LocalRotationOffset.Quaternion())
		.GetNormalized());

	/*
	 * GC의 기존 파편 속도는 변경하지 않고
	 * Collection 전체 Transform만 움직인다.
	 */
	Target->SetWorldTransform(
		TargetTransform,
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
}

void ASFBaseBuilding::FinishHitReaction()
{
	USceneComponent* Target =
		ActiveHitReactionTarget.Get();

	if (IsValid(Target))
	{
		Target->SetWorldTransform(
			OriginalHitReactionTransform,
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
	}

	ActiveHitReactionTarget.Reset();
}


void ASFBaseBuilding::PlayHitReaction(const FVector& WorldDirection)
{
	if (!IsValid(HitReactionTimeline) ||
		!IsValid(HitReactionCurve))
	{
		return;
	}

	USceneComponent* Target = ResolveHitReactionTarget();

	if (!IsValid(Target))
	{
		return;
	}

	ActiveHitReactionTarget = Target;

	OriginalHitReactionTransform = Target->GetComponentTransform();

	FVector HorizontalWorldDirection = WorldDirection;
	HorizontalWorldDirection.Z = 0.f;
	HorizontalWorldDirection.Normalize();

	/*
	 * UpdateHitReaction()에서 BaseRotation을 다시 적용하므로
	 * 여기에서는 월드 방향을 Target 로컬 방향으로 변환한다.
	 */
	HitLocalDirection =
		OriginalHitReactionTransform
			.InverseTransformVectorNoScale(HorizontalWorldDirection).GetSafeNormal();

	HitReactionTimeline->PlayFromStart();
}
#pragma endregion 

#pragma region GCDestroy

bool ASFBaseBuilding::EnsureDestructionActor()
{
	if (IsValid(DestructionActor))
	{
		return true;
	}

	UGeometryCollection* Collection =
		ActiveTierAssets.DestructionCollection;
	
	if (!IsValid(Collection))
	{
		return false;
	}

	StopHitReaction();
	
	const FTransform VisualTransform =
		StaticMeshComponent->GetComponentTransform();
	
	if (!IsValid(DestructionActorClass))
	{
		SF_LOG(Warning, "EnsureDestructionClass is null");
		return false;
	}

	ASFBuildingDestructionActor* SpawnedActor =
		GetWorld()->SpawnActorDeferred<ASFBuildingDestructionActor>(
			DestructionActorClass,
			VisualTransform,
			nullptr,
			nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

	if (!IsValid(SpawnedActor))
	{
		return false;
	}

	// 추가: 이 건물의 DA에서 읽은 불꽃 에셋을 전달한다.
	SpawnedActor->SetSurfaceFireEffect(DamagedBurningEffect.Get());
	
	if (!SpawnedActor->InitializeDestruction(Collection))
	{
		SpawnedActor->Destroy();
		return false;
	}

	SpawnedActor->FinishSpawning(VisualTransform);

	DestructionActor = SpawnedActor;
	/*
	 * FinishSpawning으로 Root Proxy 렌더러가 등록된 뒤,
	 * 같은 게임 틱에서 기존 Static Mesh만 숨긴다.
	 */
	if (IsValid(StaticMeshComponent))
	{
		StaticMeshComponent->SetVisibility(false, false);
	}

	return true;
}

void ASFBaseBuilding::ClearDestructionVisual()
{
	/*
	 * HitReact 대상이 GC일 수 있으므로
	 * GC를 제거하기 전에 Timeline과 Transform을 정리한다.
	 */
	StopHitReaction();

	// if (IsValid(PlayVFXComponent))
	// {
	// 	PlayVFXComponent->StopBurningEffects(true);
	// }
	
	if (IsValid(DestructionActor))
	{
		DestructionActor->Destroy();
		DestructionActor = nullptr;
	}

	if (IsValid(StaticMeshComponent))
	{
		StaticMeshComponent->SetVisibility(true,false);
	}
}

const FSFBuildingTierData* ASFBaseBuilding::GetCostTierInfo() const
{
	const int32 CostTier =
		PendingTier != INDEX_NONE
			? PendingTier
			: CurrentTier;

	return GetTierInfo(CostTier);
}

// void ASFBaseBuilding::PlayBurningEffect(int32 DamageStageIndex)
// {
// 	if (DamageStageIndex < 0 ||
// 		!IsValid(PlayVFXComponent) ||
// 		!IsValid(DamagedBurningEffect) ||
// 		!IsValid(StaticMeshComponent))
// 	{
// 		return;
// 	}
//
// 	FVector LocalMin;
// 	FVector LocalMax;
//
// 	StaticMeshComponent->GetLocalBounds(
// 		LocalMin,
// 		LocalMax);
//
// 	const FVector LocalBottomCenter(
// 		(LocalMin.X + LocalMax.X) * 0.5f,
// 		(LocalMin.Y + LocalMax.Y) * 0.5f,
// 		LocalMin.Z);
//
// 	const FTransform MeshTransform =
// 		StaticMeshComponent->GetComponentTransform();
//
// 	const FVector ComponentScale =
// 		MeshTransform.GetScale3D().GetAbs();
//
// 	FVector SpawnBoxSize =
// 		(LocalMax - LocalMin) * ComponentScale;
//
// 	// 건물 바닥의 가로·세로 영역만 사용한다.
// 	// SpawnBoxSize.Z = 0.f;
//
// 	const FTransform SpawnTransform(
// 		MeshTransform.GetRotation(),
// 		MeshTransform.TransformPosition(
// 			LocalBottomCenter),
// 		FVector::OneVector);
//
// 	PlayVFXComponent->PlayBurningEffect(
// 		DamagedBurningEffect,
// 		SpawnTransform,
// 		SpawnBoxSize,
// 		DamageStageIndex + 1);
// }

void ASFBaseBuilding::HandleUpgradeAssetsReady(USFBuildingDefinition* InDefinition, int32 InTierIndex, bool bSuccess)
{
	if (RequestedUpgradeTier != InTierIndex)
	{
		return;
	}

	RequestedUpgradeTier = INDEX_NONE;

	if (!bSuccess ||
		InDefinition != BuildingDefinition ||
		CurrentBuildingState != EBuildingState::Completed ||
		PendingTier != INDEX_NONE)
	{
		return;
	}

	ASFGameState* SFGS =
		GetWorld()
		? GetWorld()->GetGameState<ASFGameState>()
		: nullptr;

	if (!IsValid(SFGS))
	{
		return;
	}

	if (!IsA<ASFNexusBuilding>() &&
		InTierIndex >
			SFGS->GetNexusTier(GetTeamIndex()))
	{
		return;
	}

	const FResources UpgradeCost =
		GetUpgradeCost();

	if (!SFGS->TrySpendCost(
		GetTeamIndex(),
		UpgradeCost))
	{
		return;
	}

	if (!PrepareLoadedTier(InTierIndex))
	{
		SFGS->RefundCost(
			GetTeamIndex(),
			UpgradeCost);

		return;
	}

	SetBuildingState(
		EBuildingState::UnderConstruction);
}

void ASFBaseBuilding::RequestNextTierAssetsAsync()
{
	if (!IsValid(BuildingDefinition) ||
		PendingTier == INDEX_NONE)
	{
		return;
	}

	/*
	 * 최초 건설 중:
	 * PendingTier 0 → Tier 1 준비
	 *
	 * Tier 1 업그레이드 중:
	 * PendingTier 1 → Tier 2 준비
	 */
	const int32 NextTier = PendingTier + 1;

	// 마지막 Tier라면 요청하지 않는다.
	if (!BuildingDefinition->FindTier(NextTier))
	{
		return;
	}

	UGameInstance* GameInstance = GetGameInstance();

	USFBuildingAssetSubsystem* AssetSubsystem =
		IsValid(GameInstance)
			? GameInstance
				->GetSubsystem<USFBuildingAssetSubsystem>()
			: nullptr;

	if (!IsValid(AssetSubsystem))
	{
		return;
	}

	AssetSubsystem->RequestBuildingAssetsAsync(
		BuildingDefinition,
		NextTier,
		FSFOnBuildingAssetsReady::CreateUObject(
			this,
			&ThisClass::HandleNextTierAssetsReady));
}

void ASFBaseBuilding::HandleNextTierAssetsReady(USFBuildingDefinition* InDefinition, int32 InTierIndex, bool bSuccess)
{
	if (!bSuccess ||
		InDefinition != BuildingDefinition)
	{
		SF_LOG(
			Warning,
			"[%s] 다음 Tier 선로딩 실패. "
			"Definition=%s, Tier=%d",
			*GetName(),
			*GetNameSafe(InDefinition),
			InTierIndex);
	}
}

USceneComponent* ASFBaseBuilding::ResolveHitReactionTarget() const
{
	if (IsValid(DestructionActor))
	{
		return DestructionActor->GetGeometryCollectionComponent();
	}

	return StaticMeshComponent;
}

void ASFBaseBuilding::StopHitReaction()
{
	if (IsValid(HitReactionTimeline) &&
	HitReactionTimeline->IsPlaying())
	{
		HitReactionTimeline->Stop();
	}

	FinishHitReaction();
}

bool ASFBaseBuilding::TryRegisterGrid(const FBuildingGridInfo& GridInfo)
{
	if (bGridRegistered || !GridInfo.IsValid()) return false;

	USFBuildingGridSubsystem* GridSubsystem = GetWorld()->GetSubsystem<USFBuildingGridSubsystem>();
	if (!IsValid(GridSubsystem)) return false;
	
	// TMap에 등록시도
	if (!GridSubsystem->TryRegisterBuildingGrid(this, GridInfo))
	{
		return false;
	}
	// 등록성공하면 건물이 정보를 저장
	RegisteredGridInfo = GridInfo;
	bGridRegistered = true;
	return true;
}

FResources ASFBaseBuilding::GetConstructCost() const
{
	const FSFBuildingTierData* Tier = GetCostTierInfo();

	return Tier
		? Tier->Construction.ConstructCost
		: FResources{};
}

FResources ASFBaseBuilding::GetUpgradeCost() const
{
	const FSFBuildingTierData* Tier = GetCostTierInfo();

	return Tier
		? Tier->Construction.UpgradeCost
		: FResources{};
}

bool ASFBaseBuilding::TryRegistInitialGrid()
{
	USFBuildingGridSubsystem* Grid = GetWorld()->GetSubsystem<USFBuildingGridSubsystem>();
	if (!IsValid(Grid)) return false;
	FBuildingGridInfo GridInfo = Grid->WorldCenterToGridInfo(GetActorLocation(), ConfiguredGridSize);
	if (!TryRegisterGrid(GridInfo))
	{
		SF_LOG(Error, "[%s] 선배치 건물 격자 등록 실패", *GetName());
		return false;
	}
	
	const FVector WorldCenterVector = Grid->GridInfoToWorldCenter(GridInfo);
	SetActorLocation(WorldCenterVector);
	
	return true;
}
#pragma endregion 
