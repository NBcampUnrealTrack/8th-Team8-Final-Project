#include "SFAchievementsWidget.h"
#include "Game/GameInstance/SFGameInstance.h"
#include "Engine/Font.h"
#include "Engine/FontFace.h"
#include "UObject/ConstructorHelpers.h"
#include "InputCoreTypes.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Types/NavigationMetaData.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/Text/STextBlock.h"

namespace SFAchievements
{
    const FLinearColor Gold = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("D9B873")));
    const FLinearColor Cream = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("FFF0CB")));
    const FLinearColor Teal = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("103D3C")));
    const FLinearColor Muted = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("A1B2AA")));
    const FLinearColor Aqua = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("65DFCE")));
}

/** Small vector crests stay sharp at every viewport size, including locked states. */
class SSFAchievementCrest : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SSFAchievementCrest) {} SLATE_END_ARGS()
    void Construct(const FArguments&, int32 InStage, bool bInUnlocked) { Stage = InStage; bUnlocked = bInUnlocked; }
    virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(70, 70); }
    virtual int32 OnPaint(const FPaintArgs&, const FGeometry& G, const FSlateRect&, FSlateWindowElementList& Out,
        int32 Layer, const FWidgetStyle& Style, bool) const override
    {
        using namespace SFAchievements;
        const FLinearColor Color = (bUnlocked ? Gold : Muted.CopyWithNewOpacity(.65f)) * Style.GetColorAndOpacityTint();
        auto Line = [&](TArray<FVector2D> Points, FLinearColor Tint, float Width = 2.f)
        { FSlateDrawElement::MakeLines(Out, Layer, G.ToPaintGeometry(), Points, ESlateDrawEffect::None, Tint, true, Width); };
        Line({{35,3},{64,17},{59,48},{35,67},{11,48},{6,17},{35,3}}, Color);
        Line({{35,8},{58,20},{54,45},{35,60},{16,45},{12,20},{35,8}}, Color.CopyWithNewOpacity(.3f), 1.f);
        const FLinearColor Symbol = bUnlocked ? Aqua : Color;
        switch (Stage)
        {
        case 1: // Banner.
            Line({{25,49},{25,20},{47,20},{41,29},{25,29}}, Symbol, 3.f); break;
        case 2: // Crossed blades.
            Line({{23,46},{46,22},{44,31}}, Symbol, 3.f);
            Line({{47,46},{24,22},{26,31}}, Symbol, 3.f);
            Line({{21,37},{31,47}}, Color); Line({{39,47},{49,37}}, Color); break;
        case 3: // Keep.
            Line({{21,47},{21,24},{28,24},{28,29},{33,29},{33,24},{39,24},{39,29},{44,29},{44,24},{49,24},{49,47},{21,47}}, Symbol);
            Line({{31,47},{31,38},{39,38},{39,47}}, Color); break;
        case 4: // Arrow through a broken ring.
            Line({{26,23},{18,34},{25,47},{42,49},{51,38}}, Color);
            Line({{26,43},{46,23},{34,23}}, Symbol, 3.f); Line({{46,23},{46,35}}, Symbol, 3.f); break;
        case 5: // Hero's star.
            Line({{35,18},{40,29},{52,31},{43,39},{45,51},{35,45},{25,51},{27,39},{18,31},{30,29},{35,18}}, Symbol); break;
        case 6: // Advancing chevrons.
            Line({{23,31},{35,21},{47,31}}, Symbol, 3.f);
            Line({{23,41},{35,31},{47,41}}, Symbol, 3.f);
            Line({{23,51},{35,41},{47,51}}, Color, 3.f); break;
        default: // Final crown and ether gem.
            Line({{20,29},{25,46},{45,46},{50,29},{40,35},{35,23},{30,35},{20,29}}, Color, 3.f);
            Line({{35,32},{39,38},{35,43},{31,38},{35,32}}, Symbol); break;
        }
        return Layer;
    }
private:
    int32 Stage = 1;
    bool bUnlocked = false;
};

