#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SFVoiceAssetLibrary.generated.h"

class USoundCue;
class USoundWave;

/** Editor import support for response cues with visible, editable random graphs. */
UCLASS()
class STRIKEFORGE_API USFVoiceAssetLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category = "SF|Audio|Editor", meta = (DevelopmentOnly))
    static bool ConfigureResponseCue(USoundCue* Cue, const TArray<USoundWave*>& Waves);
};
