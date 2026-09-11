#include "MouseSensitivitySubsystem.h"

void UMouseSensitivitySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	GConfig->GetFloat(TEXT("Input"), TEXT("MouseSensitivity"), CurrentSensitivity, GGameUserSettingsIni);
}

void UMouseSensitivitySubsystem::SetSensitivity(float NewSensitivity)
{
	CurrentSensitivity = NewSensitivity;
	GConfig->SetFloat(TEXT("Input"), TEXT("MouseSensitivity"), NewSensitivity, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}