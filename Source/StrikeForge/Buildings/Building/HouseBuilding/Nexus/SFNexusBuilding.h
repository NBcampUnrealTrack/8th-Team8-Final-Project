// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Buildings/Building/HouseBuilding/SFHouseBuilding.h"
#include "SFNexusBuilding.generated.h"

UCLASS()
class STRIKEFORGE_API ASFNexusBuilding : public ASFHouseBuilding
{
	GENERATED_BODY()

public:
	ASFNexusBuilding(const FObjectInitializer& ObjectInitializer);
	virtual FSFSelectableInfo GetSelectableInfo_Implementation() override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnBuildingDestroyed() override;

	/** 파괴 순간. 연출은 BP 에서 구현한다 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Nexus")
	void OnNexusDestroyed(int32 InDestroyedTeamIndex);

private:
	/**
	 * 끄면 파괴돼도 스테이지를 끝내지 않는다.
	 * 전투를 반복 테스트할 때 매번 게임이 끝나면 번거로우므로 열어둔다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nexus", meta = (AllowPrivateAccess = true))
	uint8 bFinishStageOnDestroyed : 1;
};
