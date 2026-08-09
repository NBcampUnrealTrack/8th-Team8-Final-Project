#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "SFSelectableInfo.h"
#include "SFSelectable.generated.h"

class UProductComponent;
class UProgressComponent;
class UHealthComponent;

UINTERFACE(BlueprintType)
class STRIKEFORGE_API USFSelectable : public UInterface
{
    GENERATED_BODY()
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
};