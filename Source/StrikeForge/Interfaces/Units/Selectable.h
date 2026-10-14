#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Selectable.generated.h"

UINTERFACE()
class USelectable : public UInterface
{
	GENERATED_BODY()
};

class STRIKEFORGE_API ISelectable
{
	GENERATED_BODY()
public:
	
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	FName OnSelected();
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void OnDeSelected();
	
};
