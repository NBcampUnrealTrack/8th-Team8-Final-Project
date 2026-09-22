#include "SFProductionBuilding.h"
#include "ActorComponent/Product/ProductComponent.h"
#include "ActorComponent/UI/SFOverheadUIComponent.h"
#include "ActorComponent/UI/SFOverheadStatusComponent.h"
#include "Datas/Building/BuildingDefinition/SFProductionBuildingDefinition.h"
#include "InGame/HUD/Tutorial/SFTutorialSubsystem.h"

ASFProductionBuilding::ASFProductionBuilding(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{
	ProductComponent = CreateDefaultSubobject<UProductComponent>(TEXT("ProductComponent"));
}

void ASFProductionBuilding::OnTierActivated()
{
	Super::OnTierActivated();

	const USFProductionBuildingDefinition* Definition =	Cast<USFProductionBuildingDefinition>(GetBuildingDefinition());
	if (!IsValid(Definition))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[%s] ProductionBuildingDefinition이 아닙니다. Definition=%s"),
			*GetName(),
			*GetNameSafe(GetBuildingDefinition()));

		return;
	}

	const FSFProductionBuildingTierData* ActivatedTier = Definition->FindProductionTier(GetCurrentTier());
	if (!ActivatedTier)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[%s] Production Tier가 없습니다. Tier=%d"),
			*GetName(),
			GetCurrentTier());

		return;
	}

	StoredSpawnableUnitDatas = ActivatedTier->SpawnableUnits;
	OnSpawnableUnitsChanged.Broadcast(StoredSpawnableUnitDatas);

	ProductComponent->ResumeProductLine();
}

void ASFProductionBuilding::OnUpgradeStarted()
{
	Super::OnUpgradeStarted();
	ProductComponent->PauseProductLine();
}

void ASFProductionBuilding::OnBuildingDestroyed()
{ // deactivated -> destroy
	Super::OnBuildingDestroyed();
	// 생산열에 있는 모든 product을 지우기만 한다
	// 환불이 필요하면 로직을 여기 작성해야하나
	// 환불이 되지않는것이 자연스럽다
	ProductComponent->AllClearProductLine();

}

void ASFProductionBuilding::ProductUnits_Implementation(FName InUnitName)
{
	if (!USFTutorialSubsystem::IsUnitProductionAllowed(this, this, InUnitName))
	{
		UE_LOG(LogTemp, Log, TEXT("[%s] 현재 튜토리얼 임무에서는 광전사만 생산할 수 있습니다."), *GetName());
		return;
	}

	TryAddUnitProductLine(InUnitName);
}

FProductableData ASFProductionBuilding::GetProductableData_Implementation()
{
	FProductableData ProductData;
	ProductData.ProductableUnitFNames = StoredSpawnableUnitDatas;
	ProductData.CurrentProductingFNames = ProductComponent->GetProductLine();
	ProductData.ProductComponent = ProductComponent;
	ProductData.UpgradeCost = GetUpgradeCost();
	return ProductData;
}

void ASFProductionBuilding::TryAddUnitProductLine(FName InUnitName)
{
	if (CurrentBuildingState != EBuildingState::Completed)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] 완성되지 않은 건물은 유닛을 생산할 수 없습니다."), *GetName());
		return;
	}

	ProductComponent->AddProductInLine(InUnitName);
}

FSFSelectableInfo ASFProductionBuilding::GetSelectableInfo_Implementation()
{
	FSFSelectableInfo Info = Super::GetSelectableInfo_Implementation();

	Info.SelectableType = ESFSelectableType::ProductionBuilding;
	return Info;
}

void ASFProductionBuilding::SetAssembleVectorPoint(const FVector& InAssemblePoint)
{
	ProductComponent->SetAssemblePoint(InAssemblePoint);
}

void ASFProductionBuilding::ApplySpeedBuff(float InMultiflyValue) const
{
	ProductComponent->SetSpeedMultiplier(InMultiflyValue);
}

void ASFProductionBuilding::ResetSpeedMultiplier()
{
	ProductComponent->ResetSpeedMultiplier();
}

void ASFProductionBuilding::BeginPlay()
{
	Super::BeginPlay();
	
	// 부모 BeginPlay는 로딩 실패 시 Destroy()를 호출한다.
	if (IsActorBeingDestroyed())	return;
	if (!IsValid(ProductComponent))	return;

	// 진행도는 건물을 거치지 않고 상태 컴포넌트가 직접 받는다.
	ProductComponent->OnProductRatioChanged.AddUniqueDynamic(
		StatusComponent, &USFOverheadStatusComponent::HandleProgressRatio);
	ProductComponent->OnProductLineChanged.AddUniqueDynamic(this, &ThisClass::HandleProductLineChanged);

	// 현재 대기열 상태로 표시 여부를 한 번 맞춘다.
	HandleProductLineChanged(ProductComponent->GetProductLine());
}

void ASFProductionBuilding::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(ProductComponent))
	{
		ProductComponent->OnProductRatioChanged.RemoveDynamic(
			StatusComponent, &USFOverheadStatusComponent::HandleProgressRatio);
		ProductComponent->OnProductLineChanged.RemoveDynamic(this, &ThisClass::HandleProductLineChanged);
	}
	
	Super::EndPlay(EndPlayReason);
}

void ASFProductionBuilding::OnBuildingDefinitionLoaded()
{
	Super::OnBuildingDefinitionLoaded();

	const USFProductionBuildingDefinition* Definition =
		Cast<USFProductionBuildingDefinition>(GetBuildingDefinition());

	if (!IsValid(Definition))	return;


	StatusComponent->SetProgressColor(SFOverheadUIColors::Production);
}

bool ASFProductionBuilding::HasRallyPoint() const
{
	return IsValid(ProductComponent) &&
		ProductComponent->HasAssemblePoint();
}

FVector ASFProductionBuilding::GetRallyPoint() const
{
	if (!IsValid(ProductComponent))
	{
		return FVector::ZeroVector;
	}

	return ProductComponent->GetAssemblePoint();
}

void ASFProductionBuilding::HandleProductLineChanged(TArray<FProductInfo> InProductLine)
{
	// 내 건물만 진행도를 보여준다. 적 생산 상황은 노출하지 않는다.
	OverheadUI->SetElementVisible(ESFOverheadElement::Progress,
		!InProductLine.IsEmpty() && IsOwnedByLocalPlayer());
}
