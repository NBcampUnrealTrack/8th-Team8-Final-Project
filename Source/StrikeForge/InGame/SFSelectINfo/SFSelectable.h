#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "SFSelectableInfo.h"
#include "Datas/Resource/ResourceTypes.h"
#include "SFSelectable.generated.h"

class UHealthComponent;

UINTERFACE(BlueprintType)
class STRIKEFORGE_API USFSelectable : public UInterface
{
    GENERATED_BODY()
};

USTRUCT(BlueprintType)
struct FHealthData
{
    GENERATED_BODY()
    
    UPROPERTY()
    FText DisplayName;
    UPROPERTY()
    int32 CurrentTier;
    UPROPERTY()
    FResources UpgradeCost;
    UPROPERTY()
    TWeakObjectPtr<UHealthComponent> HealthComponent;
};

class STRIKEFORGE_API ISFSelectable

{
    GENERATED_BODY()

public:
    UFUNCTION(
        BlueprintNativeEvent,
        BlueprintCallable,
        Category = "SF|Selection"
    )
    FSFSelectableInfo GetSelectableInfo();
    
    UFUNCTION(
    BlueprintNativeEvent,
    BlueprintCallable,
    Category = "SF|Selection")
    const FHealthData GetHealthData();
};