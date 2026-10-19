#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SFPVPEditorLibrary.generated.h"
UCLASS()
class STRIKEFORGE_API USFPVPEditorLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,Category="SF|Editor") static FString DescribeLevelScript(UWorld* World);
    UFUNCTION(BlueprintCallable,Category="SF|Editor") static bool MigrateTitle(UWorld* World);
    UFUNCTION(BlueprintCallable,Category="SF|PVP|Editor") static int32 PrepareBattlefield(UWorld* World);
};
