#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "HAL/IConsoleManager.h"
#include "Editor.h"
#include "PlayInEditorDataTypes.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Containers/Ticker.h"
#include "Layout/WidgetPath.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Framework/Application/SlateApplication.h"
#include "InGame/HUD/Settings/SFSettingsWidget.h"
#include "UI/SettingWidget.h"
#include "UI/LobbyWidget.h"
#include "UI/MouseSensitivitySubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Widgets/SWindow.h"
#include "UnrealClient.h"

static FAutoConsoleCommand PauseMenuStart(TEXT("SF.Menu.PauseStart"),
    TEXT("Start a disposable standalone PIE menu check without modifying multiplayer preferences."),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        if (!GEditor || GEditor->PlayWorld) return;
        FRequestPlaySessionParams Params;
        Params.EditorPlaySettings = DuplicateObject(GetDefault<ULevelEditorPlaySettings>(), GetTransientPackage());
        Params.EditorPlaySettings->SetPlayNetMode(PIE_Standalone);
        Params.EditorPlaySettings->SetPlayNumberOfClients(1);
        Params.EditorPlaySettings->SetRunUnderOneProcess(true);
        Params.EditorPlaySettings->bLaunchSeparateServer = false;
        Params.EditorPlaySettings->NewWindowWidth = 1600;
        Params.EditorPlaySettings->NewWindowHeight = 900;
        Params.EditorPlaySettings->CenterNewWindow = true;
        GEditor->RequestPlaySession(Params);
    }));

