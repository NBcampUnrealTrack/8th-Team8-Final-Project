#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Datas/Resource/SFResourceTypes.h"
#include "Datas/Race/SFRaceType.h"
#include "SFTeamState.generated.h"

class ASFInGamePlayerController;
class ASFNexusBuilding;
class USFBuildingCatalog;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FSFOnMineralChanged, int32, NewMineral, float, NewIncomeRate);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FSFOnEtherChanged, int32, NewEther, float, NewIncomeRate);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FSFOnPopulationChanged, int32, NewCurrent, int32, NewMaximum);

UCLASS(BlueprintType)
class STRIKEFORGE_API USFTeamState : public UObject
{
	GENERATED_BODY()
	
	public:
	UPROPERTY(BlueprintAssignable, Category = "SF|Resource")
	FSFOnMineralChanged OnMineralChanged;
	UPROPERTY(BlueprintAssignable, Category = "SF|Resource")
	FSFOnEtherChanged OnEtherChanged;
	UPROPERTY(BlueprintAssignable, Category = "SF|Population")
	FSFOnPopulationChanged OnPopulationChanged;

	// 조회
	UFUNCTION(BlueprintPure) FORCEINLINE ESFRace GetRace() const { return Race; }
	UFUNCTION(BlueprintPure) FORCEINLINE int32 GetMineral() const { return Mineral; }
	UFUNCTION(BlueprintPure) FORCEINLINE int32 GetEther() const { return Ether; }
	UFUNCTION(BlueprintPure) FORCEINLINE float GetMineralIncomeRate() const { return MineralIncomeRate; }
	UFUNCTION(BlueprintPure) FORCEINLINE float GetEtherIncomeRate() const { return EtherIncomeRate; }
	UFUNCTION(BlueprintPure) FORCEINLINE int32 GetCurrentPopulation() const { return CurrentPopulation; }
	UFUNCTION(BlueprintPure) FORCEINLINE int32 GetMaximumPopulation() const { return MaximumPopulation; }
	UFUNCTION(BlueprintPure) ASFNexusBuilding* GetNexus() const;
	UFUNCTION(BlueprintPure) USFBuildingCatalog* GetBuildingCatalog() const;
	UFUNCTION(BlueprintPure) ASFInGamePlayerController* GetPlayer() const;
	
	// 변경
	void AddResource(EResourceType InType, int32 InAmount);
	void AddResourceIncomeRate(EResourceType InType, float InRate);
	void AddPopulation(int32 InDelta);
	void AddMaxPopulation(int32 InDelta);
	bool InitializeTeamData(ESFRace InRace, USFBuildingCatalog* InBuildingCatalog);

	// 판정 · 소비
	bool CanAffordResource(const FResources& InCost) const;
	bool CanAffordPopulation(int32 InPopulationCost) const;
	bool TrySpendCost(const FResources& InCost);
	void RefundCost(const FResources& InCost);

	void SetNexus(ASFNexusBuilding* InNexus);
	bool IsNexus(const ASFNexusBuilding* InNexus) const;

	void SetPlayer(ASFInGamePlayerController* InPlayer);
    void ApplyNetworkValues(int32 InMineral,int32 InEther,float InMineralRate,float InEtherRate,int32 InPopulation,int32 InCapacity);
private:
	void SetMineral(int32 NewMineral);
	void SetEther(int32 NewEther);
	void SetMineralIncomeRate(float NewRate);
	void SetEtherIncomeRate(float NewRate);

	UPROPERTY(VisibleAnywhere) int32 Mineral = 0;
	UPROPERTY(VisibleAnywhere) int32 Ether = 0;
	UPROPERTY(VisibleAnywhere) float MineralIncomeRate = 0.f;
	UPROPERTY(VisibleAnywhere) float EtherIncomeRate = 0.f;
	UPROPERTY(VisibleAnywhere) int32 CurrentPopulation = 0;
	UPROPERTY(VisibleAnywhere) int32 MaximumPopulation = 0;
	UPROPERTY(VisibleAnywhere) TWeakObjectPtr<ASFNexusBuilding> Nexus;
	UPROPERTY(VisibleAnywhere) TWeakObjectPtr<ASFInGamePlayerController> Player;
	
	UPROPERTY(
	VisibleAnywhere,
	BlueprintReadOnly,
	Category = "SF|Team",
	meta = (AllowPrivateAccess = "true"))
	ESFRace Race = ESFRace::None;
	/**
 * 해당 팀이 사용하는 종족별 건물 카탈로그.
 *
 * 예:
 * Human 팀 → DA_HumanBuildingCatalog
 * Animal 팀 → DA_AnimalBuildingCatalog
 */
	UPROPERTY(
		VisibleAnywhere,
		BlueprintReadOnly,
		Category = "SF|Building",
		meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USFBuildingCatalog> BuildingCatalog;
};
