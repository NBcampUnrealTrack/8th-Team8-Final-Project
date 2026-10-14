#include "SFProgressTracker.h"

void FSFProgressTracker::Reset(float InRequiredWork, float InBaseWorkAmount)
{
	CurrentWork = 0.f;
	RequiredWork = InRequiredWork;
	BaseWorkAmount = InBaseWorkAmount;
}

bool FSFProgressTracker::Advance(float DeltaTime)
{
	CurrentWork += BaseWorkAmount * WorkRateMultiplier * DeltaTime;
	return CurrentWork >= RequiredWork;
}

float FSFProgressTracker::GetRatio() const
{
	const float Ratio = FMath::IsNearlyZero(RequiredWork)
		? 0.f
		: CurrentWork / RequiredWork;
	return FMath::Clamp(Ratio, 0.f, 1.f);
}

bool FSFProgressTracker::WouldChangeWorkRateModifier(
	FName ModifierId,
	float InMultiplier) const
{
	if (ModifierId.IsNone())
	{
		return false;
	}

	const float NewMultiplier = FMath::Max(0.f, InMultiplier);
	const float* ExistingMultiplier = WorkRateModifiers.Find(ModifierId);
	return !ExistingMultiplier ||
		!FMath::IsNearlyEqual(*ExistingMultiplier, NewMultiplier);
}

void FSFProgressTracker::SetWorkRateModifier(
	FName ModifierId,
	float InMultiplier)
{
	if (!WouldChangeWorkRateModifier(ModifierId, InMultiplier))
	{
		return;
	}

	WorkRateModifiers.Add(ModifierId, FMath::Max(0.f, InMultiplier));
	RecalculateWorkRateMultiplier();
}

bool FSFProgressTracker::RemoveWorkRateModifier(FName ModifierId)
{
	if (ModifierId.IsNone() || WorkRateModifiers.Remove(ModifierId) == 0)
	{
		return false;
	}

	RecalculateWorkRateMultiplier();
	return true;
}

void FSFProgressTracker::RecalculateWorkRateMultiplier()
{
	WorkRateMultiplier = 1.f;
	for (const TPair<FName, float>& Modifier : WorkRateModifiers)
	{
		WorkRateMultiplier *= Modifier.Value;
	}
}
