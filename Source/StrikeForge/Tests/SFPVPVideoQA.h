#pragma once

#if WITH_DEV_AUTOMATION_TESTS
#include "CoreMinimal.h"

// Opt-in recording of the actual game viewport and its master audio output.
// This is separate from gameplay and never changes simulation speed or resources.
void SFStartPVPVideoQA(UWorld* World, const FString& Folder, float Seconds);
#endif
