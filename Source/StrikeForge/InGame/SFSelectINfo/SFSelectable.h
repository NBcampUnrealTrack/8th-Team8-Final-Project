#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "SFSelectableInfo.h"
#include "Datas/Resource/SFResourceTypes.h"
#include "SFSelectable.generated.h"

//class UHealthComponent;
class USoundBase;

UINTERFACE(BlueprintType)
class STRIKEFORGE_API USFSelectable : public UInterface
{
    GENERATED_BODY()
};

// USTRUCT(BlueprintType)
// struct FHealthData
// {
//     GENERATED_BODY()
//     
//     UPROPERTY()
//     FText DisplayName;
//     UPROPERTY()
//     int32 CurrentTier = 0;
//     UPROPERTY()
//     FResources ConstructCost;
//     UPROPERTY()
//     FResources UpgradeCost;
//     UPROPERTY()
//     TWeakObjectPtr<UHealthComponent> HealthComponent;
// };

class STRIKEFORGE_API ISFSelectable

{
    GENERATED_BODY()

public:
    UFUNCTION(
        BlueprintNativeEvent,
        BlueprintCallable,
        Category = "SF|Selection")
    FSFSelectableInfo GetSelectableInfo();
    
    // UFUNCTION(
    // BlueprintNativeEvent,
    // BlueprintCallable,
    // Category = "SF|Selection")
    // const FHealthData GetHealthData();

    UFUNCTION(
        BlueprintNativeEvent,
        BlueprintCallable,
        Category = "SF|Selection|Building")
    void RequestTierUpgrade();
    
    // 선택 사운드 getter
    UFUNCTION(
        BlueprintNativeEvent,
        BlueprintCallable,
        Category = "SF|Selection|Audio")
    USoundBase* GetSelectionSound();
};