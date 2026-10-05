// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Datas/Resource/SFResourceTypes.h"
#include "Game/Subsystem/BuildingGridInfo.h"
#include "GameFramework/Actor.h"
#include "Interface/SFBeMinedable.h"
#include "InGame/SFSelectINfo/SFSelectable.h"
#include "ResourceBase.generated.h"

class USFBuildingGridSubsystem;
class UBoxComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSFOnResourceNodeDepleted, AResourceBase*, Resource);

UCLASS(Abstract)
class STRIKEFORGE_API AResourceBase 
	: public AActor
	, public ISFSelectable
	, public IBeMinedable
{
	GENERATED_BODY()
	friend class USFBuildingGridSubsystem;

public:
	// Sets default values for this actor's properties
	AResourceBase();
	
	FSFOnResourceNodeDepleted OnDepleted;
	
	virtual int32 Extract(int32 InDigAmount) override;
	int32 ExtractFromCell(int32 ResourceCellIndex, int32 ExtractAmount);
	
	virtual FSFSelectableInfo GetSelectableInfo_Implementation() override;
	virtual FBeMinedableData GetBeMinedableData() const override;
	// Getter
	virtual FORCEINLINE EResourceType GetResourceType() const override { return ResourceType; }
	FORCEINLINE int32 GetResourceDeposit() const { return ResourceDeposit; }
	FORCEINLINE int32 GetResourceTier() const { return ResourceTier; }
	FORCEINLINE float GetResourceRequiredWork() const { return ResourceRequiredWork; }
	FORCEINLINE const FIntPoint& GetResourceGridSize() const { return ResourceGridSize;	}
	int32 GetCellDeposit(int32 ResourceCellIndex) const;
	static int32 CalculateDistributedCellDeposit(
		int32 TotalDeposit,
		const FIntPoint& GridSize,
		int32 ResourceCellIndex);

	FORCEINLINE const FBuildingGridInfo& GetRegisteredResourceGridInfo() const { return RegisteredResourceGridInfo; }
	FORCEINLINE bool IsDepleted() const { return ResourceDeposit <= 0; }

protected: // functions
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

#if WITH_EDITOR
	virtual void OnConstruction(const FTransform& Transform) override;

	virtual void PostEditMove(bool bFinished) override;

	virtual void PostEditUndo() override;

	virtual void Destroyed() override;
	
	void RefreshEditorGridPreview();
	
	bool HasEditorResourceGridConflict(
		const FBuildingGridInfo& ThisGridInfo,
		const USFBuildingGridSubsystem* GridSubsystem) const;
	bool HasEditorSpawnAnchorGridConflict(
	const FBuildingGridInfo& ThisGridInfo,
	const USFBuildingGridSubsystem* GridSubsystem) const;
#endif
	
	void SetBoxExtent();
protected: // properties
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneComponent;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> StaticMeshComponent;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> BoxComponent;
	// 자원 타입
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Resource")
	EResourceType ResourceType = EResourceType::Mineral;
	// 매장량
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resource", meta = (ClampMin = "1", UIMin = "1"))
	int32 ResourceDeposit = 1;
	// 등급 ( 품질 )
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resource", meta = (ClampMin = "1"))
	int32 ResourceTier = 1;
	// 한번캘때 요구 작업량
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resource", meta = (ClampMin = "1"))
	float ResourceRequiredWork = 50.f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Resource|Grid")
	FIntPoint ResourceGridSize = FIntPoint(2, 2);
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Resource|Grid")
	FBuildingGridInfo RegisteredResourceGridInfo;
	bool bResourceGridRegistered  = false;

	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category = "Selection|UI")
	FName SelectionUIDataRowName = NAME_None;
private:
	void InitializeCellDeposits();
	
	UPROPERTY(
	VisibleInstanceOnly,
	BlueprintReadOnly,
	Category = "Resource|Grid",
	meta = (AllowPrivateAccess = true))
	TArray<int32> CellDeposits;
	
#if WITH_EDITORONLY_DATA

	/*
	 * 레벨 디자이너에게 자원이 차지하는
	 * 격자 크기를 보여주는 에디터 전용 컴포넌트입니다.
	 */
	UPROPERTY(
		VisibleAnywhere,
		BlueprintReadOnly,
		Category = "Resource|Grid",
		meta = (AllowPrivateAccess = true))
	TObjectPtr<UBoxComponent> GridPreviewComponent;

#endif
};
