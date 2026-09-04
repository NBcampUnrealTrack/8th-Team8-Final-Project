// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Datas/Building/SFBuildingType.h"
#include "SFBuildingSpawnAnchor.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class USFBuildingDefinition;

UCLASS()
class STRIKEFORGE_API ASFBuildingSpawnAnchor : public AActor
{
	GENERATED_BODY()
public:
	
	ASFBuildingSpawnAnchor();
	
	int32 GetTeamIndex() const { return TeamIndex; }
	ESFBuildingType GetBuildingType() const { return BuildingType; }
	
protected:
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category = "SF|Spawn",meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> SceneComponent;
private:
	UPROPERTY(EditInstanceOnly,BlueprintReadOnly,Category = "SF|Spawn",	meta = (AllowPrivateAccess = "true"))
	int32 TeamIndex = 0;

	/**
	 * 종족과 무관한 건물의 역할.
	 *
	 * 예: Human,Animal > Nexus,  Human,Animal > House, MineralMine, Barracks
	 */
	UPROPERTY(EditInstanceOnly,BlueprintReadOnly,Category = "SF|Spawn",meta = (AllowPrivateAccess = "true"))
	ESFBuildingType BuildingType = ESFBuildingType::None;

#if WITH_EDITORONLY_DATA

	/**
	 * 에디터 시각화 전용.
	 * 런타임 건물 선택에는 사용하지 않는다.
	 */
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category = "SF|Spawn",meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> PreviewMeshComponent;
	UPROPERTY(EditInstanceOnly,BlueprintReadOnly,Category = "SF|Spawn|Preview",meta = (AllowPrivateAccess = "true"))
	TSoftObjectPtr<USFBuildingDefinition> PreviewDefinition;

#endif
#if WITH_EDITOR
public:
	virtual void OnConstruction(const FTransform& Transform) override;
private:
	void RefreshEditorPreview();
#endif
};
