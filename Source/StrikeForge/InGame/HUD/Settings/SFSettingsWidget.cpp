#include "SFSettingsWidget.h"
#include "SFPauseMenuPresentation.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Font.h"
#include "Engine/FontFace.h"
#include "Framework/Application/SlateApplication.h"
#include "Brushes/SlateNoResource.h"
#include "UObject/ConstructorHelpers.h"
#include "Game/Audio/SFAudioLibrary.h"
#include "Game/GameInstance/SFGameInstance.h"
#include "Logging/SFLog.h"
#include "UI/SFResultPresentation.h"
#include "UI/SettingWidget.h"

USFSettingsWidget::USFSettingsWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
    static ConstructorHelpers::FObjectFinder<UFont> Font(TEXT("/Game/Game/IN_UI/Font/Jua-Regular_Font.Jua-Regular_Font"));
    static ConstructorHelpers::FObjectFinder<UFontFace> Face(TEXT("/Game/Game/IN_UI/Font/NanumMyeongjo_ExtraBold.NanumMyeongjo_ExtraBold"));
    MenuFont = Font.Object;
    TitleFace = Face.Object;
    SetIsFocusable(true);
}

TSharedRef<SWidget> USFSettingsWidget::RebuildWidget()
{
    // BindWidget references are resolved during Initialize, before Slate is built.
    // Keep the Blueprint's actual buttons and their actions inside the new layout.
    if (!IsDesignTime() && !MenuRoot && WidgetTree && ContinueButton && ExitButton)
        BuildMenuPresentation();
    return Super::RebuildWidget();
}