// Opt-in input probe used by Scripts/Editor/verify_pause_menu.py in disposable PIE.
// Route through Slate, exercising focus and hit testing instead of calling handlers.
static FAutoConsoleCommandWithWorldAndArgs PauseMenuInput(TEXT("SF.Menu.PauseInput"),
    TEXT("Active pause menu input: Escape, F10, Tab, Enter, hover <button>, or click <button>."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
    {
        if (!World || !World->IsGameWorld() || Args.IsEmpty()) return;
        if (Args[0] == TEXT("resize") && Args.Num() > 2 && World->GetGameViewport())
        {
            if (auto Window = World->GetGameViewport()->GetWindow())
            {
                const FVector2D Desired(FCString::Atoi(*Args[1]), FCString::Atoi(*Args[2]));
                Window->Resize(Desired);
                const TWeakObjectPtr<UWorld> WeakWorld = World;
                FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakWorld, Desired](float)
                {
                    if (auto* W = WeakWorld.Get())
                        if (auto* Viewport = W->GetGameViewport())
                            if (auto Win = Viewport->GetWindow())
                                Win->Resize(Desired * 2. - FVector2D(Viewport->Viewport->GetSizeXY()));
                    return false;
                }), .3f);
            }
            return;
        }
        TArray<UUserWidget*> Widgets, Candidates;
        for (UClass* Class : {USettingWidget::StaticClass(), USFSettingsWidget::StaticClass(), ULobbyWidget::StaticClass()})
        {
            UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Candidates, Class, false);
            Widgets.Append(Candidates);
        }
        for (auto* Widget : Widgets)
        {
            auto* Menu = Widget;
            if (!Menu->IsVisible()) continue;
            if (auto* Pause = Cast<USFSettingsWidget>(Menu); Pause && !Pause->IsActivated()) continue;
            auto& Slate = FSlateApplication::Get();
            if (Args[0] == TEXT("inspect") && Args.Num() > 1)
            {
                FString Report;
                FWidgetPath RootPath;
                Slate.GeneratePathToWidgetUnchecked(Menu->TakeWidget(), RootPath);
                const bool bOptions = Menu->IsA<USettingWidget>();
                const TArray<FName> Names = bOptions
                    ? TArray<FName>{TEXT("ResolutionComboBox"), TEXT("FullScreenComboBox"), TEXT("MouseSensitivitySlider"), TEXT("MasterVolumeSlider"), TEXT("BGMVolumeSlider"), TEXT("SXVolumeSlider"), TEXT("BackUi_Btn")}
                    : TArray<FName>{TEXT("ContinueButton"), TEXT("SettingButton"), TEXT("ExitButton")};
                for (FName Name : Names)
                {
                    auto* Control = Menu->GetWidgetFromName(Name);
                    FWidgetPath Path;
                    bool bFits = Control && RootPath.IsValid() && Slate.GeneratePathToWidgetUnchecked(Control->TakeWidget(), Path);
                    FVector2D Position, Size;
                    if (bFits)
                    {
                        const auto& Root = RootPath.Widgets.Last().Geometry;
                        const auto& Geometry = Path.Widgets.Last().Geometry;
                        Position = Root.AbsoluteToLocal(Geometry.LocalToAbsolute(FVector2D::ZeroVector));
                        Size = Root.AbsoluteToLocal(Geometry.LocalToAbsolute(Geometry.GetLocalSize())) - Position;
                        bFits = Size.X > 100 && Size.Y > 30 && Position.X >= 0 && Position.Y >= 0
                            && Position.X + Size.X <= Root.GetLocalSize().X + 1 && Position.Y + Size.Y <= Root.GetLocalSize().Y + 1;
                    }
                    Report += FString::Printf(TEXT("%s %s position=%s size=%s\n"), bFits ? TEXT("PASS") : TEXT("FAIL"), *Name.ToString(), *Position.ToString(), *Size.ToString());
                }
                FFileHelper::SaveStringToFile(Report, *(FPaths::ProjectDir() / (bOptions ? TEXT("output/qa/settings-2026-10-19") : TEXT("output/qa/pause-menu-2026-10-19")) / (FPaths::GetCleanFilename(Args[1]) + TEXT(".txt"))));
                return;
            }
            if ((Args[0] == TEXT("hover") || Args[0] == TEXT("click")) && Args.Num() > 1)
            {
                UWidget* Button = Menu->GetWidgetFromName(FName(*Args[1]));
                if (!Button) continue;
                // Arrange the actual widget path: cached geometry can be empty in a
                // hidden/unattended editor, even though screenshot rendering works.
                FWidgetPath Path;
                TSharedRef<SWidget> InputTarget = Button->TakeWidget();
                if (Button->IsA<UComboBoxString>())
                {
                    // Combo boxes route mouse input through their nested Slate button.
                    TFunction<TSharedPtr<SWidget>(TSharedRef<SWidget>)> FindButton;
                    FindButton = [&FindButton](TSharedRef<SWidget> Node) -> TSharedPtr<SWidget>
                    {
                        if (Node->GetType() == FName(TEXT("SButton"))) return Node;
                        FChildren* Children = Node->GetChildren();
                        for (int32 I = 0; Children && I < Children->Num(); ++I)
                            if (auto Found = FindButton(Children->GetChildAt(I))) return Found;
                        return nullptr;
                    };
                    if (auto Found = FindButton(InputTarget)) InputTarget = Found.ToSharedRef();
                }
                if (!Slate.GeneratePathToWidgetUnchecked(InputTarget, Path)) return;
                const auto G = Path.Widgets.Last().Geometry;
                TArray<FWidgetAndPointer> PointerWidgets;
                for (int32 Index = 0; Index < Path.Widgets.Num(); ++Index)
                    PointerWidgets.Emplace(Path.Widgets[Index], TOptional<FVirtualPointerPosition>());
                Path = FWidgetPath(MakeArrayView(PointerWidgets));
                const FVector2D Cursor = G.LocalToAbsolute(G.GetLocalSize() * FVector2D(Args.Num() > 2 ? FCString::Atof(*Args[2]) : .5, .5));
                // A test teleports the OS cursor, unlike physical mouse input. Reset
                // the existing motion baseline so sensitivity does not amplify that warp
                // and move the cursor away between pointer down and pointer up.
                if (auto* GI = World->GetGameInstance())
                    if (auto* Sensitivity = GI->GetSubsystem<UMouseSensitivitySubsystem>())
                        Sensitivity->SetMouseSensitivity(Sensitivity->GetMouseSensitivity());
                Slate.SetCursorPos(Cursor);
                FPointerEvent Move(0, Cursor, Cursor, TSet<FKey>{}, EKeys::Invalid, 0, FModifierKeysState());
                Slate.RoutePointerMoveEvent(Path, Move, false);
                if (Args[0] == TEXT("click"))
                {
                    FPointerEvent Down(0, Cursor, Cursor, TSet<FKey>{EKeys::LeftMouseButton}, EKeys::LeftMouseButton, 0, FModifierKeysState());
                    Slate.RoutePointerDownEvent(Path, Down);
                    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Cursor, Path](float)
                    {
                        FPointerEvent Up(0, Cursor, Cursor, TSet<FKey>{}, EKeys::LeftMouseButton, 0, FModifierKeysState());
                        FSlateApplication::Get().RoutePointerUpEvent(Path, Up);
                        return false;
                    }), .1f);
                }
            }
            else if (Args[0] == TEXT("Escape") || Args[0] == TEXT("F10") || Args[0] == TEXT("Tab") || Args[0] == TEXT("Enter"))
            {
                const FKey Key(*Args[0]);
                const FKeyEvent Event(Key, FModifierKeysState(), 0, false, 0, 0);
                Slate.ProcessKeyDownEvent(Event);
                Slate.ProcessKeyUpEvent(Event);
            }
            return;
        }
    }));
#endif
