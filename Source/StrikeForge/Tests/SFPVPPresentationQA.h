#pragma once
#include "CoreMinimal.h"
#if WITH_DEV_AUTOMATION_TESTS
class UWorld;
bool SFPresentationQACommand(UWorld* World, const FString& Op, const FString& Value);
void SFWritePresentationQA(UWorld* World, const FString& Instance);
#endif
