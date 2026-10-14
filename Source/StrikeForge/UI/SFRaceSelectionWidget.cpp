#include "SFRaceSelectionWidget.h"
#include "Game/GameInstance/SFGameInstance.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "UObject/ConstructorHelpers.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"
#include "InputCoreTypes.h"

namespace SFRaceMenu
{
    const FLinearColor Gold(.69f, .47f, .22f);
    const FLinearColor Cream(.98f, .9f, .72f);
    const FLinearColor Muted(.64f, .69f, .65f);

    FButtonStyle CardStyle(const FLinearColor& Color)
    {
        return FButtonStyle()
            .SetNormal(FSlateRoundedBoxBrush(Color, 8.f, Gold, 2.f))
            .SetHovered(FSlateRoundedBoxBrush(FLinearColor(Color.R * 1.65f, Color.G * 1.65f, Color.B * 1.65f), 8.f, Cream, 3.f))
            .SetPressed(FSlateRoundedBoxBrush(FLinearColor(Color.R * .75f, Color.G * .75f, Color.B * .75f), 8.f, Gold, 3.f))
            .SetNormalPadding(FMargin(0)).SetPressedPadding(FMargin(0, 2, 0, 0));
    }
}

USFRaceSelectionWidget::USFRaceSelectionWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    // Keep the existing game illustrations referenced in packaged builds too.
    static ConstructorHelpers::FObjectFinder<UTexture2D> Etherian(TEXT("/Game/Game/IN_UI/Image/Intro/StoryUse/Etherial/Commander/T_EtherCommander_Normal.T_EtherCommander_Normal"));
    static ConstructorHelpers::FObjectFinder<UTexture2D> Beast(TEXT("/Game/Game/IN_UI/Image/Intro/StoryUse/Beast/Commander/Beast_Commander_Smile.Beast_Commander_Smile"));
    static ConstructorHelpers::FObjectFinder<UFont> Font(TEXT("/Game/Game/IN_UI/Font/Jua-Regular_Font.Jua-Regular_Font"));
    EtherianPortrait = Etherian.Object;
    BeastPortrait = Beast.Object;
    TitleFont = Font.Object;
    SetIsFocusable(true);
}

