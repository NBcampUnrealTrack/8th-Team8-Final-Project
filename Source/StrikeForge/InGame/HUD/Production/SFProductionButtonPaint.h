#pragma once

#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

// Lightweight Slate accents share the panel's existing wood / ivory / ether palette.
namespace SFProductionPaint
{
inline const FLinearColor Gold(0.94f, 0.68f, 0.30f, 1.f);
inline const FLinearColor Ether(0.15f, 0.95f, 0.88f, 1.f);
inline const FLinearColor Cancel(1.f, 0.28f, 0.18f, 1.f);
inline void Box(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G,
    FVector2D P, FVector2D S, FLinearColor C)
{
    FSlateDrawElement::MakeBox(Out, Layer, G.ToPaintGeometry(S, FSlateLayoutTransform(P)),
        FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None, C);
}
inline void Line(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G,
    TArray<FVector2D> Points, FLinearColor C, float Width = 1.f)
{
    FSlateDrawElement::MakeLines(Out, Layer, G.ToPaintGeometry(), Points, ESlateDrawEffect::None, C, true, Width);
}
inline void Rim(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G,
    FVector2D P, FVector2D S, FLinearColor C, float Width = 1.f)
{
    const float B = 6.f;
    Line(Out,Layer,G,{P+FVector2D(B,0),P+FVector2D(S.X-B,0),P+FVector2D(S.X,B),
        P+S-FVector2D(0,B),P+S-FVector2D(B,0),P+FVector2D(B,S.Y),
        P+FVector2D(0,S.Y-B),P+FVector2D(0,B),P+FVector2D(B,0)},C,Width);
}
}