void USFSettingsWidget::BuildMenuPresentation()
{
    if (TitleFace)
    {
        TitleFont = NewObject<UFont>(this);
        TitleFont->FontCacheType = EFontCacheType::Runtime;
        FTypefaceEntry Entry(TEXT("Default"));
        Entry.Font = FFontData(TitleFace);
        TitleFont->CompositeFont.DefaultTypeface.Fonts.Add(Entry);
    }
    OptionsButton = Cast<UButton>(GetWidgetFromName(TEXT("SettingButton")));
    OptionsContent = Cast<USettingWidget>(GetWidgetFromName(TEXT("SettingContent")));
    if (OptionsButton) OptionsButton->RemoveFromParent();
    if (OptionsContent) OptionsContent->RemoveFromParent();
    ContinueButton->RemoveFromParent();
    ExitButton->RemoveFromParent();
    MenuRoot = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("PauseMenuRoot"));
    WidgetTree->RootWidget = MenuRoot;
    MenuRoot->SetVisibility(ESlateVisibility::Visible);

    const auto Fill = [](UCanvasPanelSlot* LayoutSlot, float Inset = 0.f)
    {
        LayoutSlot->SetAnchors(FAnchors(0, 0, 1, 1));
        LayoutSlot->SetOffsets(FMargin(Inset));
    };
    Atmosphere = WidgetTree->ConstructWidget<USFPauseMenuPresentation>();
    Atmosphere->bBackdrop = true;
    Atmosphere->SetVisibility(ESlateVisibility::HitTestInvisible);
    Fill(MenuRoot->AddChildToCanvas(Atmosphere));
    auto* Fit = WidgetTree->ConstructWidget<UScaleBox>();
    Fit->SetStretch(EStretch::ScaleToFit);
    Fit->SetStretchDirection(EStretchDirection::DownOnly);
    Fill(MenuRoot->AddChildToCanvas(Fit), 24.f);
    auto* Size = WidgetTree->ConstructWidget<USizeBox>();
    Size->SetWidthOverride(620);
    Size->SetHeightOverride(740);
    auto* FitSlot = Cast<UScaleBoxSlot>(Fit->AddChild(Size));
    FitSlot->SetHorizontalAlignment(HAlign_Center);
    FitSlot->SetVerticalAlignment(VAlign_Center);
    auto* Panel = WidgetTree->ConstructWidget<UCanvasPanel>();
    Size->AddChild(Panel);
    PanelArt = WidgetTree->ConstructWidget<USFPauseMenuPresentation>();
    PanelArt->SetVisibility(ESlateVisibility::HitTestInvisible);
    Fill(Panel->AddChildToCanvas(PanelArt));

    auto Label = [&](FName Name, const FText& Text, int32 FontSize, FLinearColor Color, float Y, float Height, bool bTitle = false)
    {
        auto* TextWidget = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
        TextWidget->SetText(Text);
        TextWidget->SetFont(FSlateFontInfo(bTitle && TitleFont ? TitleFont.Get() : MenuFont.Get(), FontSize));
        TextWidget->SetColorAndOpacity(Color);
        TextWidget->SetJustification(ETextJustify::Center);
        TextWidget->SetShadowOffset(FVector2D(0, 2));
        TextWidget->SetShadowColorAndOpacity(FLinearColor(0, 0, 0, .75f));
        TextWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
        auto* Slot = Panel->AddChildToCanvas(TextWidget);
        Slot->SetPosition(FVector2D(70, Y));
        Slot->SetSize(FVector2D(480, Height));
    };
    Label(TEXT("PauseMenuTitle"), NSLOCTEXT("SFPauseMenu", "Title", "메뉴"), 48,
        FLinearColor(1.f, .9f, .69f), 210, 66, true);
    Label(TEXT("PauseMenuSubtitle"), NSLOCTEXT("SFPauseMenu", "Subtitle", "다음 승리를 위한, 잠깐의 휴식"), 19,
        FLinearColor(.59f, .75f, .73f), 280, 30);

    const auto AddButton = [&](UButton* Button, const FText& Text, float Y, bool bPrimary)
    {
        FButtonStyle Style = Button->GetStyle();
        Style.SetNormal(FSlateNoResource()).SetHovered(FSlateNoResource())
            .SetPressed(FSlateNoResource()).SetDisabled(FSlateNoResource());
        Style.SetNormalPadding(FMargin(0)).SetPressedPadding(FMargin(0, 2, 0, 0));
        Button->SetStyle(Style);
        Button->SetVisibility(ESlateVisibility::Visible);
        Button->SetBackgroundColor(FLinearColor::White);
        Button->SetColorAndOpacity(FLinearColor::White);
        auto* Content = WidgetTree->ConstructWidget<UOverlay>();
        Button->SetContent(Content);
        auto* ContentSlot = Cast<UButtonSlot>(Content->Slot);
        ContentSlot->SetHorizontalAlignment(HAlign_Fill);
        ContentSlot->SetVerticalAlignment(VAlign_Fill);
        ContentSlot->SetPadding(FMargin(0));
        Content->SetVisibility(ESlateVisibility::HitTestInvisible);
        auto* Art = WidgetTree->ConstructWidget<USFResultPresentation>();
        Art->bExitButtonDecoration = true;
        Art->ExitButton = Button;
        Art->SetVisibility(ESlateVisibility::HitTestInvisible);
        auto* ArtSlot = Content->AddChildToOverlay(Art);
        ArtSlot->SetHorizontalAlignment(HAlign_Fill);
        ArtSlot->SetVerticalAlignment(VAlign_Fill);
        auto* Caption = WidgetTree->ConstructWidget<UTextBlock>();
        Caption->SetText(Text);
        Caption->SetFont(FSlateFontInfo(MenuFont, bPrimary ? 28 : 24));
        Caption->SetColorAndOpacity(bPrimary ? FLinearColor(.94f, .97f, .84f) : FLinearColor(.78f, .82f, .77f));
        Caption->SetShadowOffset(FVector2D(0, 2));
        Caption->SetShadowColorAndOpacity(FLinearColor(0, 0, 0, .85f));
        auto* CaptionSlot = Content->AddChildToOverlay(Caption);
        CaptionSlot->SetHorizontalAlignment(HAlign_Center);
        CaptionSlot->SetVerticalAlignment(VAlign_Center);
        CaptionSlot->SetPadding(FMargin(52, 8));
        auto* Slot = Panel->AddChildToCanvas(Button);
        Slot->SetPosition(FVector2D(bPrimary ? 100 : 110, Y));
        Slot->SetSize(FVector2D(bPrimary ? 420 : 400, bPrimary ? 88 : 80));
    };
    AddButton(ContinueButton, NSLOCTEXT("SFPauseMenu", "Continue", "계속하기"), 338, true);
    if (OptionsButton && OptionsContent)
        AddButton(OptionsButton, NSLOCTEXT("SFPauseMenu", "Settings", "설정"), 442, false);
    AddButton(ExitButton, NSLOCTEXT("SFPauseMenu", "Lobby", "로비로 돌아가기"), 538, false);
    Label(TEXT("PauseMenuShortcut"), NSLOCTEXT("SFPauseMenu", "Shortcut", "ESC / F10   전장으로 돌아가기"), 16,
        FLinearColor(.57f, .65f, .63f), 647, 28);
    Label(TEXT("PauseMenuSignature"), FText::FromString(TEXT("E T H E R   O A T H")), 12,
        FLinearColor(.63f, .51f, .31f), 707, 22);
    if (OptionsContent)
    {
        auto* OptionsSlot = MenuRoot->AddChildToCanvas(OptionsContent);
        Fill(OptionsSlot);
        OptionsSlot->SetZOrder(10);
        OptionsContent->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void USFSettingsWidget::NativeOnActivated()
{
    Super::NativeOnActivated();
    const double Now = FSlateApplication::Get().GetCurrentTime();
    if (Atmosphere) Atmosphere->StartTime = Now;
    if (PanelArt) PanelArt->StartTime = Now;
    if (OptionsContent) OptionsContent->SetVisibility(ESlateVisibility::Collapsed);
}

void USFSettingsWidget::HandleOptionsButtonClicked()
{
    if (OptionsContent) OptionsContent->OpenSetting();
}

UWidget* USFSettingsWidget::NativeGetDesiredFocusTarget() const
{
    return ContinueButton;
}

FReply USFSettingsWidget::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
    if (Event.GetKey() == EKeys::Escape || Event.GetKey() == EKeys::F10)
    {
        if (!Event.IsRepeat())
        {
            if (OptionsContent && OptionsContent->IsVisible())
            {
                OptionsContent->SetVisibility(ESlateVisibility::Collapsed);
                SetFocus();
            }
            else HandleContinueButtonClicked();
        }
        return FReply::Handled();
    }
    return Super::NativeOnKeyDown(Geometry, Event);
}

