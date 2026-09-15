#include "UI/MouseSensitivitySubsystem.h"

void UMouseSensitivitySubsystem::SetMouseSensitivity(float NewSensitivity)
{
	MouseSensitivity = FMath::Clamp(NewSensitivity, MinSensitivity, MaxSensitivity);
}