#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SFPVPEditorLibrary.generated.h"
UCLASS()
class STRIKEFORGE_API USFPVPEditorLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,Category="SF|PVP|Editor") static int32 PrepareBattlefield(UWorld* World);
};
