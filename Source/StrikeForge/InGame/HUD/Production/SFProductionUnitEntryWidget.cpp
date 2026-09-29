#include "SFProductionUnitEntryWidget.h"

#include "SFProductionUIDataRow.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/CanvasPanelSlot.h"
#include "Engine/Texture2D.h"
#include "SFProductionButtonPaint.h"

#include "Datas/Unit/UnitStatData.h"

#include "Game/Subsystem/SFDataTableManager.h"
#include "Game/GameInstance/SFGameInstance.h"
#include "Logging/SFLog.h"

void USFProductionUnitEntryWidget::NativeConstruct()
{
    Super::NativeConstruct();
    ArrangeButton();

    if (IsValid(ProductionButton))
    {
        ProductionButton->OnClicked.RemoveDynamic(
            this,
            &USFProductionUnitEntryWidget::HandleProductionButtonClicked
        );

        ProductionButton->OnClicked.AddUniqueDynamic(
            this,
            &USFProductionUnitEntryWidget::HandleProductionButtonClicked
        );
    }
}

void USFProductionUnitEntryWidget::InitializeProductionUnit(
    FName InUnitRowName
)
{
    UnitRowName = InUnitRowName;
    HoverAmount = PressAmount = FeedbackRemaining = 0.f;

    if (UnitRowName.IsNone())
    {
        return;
    }

    UGameInstance* GameInstance =
        GetGameInstance();

    if (!IsValid(GameInstance))
    {
        return;
    }

    USFDataTableManager* DataTableManager =
        GameInstance->GetSubsystem<USFDataTableManager>();

    if (!IsValid(DataTableManager))
    {
        SF_LOG(
            Error,
            "[SFProductionUnitEntryWidget] "
            "DataTableManager is invalid."
        );

        return;
    }

    const FSFProductionUIDataRow* UIData =
        DataTableManager->GetDataRow<FSFProductionUIDataRow>(
            EDataTableType::ProductionUI,
            UnitRowName,
            TEXT("InitializeProductionUnit")
        );

    if (!UIData)
    {
        SF_LOG(
            Warning,
            "[SFProductionUnitEntryWidget] "
            "Production UI Row was not found: %s",
            *UnitRowName.ToString()
        );

        return;
    }

    const FUnitStatData* UnitStats =
        DataTableManager->GetDataRow<FUnitStatData>(
            EDataTableType::Unit,
            UnitRowName,
            TEXT("InitializeProductionUnit - Unit")
        );

    if (!UnitStats)
    {
        SF_LOG(
            Warning,
            "[SFProductionUnitEntryWidget] "
            "Unit Row was not found: %s",
            *UnitRowName.ToString()
        );
    }

    if (IsValid(UnitNameText))
    {
        UnitNameText->SetText(
            UIData->DisplayName
        );
    }

    if (IsValid(UnitIconImage))
    {
        UTexture2D* IconTexture =
            UIData->Icon.LoadSynchronous();

        if (IsValid(IconTexture))
        {
            UnitIconImage->SetBrushFromTexture(
                IconTexture
            );
        }
    }

    if (IsValid(UnitNeedGold))
    {
        const int32 MineralCost =
            UnitStats
            ? UnitStats->UnitProductionCost.Mineral
            : 0;

        UnitNeedGold->SetText(
            FText::AsNumber(MineralCost)
        );
    }

    if (IsValid(UnitNeedEther))
    {
        const int32 EtherCost =
            UnitStats
            ? UnitStats->UnitProductionCost.Ether
            : 0;

        UnitNeedEther->SetText(
            FText::AsNumber(EtherCost)
        );
    }
    ArrangeButton();
    SF_LOG(
        Log,
        "[SFProductionUnitEntryWidget] "
        "Initialized | Row: %s | Name: %s",
        *UnitRowName.ToString(),
        *UIData->DisplayName.ToString()
    );
}

void USFProductionUnitEntryWidget::HandleProductionButtonClicked()
{
    if (UnitRowName.IsNone())
    {
        return;
    }

    SF_LOG(
        Log,
        "[SFProductionUnitEntryWidget] "
        "Clicked | Unit: %s",
        *UnitRowName.ToString()
    );

    OnProductionUnitClicked.Broadcast(
        UnitRowName
    );
}
void USFProductionUnitEntryWidget::ArrangeButton()
{
    // All tiers use the same 280 x 70 card, so three entries fit the existing 230px column.
    auto Place = [](UWidget* Widget, FVector2D Position, FVector2D Size)
    {
        if (!Widget) return;
        if (auto* Slot = Cast<UCanvasPanelSlot>(Widget->Slot))
        {
            Slot->SetAutoSize(false);
            Slot->SetPosition(Position);
            Slot->SetSize(Size);
        }
        Widget->SetVisibility(ESlateVisibility::HitTestInvisible);
    };
    Place(ProductionButton, FVector2D::ZeroVector, FVector2D(280,70));
    if (ProductionButton)
    {
        ProductionButton->SetVisibility(ESlateVisibility::Visible);
        ProductionButton->SetToolTipText(NSLOCTEXT("SFProduction", "ProduceHint", "클릭하여 생산 대기열에 추가"));
    }
    Place(GetWidgetFromName(TEXT("ButtonBackground")), FVector2D(5,5), FVector2D(270,60));
    Place(GetWidgetFromName(TEXT("HoverOverlay")), FVector2D::ZeroVector, FVector2D(280,70));
    FVector2D IconSize(62,62);
    if (UnitIconImage)
        if (const auto* Texture = Cast<UTexture2D>(UnitIconImage->GetBrush().GetResourceObject()))
        {
            const float MaxSide = FMath::Max(Texture->GetSizeX(),Texture->GetSizeY());
            if (MaxSide > 0) IconSize = FVector2D(Texture->GetSizeX(),Texture->GetSizeY()) * (62.f / MaxSide);
        }
    Place(UnitIconImage, FVector2D(48,35) - IconSize * .5f, IconSize);
    if (UnitIconImage) UnitIconImage->SetRenderTransformPivot(FVector2D(.5f,.5f));
    Place(UnitNameText, FVector2D(96,8), FVector2D(145,27));
    Place(UnitNeedGold, FVector2D(110,37), FVector2D(65,24));
    Place(UnitNeedEther, FVector2D(193,37), FVector2D(60,24));
    if (UnitNameText) UnitNameText->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
}

