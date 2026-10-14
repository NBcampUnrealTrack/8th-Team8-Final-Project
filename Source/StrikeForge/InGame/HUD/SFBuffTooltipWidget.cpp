#include "InGame/HUD/SFBuffTooltipWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Font.h"

void USFBuffTooltipWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    SetIsFocusable(false);
    SetVisibility(ESlateVisibility::HitTestInvisible);

    const FLinearColor Gold(.64f, .39f, .14f, 1.f);
    UFont* Font = LoadObject<UFont>(nullptr, TEXT("/Game/Game/IN_UI/Font/Jua-Regular_Font.Jua-Regular_Font"));
    auto MakeText = [&](FName Name, int32 FontSize, FLinearColor Color)
    {
        auto* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
        FSlateFontInfo Info = Text->GetFont();
        if (Font) Info.FontObject = Font;
        Info.Size = FontSize;
        Text->SetFont(Info);
        Text->SetColorAndOpacity(FSlateColor(Color));
        Text->SetWrapTextAt(190.f);
        Text->SetShadowOffset(FVector2D(0.f, 1.f));
        Text->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, .6f));
        return Text;
    };

    auto* Size = WidgetTree->ConstructWidget<USizeBox>();
    Size->SetWidthOverride(208.f);
    WidgetTree->RootWidget = Size;
    auto* Frame = WidgetTree->ConstructWidget<UBorder>();
    Frame->SetBrushColor(Gold);
    Frame->SetPadding(FMargin(1.f));
    Size->SetContent(Frame);
    auto* Background = WidgetTree->ConstructWidget<UBorder>();
    Background->SetBrushColor(FLinearColor(.028f, .04f, .033f, 1.f));
    Background->SetPadding(FMargin(8.f, 6.f));
    Frame->SetContent(Background);
    auto* Lines = WidgetTree->ConstructWidget<UVerticalBox>();
    Background->SetContent(Lines);

    TitleText = MakeText(TEXT("BuffTitle"), 14, FLinearColor(.95f, .88f, .72f, 1.f));
    Lines->AddChildToVerticalBox(TitleText)->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));
    auto* DividerSize = WidgetTree->ConstructWidget<USizeBox>();
    DividerSize->SetHeightOverride(1.f);
    auto* Divider = WidgetTree->ConstructWidget<UBorder>();
    Divider->SetBrushColor(FLinearColor(.35f, .25f, .12f, 1.f));
    DividerSize->SetContent(Divider);
    Lines->AddChildToVerticalBox(DividerSize)->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));
    EffectText = MakeText(TEXT("BuffEffect"), 12, FLinearColor(.84f, .83f, .75f, 1.f));
    Lines->AddChildToVerticalBox(EffectText);
}

void USFBuffTooltipWidget::SetDetails(const FText& Name, const FText& Description)
{
    if (TitleText) TitleText->SetText(Name);
    if (EffectText) EffectText->SetText(Description);
}
