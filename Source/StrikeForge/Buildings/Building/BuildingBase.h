#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BuildingBase.generated.h"

class UProgressComponent;
class UHealthComponent;

UENUM(BlueprintType)
enum class EBuildingState : uint8
{
	SettingMode,
	BuildingMode
};

UCLASS()
class STRIKEFORGE_API ABuildingBase : public AActor
{
	GENERATED_BODY()

public:
	ABuildingBase();
	
	void ApplyPreviewMesh();
	
	void GetTierMeshList(int32 InCurrentTier);
	
	void SetMesh(UStaticMesh* InStaticMesh);
protected:
	virtual void BeginPlay() override;
	
	UPROPERTY()
	UStaticMeshComponent* StaticMesh;
	UPROPERTY()
	UHealthComponent* HealthComponent;
	UPROPERTY()
	UProgressComponent* ProgressComponent;
private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Information", meta=(AllowPrivateAccess=true))
	FName BuildingName;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Information", meta=(AllowPrivateAccess=true))
	int32 TeamIndex;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Information", meta=(AllowPrivateAccess=true))
	int32 CurrentTier = 0;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State", meta=(AllowPrivateAccess=true))
	EBuildingState CurrentMode = EBuildingState::SettingMode;
};
