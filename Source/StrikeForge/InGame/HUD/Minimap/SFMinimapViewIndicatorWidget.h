#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SFMinimapViewIndicatorWidget.generated.h"

UCLASS()
class STRIKEFORGE_API USFMinimapViewIndicatorWidget
    : public UUserWidget
{
    GENERATED_BODY()

public:
    void SetViewCorners(
        const FVector2D& InTopLeft,
        const FVector2D& InTopRight,
        const FVector2D& InBottomRight,
        const FVector2D& InBottomLeft
    );

protected:
    virtual int32 NativePaint(
        const FPaintArgs& Args,
        const FGeometry& AllottedGeometry,
        const FSlateRect& MyCullingRect,
        FSlateWindowElementList& OutDrawElements,
        int32 LayerId,
        const FWidgetStyle& InWidgetStyle,
        bool bParentEnabled
    ) const override;

private:
    FVector2D TopLeft = FVector2D::ZeroVector;
    FVector2D TopRight = FVector2D::ZeroVector;
    FVector2D BottomRight = FVector2D::ZeroVector;
    FVector2D BottomLeft = FVector2D::ZeroVector;

    bool bHasValidCorners = false;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "SF|Minimap|View Indicator",
        meta = (AllowPrivateAccess = "true")
    )
    FLinearColor FillColor =
        FLinearColor(
            0.0f,
            1.0f,
            1.0f,
            0.3f
        );

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "SF|Minimap|View Indicator",
        meta = (AllowPrivateAccess = "true")
    )
    FLinearColor BorderColor =
        FLinearColor(
            0.0f,
            1.0f,
            1.0f,
            1.0f
        );

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "SF|Minimap|View Indicator",
        meta = (
            ClampMin = "1.0",
            AllowPrivateAccess = "true"
            )
    )
    float LineThickness = 2.0f;
};