TSharedRef<SWidget> USFRaceSelectionWidget::RebuildWidget()
{
    using namespace SFRaceMenu;
    const auto Label = [this](const TCHAR* Text, int32 Size, FLinearColor Color)
    {
        return SNew(STextBlock).Text(FText::FromString(Text))
            .Font(TitleFont ? FSlateFontInfo(TitleFont, Size) : FCoreStyle::GetDefaultFontStyle("Regular", Size))
            .ColorAndOpacity(Color).Justification(ETextJustify::Center)
            .ShadowOffset(FVector2D(0, 1)).ShadowColorAndOpacity(FLinearColor(0, 0, 0, .65f));
    };
    EtherianBrush.SetResourceObject(EtherianPortrait);
    BeastBrush.SetResourceObject(BeastPortrait);
    EtherianBrush.ImageSize = BeastBrush.ImageSize = FVector2D(280, 280);
    EtherianButtonStyle = CardStyle(FLinearColor(.025f, .095f, .09f));
    BeastButtonStyle = CardStyle(FLinearColor(.10f, .061f, .037f));
    BackButtonStyle = CardStyle(FLinearColor(.045f, .05f, .043f));

    const auto Card = [&](ESFRace Race, const TCHAR* Name, const TCHAR* Subtitle,
        const TCHAR* Description, const TCHAR* Action, const FSlateBrush* Portrait, const FButtonStyle* Style,
        FLinearColor Accent) -> TSharedRef<SWidget>
    {
        return SNew(SButton).ButtonStyle(Style).ContentPadding(0)
            .HAlign(HAlign_Fill).VAlign(VAlign_Fill)
            .IsEnabled_Lambda([this] { return !bTravelling; })
            .OnClicked_Lambda([this, Race] { return Choose(Race); })
            [SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().Padding(20, 20, 20, 0)[Label(Subtitle, 18, Accent)]
                + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(12, 6)
                    [SNew(SBox).WidthOverride(280).HeightOverride(280)[SNew(SImage).Image(Portrait)]]
                + SVerticalBox::Slot().AutoHeight().Padding(16, 0)[Label(Name, 32, Cream)]
                + SVerticalBox::Slot().AutoHeight().Padding(16, 8, 16, 18)[Label(Description, 18, Muted)]
                + SVerticalBox::Slot().AutoHeight().Padding(20, 0, 20, 20)
                    [SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                        .BorderBackgroundColor(FLinearColor(Accent.R * .28f, Accent.G * .28f, Accent.B * .28f)).Padding(FMargin(14, 12))
                        [Label(Action, 22, Cream)]]];
    };

    return SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
        .BorderBackgroundColor(FLinearColor(.008f, .015f, .015f, .88f)).Padding(24)
        [SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly)
        [SNew(SBox).WidthOverride(960)
        [SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
            .BorderBackgroundColor(Gold).Padding(2)
        [SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
            .BorderBackgroundColor(FLinearColor(.018f, .032f, .03f)).Padding(FMargin(44, 28))
        [SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()[Label(TEXT("S T A G E  0 7"), 17, Gold)]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 7)[Label(TEXT("최후의 전투"), 40, Cream)]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 22)
                [Label(TEXT("당신의 진영을 선택하세요"), 21, Muted)]
            + SVerticalBox::Slot().AutoHeight()
            [SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(1).Padding(0, 0, 10, 0)
                    [Card(ESFRace::Human, TEXT("에테리안"), TEXT("에테르의 수호자"),
                        TEXT("회복과 보호로 전선을 지켜내세요."), TEXT("에테리안으로 출전"),
                        &EtherianBrush, &EtherianButtonStyle, FLinearColor(.2f, .68f, .62f))]
                + SHorizontalBox::Slot().FillWidth(1).Padding(10, 0, 0, 0)
                    [Card(ESFRace::Beast, TEXT("비스트"), TEXT("깨어난 야성"),
                        TEXT("돌격과 함성으로 전장을 압도하세요."), TEXT("비스트로 출전"),
                        &BeastBrush, &BeastButtonStyle, FLinearColor(.84f, .47f, .22f))]]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 18, 0, 0)
                [Label(TEXT("선택한 종족의 본진과 지휘관으로 시작합니다. 상대 종족은 적군이 됩니다."), 17, Muted)]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 4)
                [SNew(STextBlock).Text_Lambda([this] { return TravelError; })
                    .Font(TitleFont ? FSlateFontInfo(TitleFont, 17) : FCoreStyle::GetDefaultFontStyle("Regular", 17))
                    .Justification(ETextJustify::Center).ColorAndOpacity(FLinearColor(1.f, .48f, .3f))]
            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 8, 0, 0)
                [SNew(SButton).ButtonStyle(&BackButtonStyle).ContentPadding(FMargin(40, 10))
                    .OnClicked_Lambda([this] { return Cancel(); })[Label(TEXT("로비로 돌아가기"), 19, Cream)]]]]]]];
}

void USFRaceSelectionWidget::NativeConstruct()
{
    Super::NativeConstruct();
    bTravelling = false;
    TravelError = FText::GetEmpty();
    SetKeyboardFocus();
}

FReply USFRaceSelectionWidget::Choose(ESFRace Race)
{
    if (bTravelling) return FReply::Handled();
    USFGameInstance* GI = GetGameInstance<USFGameInstance>();
    if (GI && GI->SelectRace(Race) && GI->MoveToStage())
    {
        bTravelling = true;
        RemoveFromParent();
    }
    else
    {
        TravelError = NSLOCTEXT("SFRaceMenu", "TravelFailed", "전장에 진입하지 못했습니다. 잠시 후 다시 선택해 주세요.");
    }
    return FReply::Handled();
}

FReply USFRaceSelectionWidget::Cancel()
{
    if (!bTravelling) RemoveFromParent();
    return FReply::Handled();
}

FReply USFRaceSelectionWidget::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
    return Event.GetKey() == EKeys::Escape ? Cancel() : Super::NativeOnKeyDown(Geometry, Event);
}
