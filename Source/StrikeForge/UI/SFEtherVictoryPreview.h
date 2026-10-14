#pragma once
#include "CoreMinimal.h"
struct FGeometry;
class FSlateWindowElementList;
class UFont;

// Image-free victory renderer, driven by real time while the world is paused.
int32 PaintEtherVictoryPreview(const FGeometry& Geometry, FSlateWindowElementList& Out,
    int32 Layer, float Age, UFont* Font, const FLinearColor& Tint, bool bVictory = true);
