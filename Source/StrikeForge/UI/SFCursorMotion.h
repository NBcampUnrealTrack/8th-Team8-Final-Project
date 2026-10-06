#pragma once

#include "CoreMinimal.h"

// Both samples and applied positions are desktop pixels, never viewport pixels.
struct FSFCursorMotion
{
	FVector2D LastPosition = FVector2D::ZeroVector;
	FVector2D Remainder = FVector2D::ZeroVector;
	bool bHasPosition = false;

	void Reset() { bHasPosition = false; Remainder = FVector2D::ZeroVector; }
	void RecordActualPosition(FVector2D Position) { LastPosition = Position; bHasPosition = true; }

	FVector2D Adjust(FVector2D Position, float Sensitivity)
	{
		if (!bHasPosition)
		{
			RecordActualPosition(Position);
			return Position;
		}
		const FVector2D Delta = Position - LastPosition;
		// Residual fractions or synthetic viewport events must never move an idle cursor.
		if (Delta.IsZero() || FMath::IsNearlyEqual(Sensitivity, 1.f)) return Position;
		const FVector2D Desired = LastPosition + Delta * Sensitivity + Remainder;
		const FVector2D Rounded(FMath::RoundToInt(Desired.X), FMath::RoundToInt(Desired.Y));
		Remainder = Desired - Rounded;
		return Rounded;
	}
};
