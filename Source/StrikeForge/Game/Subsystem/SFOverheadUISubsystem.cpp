#include "SFOverheadUISubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

USFOverheadUISubsystem* USFOverheadUISubsystem::Get(const UObject* WorldContext)
{
	if (!WorldContext) return nullptr;

	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return IsValid(World) ? World->GetSubsystem<USFOverheadUISubsystem>() : nullptr;
}

void USFOverheadUISubsystem::SetUIScale(float InScale)
{
	const float NewScale = FMath::Max(InScale, KINDA_SMALL_NUMBER);

	if (FMath::IsNearlyEqual(CurrentUIScale, NewScale))	return;

	CurrentUIScale = NewScale;

	OnUIScaleChanged.Broadcast();
}
