#include "SettingWidget.h"
#include "SFOptionsPresentation.h"
#include "SFResultPresentation.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateNoResource.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/BackgroundBlur.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ComboBoxString.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Engine/Font.h"
#include "Engine/FontFace.h"
#include "Framework/Application/SlateApplication.h"
#include "InGame/HUD/Settings/SFPauseMenuPresentation.h"
#include "UObject/ConstructorHelpers.h"

USettingWidget::USettingWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
    static ConstructorHelpers::FObjectFinder<UFontFace> Body(TEXT("/Game/Game/IN_UI/Font/NotoSansKR_Medium.NotoSansKR_Medium"));
    static ConstructorHelpers::FObjectFinder<UFontFace> Heading(TEXT("/Game/Game/IN_UI/Font/NotoSansKR_Bold.NotoSansKR_Bold"));
    SettingsBodyFace = Body.Object;
    SettingsTitleFace = Heading.Object;
}

TSharedRef<SWidget> USettingWidget::RebuildWidget()
{
    if (!IsDesignTime() && !SettingsRoot && WidgetTree && BackUi_Btn && ResolutionComboBox
        && FullScreenComboBox && MouseSensitivitySlider && MasterVolumeSlider && BGMVolumeSlider && SXVolumeSlider)
        BuildSettingsPresentation();
    return Super::RebuildWidget();
}

UWidget* USettingWidget::GenerateOptionLabel(FString Item)
{
    FText Caption = FText::FromString(Item);
    if (Item == TEXT("Fullscreen")) Caption = NSLOCTEXT("SFSettings", "Fullscreen", "전체 화면");
    else if (Item == TEXT("WindowedFullscreen")) Caption = NSLOCTEXT("SFSettings", "Borderless", "테두리 없는 전체 화면");
    else if (Item == TEXT("Windowed")) Caption = NSLOCTEXT("SFSettings", "Windowed", "창 모드");
    auto* Label = WidgetTree->ConstructWidget<UTextBlock>();
    Label->SetText(Caption);
    Label->SetFont(FSlateFontInfo(SettingsFont, 22));
    Label->SetColorAndOpacity(SFOptionsColors::Hex(TEXT("FFF0CB")));
    Label->SetMargin(FMargin(10, 1));
    Label->SetVisibility(ESlateVisibility::HitTestInvisible);
    return Label;
}

