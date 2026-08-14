#include "BuildingHealthBar.h"
#include "Components/ProgressBar.h"
#include "Kismet/KismetMathLibrary.h"

void UBuildingHealthBar::SetHealthBar(float InBeforeHP, float InCurrentHP, float InMaxHP)
{
	const float Ratio = FMath::Clamp(UKismetMathLibrary::SafeDivide(InCurrentHP, InMaxHP), 0.0f, 1.0f);
	
	HealthBar->SetPercent(Ratio);
}