void USFProductionUnitEntryWidget::ShowRequestFeedback(bool bAccepted)
{
    bLastAccepted = bAccepted;
    FeedbackRemaining = .38f;
}

void USFProductionUnitEntryWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
    Super::NativeTick(Geometry, DeltaTime);
    bool bHover = ProductionButton && ProductionButton->IsHovered();
#if WITH_DEV_AUTOMATION_TESTS
    bHover |= bPreviewHover;
#endif
    HoverAmount = FMath::FInterpTo(HoverAmount, bHover ? 1.f : 0.f, DeltaTime, 15.f);
    PressAmount = FMath::FInterpTo(PressAmount, ProductionButton && ProductionButton->IsPressed() ? 1.f : 0.f, DeltaTime, 25.f);
    FeedbackRemaining = FMath::Max(0.f, FeedbackRemaining - DeltaTime);
    ShineTime = bHover ? ShineTime + DeltaTime : 0.f;
    if (UnitIconImage) UnitIconImage->SetRenderScale(FVector2D(1.f + .055f * HoverAmount - .06f * PressAmount));
    // The Blueprint used to fade this frame on hover; keep the authored frame opaque.
    if (auto* Frame = Cast<UImage>(GetWidgetFromName(TEXT("HoverOverlay")))) Frame->SetOpacity(1.f);
    if (auto* Background = Cast<UImage>(GetWidgetFromName(TEXT("ButtonBackground")))) Background->SetOpacity(1.f);
}

int32 USFProductionUnitEntryWidget::NativePaint(const FPaintArgs& Args, const FGeometry& G,
    const FSlateRect& Cull, FSlateWindowElementList& Out, int32 Layer,
    const FWidgetStyle& Style, bool bParentEnabled) const
{
    using namespace SFProductionPaint;
    Layer = Super::NativePaint(Args,G,Cull,Out,Layer,Style,bParentEnabled) + 1;
    const float Alpha = Style.GetColorAndOpacityTint().A * GetRenderOpacity();
    const float Flash = FeedbackRemaining / .38f;
    FLinearColor Accent = bLastAccepted ? Gold : Cancel;
    Accent = FMath::Lerp(Ether, Accent, Flash); Accent.A = Alpha * FMath::Max(HoverAmount, Flash);
    Rim(Out,Layer,G,FVector2D(7,6),FVector2D(266,58),Accent,1.5f + HoverAmount);
    Box(Out,Layer,G,FVector2D(87,14),FVector2D(1,42),FLinearColor(.8f,.62f,.35f,.24f*Alpha));
    // Tiny resource diamonds leave the existing cost text and its bindings intact.
    for (int32 I=0;I<2;++I)
    {
        const FVector2D P(I ? 184 : 101,48);
        FLinearColor C=I ? Ether : Gold; C.A=Alpha;
        Line(Out,Layer,G,{P+FVector2D(0,-4),P+FVector2D(3,0),P+FVector2D(0,4),P-FVector2D(3,0),P-FVector2D(0,4)},C,1.5f);
    }
    FLinearColor Plus=Ether; Plus.A=Alpha*(.32f+.68f*HoverAmount);
    const FVector2D P(260,24 + 2*PressAmount);
    Line(Out,Layer,G,{P-FVector2D(4,0),P+FVector2D(4,0)},Plus,2.f);
    Line(Out,Layer,G,{P-FVector2D(0,4),P+FVector2D(0,4)},Plus,2.f);
    if (HoverAmount > .01f)
    {
        Box(Out,Layer,G,FVector2D(96,64),FVector2D(155*HoverAmount,2),Accent);
        const float X=18+FMath::Frac(ShineTime*.8f)*244;
        FLinearColor Glint=Gold; Glint.A=Alpha*HoverAmount*.8f;
        Line(Out,Layer,G,{FVector2D(X-7,6),FVector2D(X+7,6)},Glint,2.f);
    }
    return Layer;
}