void USettingWidget::BuildSettingsPresentation()
{
    using namespace SFOptionsColors;
    auto Font = [this](UFontFace* Face)
    {
        auto* Result = NewObject<UFont>(this);
        Result->FontCacheType = EFontCacheType::Runtime;
        FTypefaceEntry Entry(TEXT("Default"));
        Entry.Font = FFontData(Face);
        Result->CompositeFont.DefaultTypeface.Fonts.Add(Entry);
        return Result;
    };
    SettingsFont = Font(SettingsBodyFace);
    SettingsTitleFont = Font(SettingsTitleFace);
    for (UWidget* Control : TArray<UWidget*>{BackUi_Btn, ResolutionComboBox, FullScreenComboBox,
        MouseSensitivitySlider, MasterVolumeSlider, BGMVolumeSlider, SXVolumeSlider, BackgroundBlur})
        if (Control) Control->RemoveFromParent();
    SettingsRoot = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("SettingsModalRoot"));
    WidgetTree->RootWidget = SettingsRoot;
    auto Fill = [](UCanvasPanelSlot* LayoutSlot, float Inset = 0.f)
    { LayoutSlot->SetAnchors(FAnchors(0,0,1,1)); LayoutSlot->SetOffsets(FMargin(Inset)); };
    if (BackgroundBlur)
    {
        BackgroundBlur->ClearChildren();
        BackgroundBlur->SetBlurStrength(3.f);
        BackgroundBlur->SetVisibility(ESlateVisibility::HitTestInvisible);
        Fill(SettingsRoot->AddChildToCanvas(BackgroundBlur));
    }
    SettingsAtmosphere = WidgetTree->ConstructWidget<USFPauseMenuPresentation>();
    SettingsAtmosphere->bBackdrop = true;
    SettingsAtmosphere->SetVisibility(ESlateVisibility::HitTestInvisible);
    Fill(SettingsRoot->AddChildToCanvas(SettingsAtmosphere));
    auto* Fit = WidgetTree->ConstructWidget<UScaleBox>();
    Fit->SetStretch(EStretch::ScaleToFit);
    Fit->SetStretchDirection(EStretchDirection::DownOnly);
    Fill(SettingsRoot->AddChildToCanvas(Fit), 20);
    auto* Size = WidgetTree->ConstructWidget<USizeBox>();
    Size->SetWidthOverride(1240);
    Size->SetHeightOverride(900);
    auto* FitSlot = Cast<UScaleBoxSlot>(Fit->AddChild(Size));
    FitSlot->SetHorizontalAlignment(HAlign_Center);
    FitSlot->SetVerticalAlignment(VAlign_Center);
    auto* Panel = WidgetTree->ConstructWidget<UCanvasPanel>();
    SettingsContent = Panel;
    Size->AddChild(Panel);
    SettingsPanelArt = WidgetTree->ConstructWidget<USFOptionsPresentation>();
    SettingsPanelArt->SetVisibility(ESlateVisibility::HitTestInvisible);
    SettingsPanelArt->Controls = {ResolutionComboBox, FullScreenComboBox, MouseSensitivitySlider,
        MasterVolumeSlider, BGMVolumeSlider, SXVolumeSlider};
    SettingsPanelArt->Sliders = {MouseSensitivitySlider, MasterVolumeSlider, BGMVolumeSlider, SXVolumeSlider};
    Fill(Panel->AddChildToCanvas(SettingsPanelArt));
    const auto Cream = Hex(TEXT("FFF0CB")), Muted = Hex(TEXT("A6C6B6")), Gold = Hex(TEXT("D5AF68")), Aqua = Hex(TEXT("36E3CE"));
    auto Place = [&](UWidget* Control, float X, float Y, float W, float H)
    {
        auto* LayoutSlot = Panel->AddChildToCanvas(Control);
        LayoutSlot->SetPosition(FVector2D(X,Y)); LayoutSlot->SetSize(FVector2D(W,H));
    };
    auto Label = [&](FName Name, FText Text, int32 FontSize, FLinearColor Color,
        float X, float Y, float W, float H, bool Center = false, bool Bold = false)
    {
        auto* TextWidget = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
        TextWidget->SetText(Text);
        TextWidget->SetFont(FSlateFontInfo(Bold ? SettingsTitleFont.Get() : SettingsFont.Get(), FontSize));
        TextWidget->SetColorAndOpacity(Color);
        TextWidget->SetJustification(Center ? ETextJustify::Center : ETextJustify::Left);
        TextWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
        Place(TextWidget,X,Y,W,H);
        return TextWidget;
    };
    Label(TEXT("SettingsTitle"), NSLOCTEXT("SFSettings","Title","설정"), 44, Cream, 400,183,440,66,true,true);
    Label(TEXT("SettingsSubtitle"), NSLOCTEXT("SFSettings","RefinedSubtitle","더 편안한 플레이를 위한 나만의 설정"), 18, Muted, 280,253,680,32,true);
    Label(TEXT("DisplaySection"), NSLOCTEXT("SFSettings","DisplaySection","화면 및 조작"), 30, Cream, 166,295,370,48,false,true);
    Label(TEXT("AudioSection"), NSLOCTEXT("SFSettings","AudioSection","사운드"), 30, Cream, 730,295,370,48,false,true);
    Label(TEXT("DisplayHelp"), NSLOCTEXT("SFSettings","DisplayHelp","화면 크기와 마우스 움직임"), 14, Muted, 166,342,370,24);
    Label(TEXT("AudioHelp"), NSLOCTEXT("SFSettings","AudioHelp","전체 음량과 개별 소리 조절"), 14, Muted, 730,342,370,24);
    auto Combo = [&](UComboBoxString* Control, FName Name, FText Caption, float Y)
    {
        Label(Name,Caption,22,Cream,118,Y,420,38);
        FComboBoxStyle Style = Control->GetWidgetStyle();
        FButtonStyle Button = Style.ComboButtonStyle.ButtonStyle;
        Button.SetNormal(FSlateRoundedBoxBrush(Hex(TEXT("032B27")), 3.f, Gold.CopyWithNewOpacity(.7f), 1.f));
        Button.SetHovered(FSlateRoundedBoxBrush(Hex(TEXT("09584C")), 3.f, Aqua, 1.5f));
        Button.SetPressed(FSlateRoundedBoxBrush(Hex(TEXT("06433B")), 3.f, Cream, 1.5f));
        Button.SetNormalPadding(FMargin(4,0)).SetPressedPadding(FMargin(4,0));
        Style.ComboButtonStyle.SetButtonStyle(Button);
        Style.ComboButtonStyle.SetMenuBorderBrush(FSlateRoundedBoxBrush(Hex(TEXT("043D34")), 3.f, Gold, 1.f));
        Style.ComboButtonStyle.SetMenuBorderPadding(FMargin(5));
        Control->SetWidgetStyle(Style);
        FTableRowStyle ItemStyle = Control->GetItemStyle();
        const FSlateRoundedBoxBrush Highlight(Hex(TEXT("106C59")), 2.f);
        ItemStyle.SetActiveBrush(Highlight).SetActiveHoveredBrush(Highlight).SetInactiveBrush(Highlight)
            .SetInactiveHoveredBrush(Highlight).SetEvenRowBackgroundHoveredBrush(Highlight).SetOddRowBackgroundHoveredBrush(Highlight);
        Control->SetItemStyle(ItemStyle);
        Control->SetContentPadding(FMargin(8,0,28,0));
        Control->SetMaxListHeight(240);
        Control->SetHasDownArrow(false);
        Control->OnGenerateWidgetEvent.BindDynamic(this,&ThisClass::GenerateOptionLabel);
        Control->SetVisibility(ESlateVisibility::Visible);
        Place(Control,118,Y+44,440,44);
        auto* Arrow = WidgetTree->ConstructWidget<UImage>();
        FSlateBrush Brush = *FCoreStyle::Get().GetBrush("ComboButton.Arrow"); Brush.TintColor = Gold;
        Arrow->SetBrush(Brush); Arrow->SetVisibility(ESlateVisibility::HitTestInvisible);
        Place(Arrow,530,Y+62,12,8);
    };
    Combo(ResolutionComboBox,TEXT("ResolutionLabel"),NSLOCTEXT("SFSettings","Resolution","해상도"),394);
    Combo(FullScreenComboBox,TEXT("ScreenModeLabel"),NSLOCTEXT("SFSettings","ScreenMode","화면 모드"),496);
    ResolutionComboBox->SetToolTipText(NSLOCTEXT("SFSettings","ResolutionTip","화면에 표시할 가로·세로 픽셀 수를 선택합니다."));
    FullScreenComboBox->SetToolTipText(NSLOCTEXT("SFSettings","ModeTip","전체 화면, 테두리 없는 전체 화면, 창 모드 중 선택합니다."));
    auto Slider = [&](USlider* Control, FName Name, FText Caption, float X, float Y, FText Low, FText High)
    {
        Label(Name,Caption,22,Cream,X,Y,300,38);
        auto* Value = Label(FName(*(Name.ToString()+TEXT("Value"))),FText::GetEmpty(),22,Aqua,X+330,Y,110,38);
        Value->SetJustification(ETextJustify::Right);
        FSliderStyle Style;
        Style.SetNormalBarImage(FSlateNoResource()).SetHoveredBarImage(FSlateNoResource()).SetDisabledBarImage(FSlateNoResource());
        // The painter owns the handle. Zero native thumb width keeps pointer values
        // aligned exactly with the painted track endpoints (SSlider otherwise insets).
        FSlateBrush Thumb = FSlateNoResource(); Thumb.ImageSize = FVector2D(0,28);
        Style.SetNormalThumbImage(Thumb).SetHoveredThumbImage(Thumb).SetDisabledThumbImage(Thumb);
        Control->SetWidgetStyle(Style);
        Control->SetIndentHandle(false);
        Control->SetStepSize(Control == MouseSensitivitySlider ? .05f : .01f);
        Control->SetVisibility(ESlateVisibility::Visible);
        // A wide 44 px hit target surrounds the painted track and diamond handle.
        Place(Control,X,Y+31,440,44);
        Label(FName(*(Name.ToString()+TEXT("Low"))),Low,13,Muted,X,Y+72,150,24);
        auto* HighLabel = Label(FName(*(Name.ToString()+TEXT("High"))),High,13,Muted,X+290,Y+72,150,24);
        HighLabel->SetJustification(ETextJustify::Right);
        ReadoutSliders.Add(Control); ValueLabels.Add(Value);
    };
    Slider(MouseSensitivitySlider,TEXT("SensitivityLabel"),NSLOCTEXT("SFSettings","Sensitivity","마우스 감도"),118,600,
        NSLOCTEXT("SFSettings","Slower","느리게"),NSLOCTEXT("SFSettings","Faster","빠르게"));
    Slider(MasterVolumeSlider,TEXT("MasterVolumeLabel"),NSLOCTEXT("SFSettings","MasterVolume","전체 음량"),680,396,
        NSLOCTEXT("SFSettings","Mute","음소거"),NSLOCTEXT("SFSettings","Max","최대"));
    Slider(BGMVolumeSlider,TEXT("MusicVolumeLabel"),NSLOCTEXT("SFSettings","MusicVolumeFull","배경 음악"),680,498,
        NSLOCTEXT("SFSettings","Mute","음소거"),NSLOCTEXT("SFSettings","Max","최대"));
    Slider(SXVolumeSlider,TEXT("EffectsVolumeLabel"),NSLOCTEXT("SFSettings","EffectsVolume","효과음"),680,600,
        NSLOCTEXT("SFSettings","Mute","음소거"),NSLOCTEXT("SFSettings","Max","최대"));
    MasterVolumeSlider->SetToolTipText(NSLOCTEXT("SFSettings","MasterTip","배경 음악과 효과음을 포함한 모든 소리의 크기입니다."));
    MouseSensitivitySlider->SetToolTipText(NSLOCTEXT("SFSettings","MouseTip","낮추면 마우스가 천천히, 높이면 빠르게 움직입니다."));
    FButtonStyle BackStyle = BackUi_Btn->GetStyle();
    BackStyle.SetNormal(FSlateNoResource()).SetHovered(FSlateNoResource()).SetPressed(FSlateNoResource());
    BackStyle.SetNormalPadding(FMargin(0)).SetPressedPadding(FMargin(0,2,0,0));
    BackUi_Btn->SetStyle(BackStyle);
    BackUi_Btn->SetBackgroundColor(FLinearColor::White); BackUi_Btn->SetColorAndOpacity(FLinearColor::White);
    BackUi_Btn->SetVisibility(ESlateVisibility::Visible);
    auto* Content = WidgetTree->ConstructWidget<UOverlay>(); Content->SetVisibility(ESlateVisibility::HitTestInvisible);
    BackUi_Btn->SetContent(Content);
    auto* ButtonSlot = Cast<UButtonSlot>(Content->Slot);
    ButtonSlot->SetHorizontalAlignment(HAlign_Fill); ButtonSlot->SetVerticalAlignment(VAlign_Fill); ButtonSlot->SetPadding(FMargin(0));
    auto* Art = WidgetTree->ConstructWidget<USFResultPresentation>(); Art->bExitButtonDecoration = true; Art->ExitButton = BackUi_Btn;
    auto* ArtSlot = Content->AddChildToOverlay(Art); ArtSlot->SetHorizontalAlignment(HAlign_Fill); ArtSlot->SetVerticalAlignment(VAlign_Fill);
    auto* Caption = WidgetTree->ConstructWidget<UTextBlock>();
    Caption->SetText(NSLOCTEXT("SFSettings","Back","돌아가기")); Caption->SetFont(FSlateFontInfo(SettingsTitleFont,24)); Caption->SetColorAndOpacity(Cream);
    auto* CaptionSlot = Content->AddChildToOverlay(Caption); CaptionSlot->SetHorizontalAlignment(HAlign_Center); CaptionSlot->SetVerticalAlignment(VAlign_Center);
    Place(BackUi_Btn,870,756,280,70);
    Label(TEXT("SettingsHint"),NSLOCTEXT("SFSettings","SavedHint","변경 즉시 적용 · 자동 저장"),17,Muted,136,751,650,34);
    Label(TEXT("SettingsShortcut"),NSLOCTEXT("SFSettings","Shortcut","ESC   돌아가기"),14,Gold,136,788,460,27);
    Label(TEXT("SettingsSignature"),FText::FromString(TEXT("E T H E R   O A T H")),12,Gold.CopyWithNewOpacity(.65f),400,871,440,24,true);
}

void USettingWidget::UpdateSettingsReadouts()
{
    for (int32 I=0; I<ReadoutSliders.Num(); ++I)
    {
        auto* Control = ReadoutSliders[I].Get();
        const float Value = Control->GetValue();
        ValueLabels[I]->SetText(FText::FromString(Control == MouseSensitivitySlider
            ? FString::Printf(TEXT("%.2fx"),Value) : FString::Printf(TEXT("%d%%"),FMath::RoundToInt(Value*100))));
    }
}

void USettingWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
    Super::NativeTick(Geometry,DeltaTime);
    if (!bSettingsOpening || !SettingsContent || !SettingsPanelArt) return;
    const float T = FMath::Clamp((FSlateApplication::Get().GetCurrentTime()-SettingsPanelArt->StartTime)/.28,0.,1.);
    const float Ease = 1.f-FMath::Pow(1.f-T,3.f);
    SettingsContent->SetRenderOpacity(Ease);
    SettingsContent->SetRenderTranslation(FVector2D(0,(1-Ease)*14));
    bSettingsOpening = T<1.f;
}

FReply USettingWidget::NativeOnMouseButtonDown(const FGeometry&,const FPointerEvent&)
{ return FReply::Handled(); }
