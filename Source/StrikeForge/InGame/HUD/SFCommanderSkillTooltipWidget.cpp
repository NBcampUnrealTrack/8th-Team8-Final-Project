#include "InGame/HUD/SFCommanderSkillTooltipWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Font.h"

void USFCommanderSkillTooltipWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    SetIsFocusable(false);
    SetVisibility(ESlateVisibility::HitTestInvisible);

    const FLinearColor Gold(.64f, .39f, .14f, 1.f);
    const FLinearColor Ivory(.95f, .88f, .72f, 1.f);
    UFont* Font = LoadObject<UFont>(nullptr, TEXT("/Game/Game/IN_UI/Font/Jua-Regular_Font.Jua-Regular_Font"));
    auto Text = [&](int32 Size, FLinearColor Color)
    {
        UTextBlock* Block = WidgetTree->ConstructWidget<UTextBlock>();
        FSlateFontInfo Info = Block->GetFont();
        if (Font) Info.FontObject = Font;
        Info.Size = Size;
        Block->SetFont(Info);
        Block->SetColorAndOpacity(FSlateColor(Color));
        Block->SetShadowOffset(FVector2D(0.f, 1.f));
        Block->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, .6f));
        return Block;
    };

    USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
    Size->SetWidthOverride(380.f);
    WidgetTree->RootWidget = Size;
    UBorder* Frame = WidgetTree->ConstructWidget<UBorder>();
    Frame->SetBrushColor(Gold);
    Frame->SetPadding(FMargin(2.f));
    Size->SetContent(Frame);
    UBorder* Background = WidgetTree->ConstructWidget<UBorder>();
    Background->SetBrushColor(FLinearColor(.028f, .04f, .033f, 1.f));
    Background->SetPadding(FMargin(16.f, 12.f));
    Frame->SetContent(Background);
    UVerticalBox* Lines = WidgetTree->ConstructWidget<UVerticalBox>();
    Background->SetContent(Lines);

    UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>();
    Lines->AddChildToVerticalBox(Header)->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
    UBorder* KeyBadge = WidgetTree->ConstructWidget<UBorder>();
    KeyBadge->SetBrushColor(FLinearColor(.025f, .22f, .19f, 1.f));
    KeyBadge->SetPadding(FMargin(10.f, 3.f));
    KeyText = Text(18, Ivory);
    KeyBadge->SetContent(KeyText);
    Header->AddChildToHorizontalBox(KeyBadge)->SetVerticalAlignment(VAlign_Center);
    TitleText = Text(21, Ivory);
    TitleText->SetAutoWrapText(true);
    UHorizontalBoxSlot* TitleSlot = Header->AddChildToHorizontalBox(TitleText);
    TitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    TitleSlot->SetPadding(FMargin(12.f, 0.f, 0.f, 0.f));
    TitleSlot->SetVerticalAlignment(VAlign_Center);

    EffectText = Text(17, FLinearColor(.84f, .83f, .75f, 1.f));
    EffectText->SetWrapTextAt(344.f);
    Lines->AddChildToVerticalBox(EffectText)->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
    USizeBox* DividerSize = WidgetTree->ConstructWidget<USizeBox>();
    DividerSize->SetHeightOverride(1.f);
    UBorder* Divider = WidgetTree->ConstructWidget<UBorder>();
    Divider->SetBrushColor(Gold);
    DividerSize->SetContent(Divider);
    Lines->AddChildToVerticalBox(DividerSize)->SetPadding(FMargin(0.f, 0.f, 0.f, 9.f));
    RequirementText = Text(15, Ivory);
    RequirementText->SetAutoWrapText(true);
    Lines->AddChildToVerticalBox(RequirementText);
    CooldownText = Text(15, FLinearColor(.3f, .8f, .72f, 1.f));
    Lines->AddChildToVerticalBox(CooldownText)->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
}

void USFCommanderSkillTooltipWidget::SetSkillDetails(const FText& Name, const FText& Effect,
    const FString& Key, int32 Tier, bool bUnlocked, float CooldownSeconds)
{
    auto Update = [](UTextBlock* Block, const FText& Value)
    {
        if (Block && !Block->GetText().EqualTo(Value)) Block->SetText(Value);
    };
    Update(TitleText, Name);
    Update(KeyText, FText::FromString(Key));
    Update(EffectText, Effect);
    Update(RequirementText, FText::Format(bUnlocked
        ? NSLOCTEXT("SFSkill", "Unlocked", "넥서스 {0}티어 / 사용 가능")
        : NSLOCTEXT("SFSkill", "Locked", "잠김 / 넥서스 {0}티어 필요"), FText::AsNumber(Tier)));
    if (RequirementText) RequirementText->SetColorAndOpacity(FSlateColor(bUnlocked
        ? FLinearColor(.88f, .7f, .4f, 1.f) : FLinearColor(1.f, .44f, .3f, 1.f)));
    Update(CooldownText, FText::Format(NSLOCTEXT("SFSkill", "Cooldown", "재사용 대기시간  {0}초"),
        FText::AsNumber(FMath::RoundToInt(CooldownSeconds))));
}