USFAchievementsWidget::USFAchievementsWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
    SetIsFocusable(true);
    static ConstructorHelpers::FObjectFinder<UFont> Heading(TEXT("/Game/Game/IN_UI/Font/Jua-Regular_Font.Jua-Regular_Font"));
    static ConstructorHelpers::FObjectFinder<UFontFace> Body(TEXT("/Game/Game/IN_UI/Font/NotoSansKR_Medium.NotoSansKR_Medium"));
    TitleFont = Heading.Object;
    BodyFace = Body.Object;
}

TSharedRef<SWidget> USFAchievementsWidget::RebuildWidget()
{
    using namespace SFAchievements;
    if (!BodyFont && BodyFace)
    {
        BodyFont = NewObject<UFont>(this);
        BodyFont->FontCacheType = EFontCacheType::Runtime;
        FTypefaceEntry FaceEntry(TEXT("Default"));
        FaceEntry.Font = FFontData(BodyFace);
        BodyFont->CompositeFont.DefaultTypeface.Fonts.Add(FaceEntry);
    }
    const auto* GI = GetGameInstance<USFGameInstance>();
    const TArray<FSFPVEAchievement> Entries = GI ? GI->GetPVEAchievements() : TArray<FSFPVEAchievement>();
    int32 Completed = 0;
    for (const auto& Entry : Entries) if (Entry.bUnlocked) ++Completed;
    const auto Font = [this](int32 Size, bool bTitle = false)
    { return FSlateFontInfo(bTitle ? TitleFont.Get() : BodyFont.Get(), Size); };
    const FSlateBrush* White = FCoreStyle::Get().GetBrush("WhiteBrush");
    auto List = SNew(SScrollBox).ScrollBarVisibility(EVisibility::Collapsed);
    for (const auto& Entry : Entries)
    {
        List->AddSlot().Padding(0, 0, 0, 8)
        [
            SNew(SBox).HeightOverride(86)
            [ SNew(SBorder).BorderImage(White).BorderBackgroundColor(Entry.bUnlocked ? Gold.CopyWithNewOpacity(.65f) : Muted.CopyWithNewOpacity(.22f)).Padding(1)
                [ SNew(SBorder).BorderImage(White).BorderBackgroundColor(Entry.bUnlocked ? Teal : FLinearColor(.018f,.035f,.035f)).Padding(FMargin(14, 5, 22, 5))
                    [ SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                        [ SNew(SSFAchievementCrest, Entry.StageOrder, Entry.bUnlocked) ]
                        + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center).Padding(20,0)
                        [ SNew(SVerticalBox)
                            + SVerticalBox::Slot().AutoHeight()
                            [ SNew(STextBlock).Text(Entry.Title).Font(Font(24,true)).ColorAndOpacity(Entry.bUnlocked ? Cream : Muted) ]
                            + SVerticalBox::Slot().AutoHeight().Padding(0,5,0,0)
                            [ SNew(STextBlock).Text(Entry.Description).Font(Font(15)).ColorAndOpacity(Muted) ] ]
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).HAlign(HAlign_Right)
                        [ SNew(SVerticalBox)
                            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
                            [ SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("STAGE %02d"), Entry.StageOrder))).Font(Font(12)).ColorAndOpacity(Muted) ]
                            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0,6,0,0)
                            [ SNew(STextBlock).Text(Entry.bUnlocked ? NSLOCTEXT("SFAchievements","Unlocked","달성 완료") : NSLOCTEXT("SFAchievements","Locked","미달성"))
                                .Font(Font(18,true)).ColorAndOpacity(Entry.bUnlocked ? Aqua : Muted) ] ] ] ] ]
        ];
    }

    auto Panel = SNew(SBorder).BorderImage(White).BorderBackgroundColor(Gold).Padding(3)
    [ SNew(SBorder).BorderImage(White).BorderBackgroundColor(FLinearColor(.012f,.035f,.034f)).Padding(7)
        [ SNew(SBorder).BorderImage(White).BorderBackgroundColor(Gold.CopyWithNewOpacity(.45f)).Padding(1)
            [ SNew(SBorder).BorderImage(White).BorderBackgroundColor(FLinearColor(.015f,.055f,.052f)).Padding(FMargin(32,24))
                [ SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight()
                    [ SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().FillWidth(1)
                        [ SNew(SVerticalBox)
                            + SVerticalBox::Slot().AutoHeight()
                            [ SNew(STextBlock).Text(NSLOCTEXT("SFAchievements","Journal","전장의 기록")).Font(Font(14)).ColorAndOpacity(Gold) ]
                            + SVerticalBox::Slot().AutoHeight().Padding(0,3,0,0)
                            [ SNew(STextBlock).Text(NSLOCTEXT("SFAchievements","Title","PvE 업적")).Font(Font(38,true)).ColorAndOpacity(Cream) ] ]
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                        [ SNew(SButton).ButtonColorAndOpacity(Teal).ContentPadding(FMargin(22,10))
                            .OnClicked_Lambda([this] { CloseAchievements(); return FReply::Handled(); })
                            [ SNew(STextBlock).Text(NSLOCTEXT("SFAchievements","Close","닫기  ·  ESC")).Font(Font(18)).ColorAndOpacity(Cream) ] ] ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0,14,0,9)
                    [ SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().FillWidth(1)
                        [ SNew(STextBlock).Text(NSLOCTEXT("SFAchievements","Hint","승리로 새기는 일곱 개의 발자취")).Font(Font(16)).ColorAndOpacity(Muted) ]
                        + SHorizontalBox::Slot().AutoWidth()
                        [ SNew(STextBlock).Text(FText::Format(NSLOCTEXT("SFAchievements","Progress","{0} / {1} 달성"), FText::AsNumber(Completed), FText::AsNumber(Entries.Num())))
                            .Font(Font(18,true)).ColorAndOpacity(Gold) ] ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,20)
                    [ SNew(SBox).HeightOverride(5)
                        [ SNew(SProgressBar).Percent(Entries.IsEmpty() ? 0.f : float(Completed)/Entries.Num()).FillColorAndOpacity(Aqua) ] ]
                    + SVerticalBox::Slot().FillHeight(1) [ List ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0,10,0,0)
                    [ SNew(STextBlock).Text(NSLOCTEXT("SFAchievements","Footer","PvE 전용  ·  스테이지를 클리어하면 업적이 자동으로 달성됩니다."))
                        .Font(Font(14)).ColorAndOpacity(Muted).Justification(ETextJustify::Center) ] ] ] ] ];
    TSharedRef<FNavigationMetaData> FocusNavigation = MakeShared<FNavigationMetaData>();
    FocusNavigation->SetNavigationWrap(EUINavigation::Next);
    FocusNavigation->SetNavigationWrap(EUINavigation::Previous);
    Panel->AddMetadata(FocusNavigation);

    return SNew(SOverlay)
        + SOverlay::Slot()
        [ SNew(SBorder).BorderImage(White).BorderBackgroundColor(FLinearColor(.002f,.012f,.012f,.82f))
            .OnMouseButtonDown_Lambda([](const FGeometry&,const FPointerEvent&) { return FReply::Handled(); }) ]
        + SOverlay::Slot().Padding(32)
        [ SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly)
            [ SNew(SBox).WidthOverride(1180).HeightOverride(930) [ Panel ] ] ];
}

void USFAchievementsWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SetFocus();
}

void USFAchievementsWidget::CloseAchievements()
{
    RemoveFromParent();
    OnClosed.Broadcast();
}

FReply USFAchievementsWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
    if (Event.GetKey() == EKeys::Escape || Event.GetKey() == EKeys::Gamepad_FaceButton_Right)
    {
        CloseAchievements();
        return FReply::Handled();
    }
    return Super::NativeOnPreviewKeyDown(Geometry, Event);
}
