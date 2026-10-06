#include "SFRaceSelectionWidget.h"
#include "Game/GameInstance/SFGameInstance.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"
#include "InputCoreTypes.h"

TSharedRef<SWidget> USFRaceSelectionWidget::RebuildWidget()
{
    const auto Label = [](const TCHAR* Text, int32 Size)
    {
        return SNew(STextBlock).Text(FText::FromString(Text))
            .Font(FCoreStyle::GetDefaultFontStyle("Regular", Size)).Justification(ETextJustify::Center);
    };
    return SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
        .BorderBackgroundColor(FLinearColor(0.015f, 0.025f, 0.04f, 0.98f))
        .HAlign(HAlign_Center).VAlign(VAlign_Center)
        [SNew(SBox).WidthOverride(620.f)
        [SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 24)[Label(TEXT("최후의 전투 · 종족 선택"), 30)]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 28)
            [Label(TEXT("선택한 종족의 본진에서 시작합니다.\n다른 종족은 적군이 됩니다."), 20)]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 8)
            [SNew(SButton).HAlign(HAlign_Center).ContentPadding(FMargin(24)).OnClicked_Lambda([this]{ return Choose(false); })
            [Label(TEXT("에테리안"), 26)]]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 8)
            [SNew(SButton).HAlign(HAlign_Center).ContentPadding(FMargin(24)).OnClicked_Lambda([this]{ return Choose(true); })
            [Label(TEXT("비스트"), 26)]]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 24, 0, 0)
            [SNew(SButton).HAlign(HAlign_Center).ContentPadding(FMargin(16)).OnClicked_Lambda([this]{ return Cancel(); })
            [Label(TEXT("돌아가기"), 20)]]]];
}

void USFRaceSelectionWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SetIsFocusable(true);
    SetKeyboardFocus();
}

FReply USFRaceSelectionWidget::Choose(bool bBeast)
{
    USFGameInstance* GI = GetGameInstance<USFGameInstance>();
    if (GI && GI->SelectRace(bBeast ? ESFRace::Beast : ESFRace::Human) && GI->MoveToStage()) RemoveFromParent();
    return FReply::Handled();
}

FReply USFRaceSelectionWidget::Cancel()
{
    RemoveFromParent();
    return FReply::Handled();
}

FReply USFRaceSelectionWidget::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
    return Event.GetKey() == EKeys::Escape ? Cancel() : Super::NativeOnKeyDown(Geometry, Event);
}
