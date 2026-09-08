#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Buildings/Building/SFBaseBuilding.h"
#include "SFBuildingSpawnLibrary.generated.h"

class USFBuildingDefinition;

/**
 * BuildingDefinition을 이용한 건물 생성 공통 로직.
 *
 * PlayerController의 건설 배치와
 * GameMode의 시작 건물 생성이 같은 초기화 경로를 사용한다.
 */
UCLASS()
class STRIKEFORGE_API USFBuildingSpawnLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/**
	 * BeginPlay 전에 Definition, TeamIndex, SpawnMode를 주입하고
	 * 생성이 완료된 건물을 반환한다.
	 */
	UFUNCTION(BlueprintCallable, Category = "SF|Building|Spawn", meta = (WorldContext = "WorldContextObject"))
	static ASFBaseBuilding* SpawnBuildingFromDefinition(
		UObject* WorldContextObject,
		USFBuildingDefinition* InDefinition,
		int32 InTeamIndex,
		const FTransform& InSpawnTransform,
		ESFBuildingSpawnMode InSpawnMode,
		int32 InInitialTierIndex = 0);
};
