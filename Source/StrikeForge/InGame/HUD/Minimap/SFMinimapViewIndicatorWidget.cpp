#include "SFMinimapViewIndicatorWidget.h"

#include "Rendering/DrawElementTypes.h"

void USFMinimapViewIndicatorWidget::SetViewCorners(
    const FVector2D& InTopLeft,
    const FVector2D& InTopRight,
    const FVector2D& InBottomRight,
    const FVector2D& InBottomLeft
)
{
    TopLeft = InTopLeft;
    TopRight = InTopRight;
    BottomRight = InBottomRight;
    BottomLeft = InBottomLeft;

    bHasValidCorners = true;

    Invalidate(
        EInvalidateWidgetReason::Paint
    );
}

int32 USFMinimapViewIndicatorWidget::NativePaint(
    const FPaintArgs& Args,
    const FGeometry& AllottedGeometry,
    const FSlateRect& MyCullingRect,
    FSlateWindowElementList& OutDrawElements,
    int32 LayerId,
    const FWidgetStyle& InWidgetStyle,
    bool bParentEnabled
) const
{
    const int32 ResultLayerId =
        Super::NativePaint(
            Args,
            AllottedGeometry,
            MyCullingRect,
            OutDrawElements,
            LayerId,
            InWidgetStyle,
            bParentEnabled
        );

    if (!bHasValidCorners)
    {
        return ResultLayerId;
    }

    TArray<FSlateVertex> Vertices;

    Vertices.Reserve(4);

    const FColor SlateFillColor =
        FillColor.ToFColor(true);

    auto AddVertex =
        [
            &Vertices,
            &SlateFillColor,
            &AllottedGeometry
        ](
            const FVector2D& LocalPoint
            )
        {
            /*
             * LocalPoint는 미니맵 내부 로컬 좌표다.
             * CustomVerts는 화면 절대 좌표를 사용하므로
             * 반드시 절대 좌표로 변환한다.
             */
            const FVector2D AbsolutePoint =
                AllottedGeometry.LocalToAbsolute(
                    LocalPoint
                );

            FSlateVertex Vertex;

            Vertex.Position =
                FVector2f(
                    static_cast<float>(AbsolutePoint.X),
                    static_cast<float>(AbsolutePoint.Y)
                );

            Vertex.Color =
                SlateFillColor;

            Vertices.Add(Vertex);
        };

    AddVertex(TopLeft);
    AddVertex(TopRight);
    AddVertex(BottomRight);
    AddVertex(BottomLeft);

    TArray<SlateIndex> Indices =
    {
        0, 1, 2,
        0, 2, 3
    };

    FSlateDrawElement::MakeCustomVerts(
        OutDrawElements,
        ResultLayerId + 1,
        FSlateResourceHandle(),
        Vertices,
        Indices,
        nullptr,
        0,
        0,
        ESlateDrawEffect::None
    );

    TArray<FVector2D> BorderPoints;

    BorderPoints.Add(TopLeft);
    BorderPoints.Add(TopRight);
    BorderPoints.Add(BottomRight);
    BorderPoints.Add(BottomLeft);
    BorderPoints.Add(TopLeft);

    FSlateDrawElement::MakeLines(
        OutDrawElements,
        ResultLayerId + 2,
        AllottedGeometry.ToPaintGeometry(),
        BorderPoints,
        ESlateDrawEffect::None,
        BorderColor,
        true,
        LineThickness
    );

    return ResultLayerId + 2;
}