FReply USFSettingsWidget::NativeOnMouseButtonDown(const FGeometry&, const FPointerEvent&)
{
    // A click on the modal backdrop must not issue an order to the battlefield.
    return FReply::Handled();
}

void USFSettingsWidget::NativeConstruct()
{
    Super::NativeConstruct();
    USFAudioLibrary::ApplyMenuButtonSounds(this);
    if (OptionsContent)
    {
        OptionsContent->OnNativeVisibilityChanged.RemoveAll(this);
        OptionsContent->OnNativeVisibilityChanged.AddWeakLambda(this, [this](ESlateVisibility InVisibility)
        {
            if (IsActivated() && (InVisibility == ESlateVisibility::Collapsed || InVisibility == ESlateVisibility::Hidden))
                SetFocus();
        });
    }
    if (OptionsButton)
    {
        // Replace the Blueprint's old panel-switching callback with this modal's settings page.
        OptionsButton->OnClicked.Clear();
        OptionsButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleOptionsButtonClicked);
    }

    if (IsValid(ContinueButton))
    {
        ContinueButton->OnClicked.AddUniqueDynamic(this, &USFSettingsWidget::HandleContinueButtonClicked);
    }
    else
    {
        SF_LOG(Error, "[SFSettingsWidget] ContinueButton is invalid.");
    }

    if (IsValid(ExitButton))
    {
        ExitButton->OnClicked.AddUniqueDynamic(this, &USFSettingsWidget::HandleExitButtonClicked);
    }
    else
    {
        SF_LOG(Error, "[SFSettingsWidget] ExitButton is invalid.");
    }

    SF_LOG(Log, "[SFSettingsWidget] Button binding completed | Widget: %s", *GetName());
}

void USFSettingsWidget::NativeDestruct()
{
    if (OptionsContent) OptionsContent->OnNativeVisibilityChanged.RemoveAll(this);
    if (OptionsButton) OptionsButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleOptionsButtonClicked);
    if (IsValid(ContinueButton))
    {
        ContinueButton->OnClicked.RemoveDynamic(this, &USFSettingsWidget::HandleContinueButtonClicked);
    }

    if (IsValid(ExitButton))
    {
        ExitButton->OnClicked.RemoveDynamic(this, &USFSettingsWidget::HandleExitButtonClicked);
    }

    Super::NativeDestruct();
}

void USFSettingsWidget::HandleContinueButtonClicked()
{
    SF_LOG(Log, "[SFSettingsWidget] Continue requested.");
    DeactivateWidget();
}

void USFSettingsWidget::HandleExitButtonClicked()
{
    USFGameInstance* GameInstance = Cast<USFGameInstance>(GetGameInstance());

    if (!IsValid(GameInstance))
    {
        SF_LOG(Error, "[SFSettingsWidget] SFGameInstance is invalid.");
        return;
    }

    SF_LOG(Log, "[SFSettingsWidget] Move to lobby requested.");

    if (!GameInstance->MoveToLobby())
    {
        SF_LOG(Error, "[SFSettingsWidget] Failed to move to lobby.");
    }
}
