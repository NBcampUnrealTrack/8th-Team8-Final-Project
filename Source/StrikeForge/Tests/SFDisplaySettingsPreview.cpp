#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "HAL/IConsoleManager.h"
#include "UI/SettingWidget.h"
#include "InGame/Controller/SFInGamePlayerController.h"
#include "InGame/SFInGameHUDWidget.h"
#include "InGame/HUD/SFCommanderSkillTooltipWidget.h"
#include "Units/Commander/SFCommander.h"
#include "Components/ComboBoxString.h"
#include "Components/Image.h"
#include "GameFramework/GameUserSettings.h"
#include "Engine/GameViewportClient.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "UnrealClient.h"

// Opt-in rendered regression: use in a disposable game/PIE session with a commander.
// Exercises the real combo delegates while the HUD's SceneCapture portrait is active.
static FAutoConsoleCommandWithWorld DisplaySettingsCheck(TEXT("SF.Display.Check"),
    TEXT("After 15 seconds, cycle display settings, restore them, and capture skill tooltips."),
    FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
    {
        if (!World || !World->IsGameWorld()) return;
        FTimerHandle Ready;
        World->GetTimerManager().SetTimer(Ready, FTimerDelegate::CreateWeakLambda(World, [World]()
        {
            auto* PC = World->GetFirstPlayerController<ASFInGamePlayerController>();
            auto* HUD = PC ? PC->GetInGameHUDWidget() : nullptr;
            auto* Commander = PC ? PC->GetMyCommander() : nullptr;
            if (!HUD || !Commander)
            {
                UE_LOG(LogTemp, Error, TEXT("DISPLAY_CHECK requires a live commander and HUD"));
                return;
            }
            HUD->SelectActor(Commander);
            HUD->RefreshCommanderSkillIcons();
            auto* Class = LoadClass<USettingWidget>(nullptr, TEXT("/Game/Core/UI/WBP_Setting.WBP_Setting_C"));
            auto* Widget = CreateWidget<USettingWidget>(PC, Class);
            if (!Widget) return;
            Widget->AddToViewport(100);
            Widget->OpenSetting();
            auto* Resolution = Cast<UComboBoxString>(Widget->GetWidgetFromName(TEXT("ResolutionComboBox")));
            auto* Mode = Cast<UComboBoxString>(Widget->GetWidgetFromName(TEXT("FullScreenComboBox")));
            if (!Resolution || !Mode) { Widget->RemoveFromParent(); return; }
            auto* Settings = UGameUserSettings::GetGameUserSettings();
            const FIntPoint OriginalResolution = Settings->GetScreenResolution();
            const EWindowMode::Type OriginalMode = Settings->GetFullscreenMode();
            const FString Folder = FPaths::ProjectSavedDir()/TEXT("DisplayValidation");
            auto Report = MakeShared<FString>();
            const TArray<FString> Values = {TEXT("1280x720"), TEXT("1920x1080"), TEXT("1600x900"), TEXT("1280x720")};
            for (int32 Index = 0; Index < 7; ++Index)
            {
                FTimerHandle Step;
                World->GetTimerManager().SetTimer(Step, FTimerDelegate::CreateWeakLambda(Widget,
                    [World, PC, HUD, Widget, Resolution, Mode, Settings, OriginalResolution, OriginalMode, Folder, Report, Values, Index]()
                {
                    const FIntPoint ViewportSize = World->GetGameViewport()->Viewport->GetSizeXY();
                    if (Index > 0 && Index <= Values.Num())
                    {
                        const FString Actual = FString::Printf(TEXT("%dx%d"), ViewportSize.X, ViewportSize.Y);
                        *Report += FString::Printf(TEXT("%s rendered viewport %s (expected %s)\n"),
                            Actual == Values[Index - 1] ? TEXT("PASS") : TEXT("FAIL"), *Actual, *Values[Index - 1]);
                    }
                    if (Index == 0) Mode->OnSelectionChanged.Broadcast(TEXT("Windowed"), ESelectInfo::OnMouseClick);
                    if (Index < Values.Num()) Resolution->OnSelectionChanged.Broadcast(Values[Index], ESelectInfo::OnMouseClick);
                    else if (Index == 4) Mode->OnSelectionChanged.Broadcast(TEXT("WindowedFullscreen"), ESelectInfo::OnMouseClick);
                    else if (Index == 5) Mode->OnSelectionChanged.Broadcast(TEXT("Fullscreen"), ESelectInfo::OnMouseClick);
                    else
                    {
                        Settings->SetScreenResolution(OriginalResolution);
                        Settings->SetFullscreenMode(OriginalMode);
                        Settings->ApplyResolutionSettings(false);
                        Settings->SaveSettings();
                        Widget->RemoveFromParent();
                        for (int32 Tier : {1, 3})
                        {
                            UWidget* Slot = HUD->GetTutorialCommanderSkillWidget(Tier);
                            auto* Panel = Slot ? Slot->GetTypedOuter<UUserWidget>() : nullptr;
                            auto* Icon = Panel ? Cast<UImage>(Panel->GetWidgetFromName(FName(*FString::Printf(TEXT("SkillIcon%02d"), Tier)))) : nullptr;
                            auto* Tip = Icon ? Cast<USFCommanderSkillTooltipWidget>(Icon->GetToolTip()) : nullptr;
                            *Report += FString::Printf(TEXT("%s custom tooltip tier %d\n"), Tip ? TEXT("PASS") : TEXT("FAIL"), Tier);
                            if (Tip)
                            {
                                Tip->AddToViewport(200);
                                Tip->SetPositionInViewport(FVector2D(30.f + (Tier == 3 ? 400.f : 0.f), 220.f), false);
                            }
                        }
                        FTimerHandle Shot;
                        World->GetTimerManager().SetTimer(Shot, FTimerDelegate::CreateWeakLambda(World, [World, Folder, Report, OriginalResolution]()
                        {
                            const FIntPoint Actual = World->GetGameViewport()->Viewport->GetSizeXY();
                            *Report += FString::Printf(TEXT("%s restored viewport %s\n"), Actual == OriginalResolution ? TEXT("PASS") : TEXT("FAIL"), *Actual.ToString());
                            FFileHelper::SaveStringToFile(*Report, *(Folder/TEXT("result.txt")));
                            FScreenshotRequest::RequestScreenshot(Folder/TEXT("tooltips.png"), true, false);
                        }), 2.f, false);
                    }
                    *Report += FString::Printf(TEXT("step %d requested=%s mode=%d\n"), Index,
                        *Settings->GetScreenResolution().ToString(), static_cast<int32>(Settings->GetFullscreenMode()));
                    UE_LOG(LogTemp, Display, TEXT("DISPLAY_CHECK completed step %d"), Index);
                }), 2.f + Index * 3.f, false);
            }
        }), 15.f, false);
    }));
#endif
