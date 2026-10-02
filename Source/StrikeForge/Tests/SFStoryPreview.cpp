#if WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Button.h"
#include "Engine/Engine.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/IConsoleManager.h"
#include "Layout/WidgetPath.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "UI/LobbyWidget.h"
#include "UI/SFStoryWidget.h"
#include "UnrealClient.h"

// Exercise the actual lobby button, all chapters, Escape, and modal reuse.
static FAutoConsoleCommandWithWorldAndArgs SFStoryPreview(
    TEXT("SF.Story.Preview"), TEXT("SF.Story.Preview [folder]: capture and verify the lobby chronicle, then quit."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
    {
        if (!World) return;
        const FString Folder = FPaths::ProjectSavedDir() / TEXT("StoryPreview") /
            (Args.IsEmpty() ? TEXT("FullHD") : FPaths::GetCleanFilename(Args[0]));
        // The saved game settings override startup ResX/ResY; resize after initialization.
        FTimerHandle Resize;
        const FString Resolution = Args.Num()>1 ? Args[1] : TEXT("1920x1080w");
        World->GetTimerManager().SetTimer(Resize, FTimerDelegate::CreateWeakLambda(World, [World,Resolution]()
        { if (GEngine) GEngine->Exec(World, *(TEXT("r.SetRes ")+Resolution)); }), 2.f, false);
        FTimerHandle Ready;
        World->GetTimerManager().SetTimer(Ready, FTimerDelegate::CreateWeakLambda(World, [World, Folder]()
        {
            TArray<UUserWidget*> Lobbies;
            UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Lobbies, ULobbyWidget::StaticClass(), true);
            UButton* Button = Lobbies.IsEmpty() ? nullptr : Cast<UButton>(Lobbies[0]->GetWidgetFromName(TEXT("Story_Btn")));
            if (!Button) { UE_LOG(LogTemp, Error, TEXT("STORY_QA missing lobby story button")); FPlatformMisc::RequestExit(false); return; }
            Button->OnClicked.Broadcast();
            TArray<UUserWidget*> Stories;
            UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Stories, USFStoryWidget::StaticClass(), true);
            USFStoryWidget* Story = Stories.IsEmpty() ? nullptr : Cast<USFStoryWidget>(Stories[0]);
            if (!Story) { UE_LOG(LogTemp, Error, TEXT("STORY_QA button did not open chronicle")); FPlatformMisc::RequestExit(false); return; }
            UE_LOG(LogTemp, Display, TEXT("STORY_QA open count=%d focus=%d"), Stories.Num(), Story->HasKeyboardFocus());
            Button->OnClicked.Broadcast();
            Stories.Reset();
            UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Stories, USFStoryWidget::StaticClass(), true);
            UE_LOG(LogTemp, Display, TEXT("STORY_QA duplicate click count=%d"), Stories.Num());
            UE_LOG(LogTemp, Display, TEXT("STORY_QA live Niagara=%d"), Story->HasLiveIllustration());
            auto After = [World](float Time, FTimerDelegate Action)
            {
                FTimerHandle Timer;
                World->GetTimerManager().SetTimer(Timer, Action, Time, false);
            };
            auto Shot = [Folder](const FString& Name)
            { FScreenshotRequest::RequestScreenshot(Folder/(Name+TEXT(".png")), true, false); };
            After(.06f, FTimerDelegate::CreateLambda([Shot] { Shot(TEXT("00_rolled")); }));
            After(.60f, FTimerDelegate::CreateLambda([Shot] { Shot(TEXT("01_unrolling")); }));
            After(2.f, FTimerDelegate::CreateLambda([Shot] { Shot(TEXT("02_chapter1")); }));
            After(2.6f, FTimerDelegate::CreateWeakLambda(Story, [Story]()
            {
                bool bContained = true;
                for (int32 I=0; I<8; ++I)
                {
                    FKeyEvent Tab(EKeys::Tab, FModifierKeysState(), 0, false, 0, 0);
                    FSlateApplication::Get().ProcessKeyDownEvent(Tab);
                    const TSharedPtr<SWidget> Focused = FSlateApplication::Get().GetUserFocusedWidget(0);
                    FWidgetPath Path;
                    bContained &= Focused.IsValid() && FSlateApplication::Get().GeneratePathToWidgetUnchecked(Focused.ToSharedRef(),Path)
                        && Path.ContainsWidget(&Story->TakeWidget().Get());
                }
                UE_LOG(LogTemp, Display, TEXT("STORY_QA Tab modal containment=%d"),bContained);
                Story->SetFocus();
            }));
            for (int32 Page=1; Page<5; ++Page)
            {
                After(1.4f+Page*2.6f, FTimerDelegate::CreateWeakLambda(Story, [Story,Page]()
                {
                    FKeyEvent Key(EKeys::Right, FModifierKeysState(), 0, false, 0, 0);
                    FSlateApplication::Get().ProcessKeyDownEvent(Key);
                    UE_LOG(LogTemp, Display, TEXT("STORY_QA keyboard page expected=%d actual=%d"), Page, Story->GetChapterIndex());
                }));
                After(2.5f+Page*2.6f, FTimerDelegate::CreateLambda([Shot,Page] { Shot(FString::Printf(TEXT("%02d_chapter%d"),Page+2,Page+1)); }));
                After(3.2f+Page*2.6f, FTimerDelegate::CreateLambda([Shot,Page] { Shot(FString::Printf(TEXT("motion_chapter%d"),Page+1)); }));
            }
            After(14.f, FTimerDelegate::CreateWeakLambda(Story, [Story]()
            {
                FKeyEvent Key(EKeys::Escape, FModifierKeysState(), 0, false, 0, 0);
                FSlateApplication::Get().ProcessKeyDownEvent(Key);
                UE_LOG(LogTemp, Display, TEXT("STORY_QA Escape closing=%d"),Story->IsClosing());
            }));
            After(14.25f, FTimerDelegate::CreateLambda([Shot] { Shot(TEXT("07_rolling_closed")); }));
            After(15.f, FTimerDelegate::CreateWeakLambda(Button, [Story,Button,Shot]()
            {
                UE_LOG(LogTemp, Display, TEXT("STORY_QA closed viewport=%d restoredFocus=%d"), Story->IsInViewport(), Button->HasKeyboardFocus());
                UE_LOG(LogTemp, Display, TEXT("STORY_QA closed Niagara=%d"), Story->HasLiveIllustration());
                Shot(TEXT("08_return_to_lobby"));
            }));
            After(15.2f, FTimerDelegate::CreateWeakLambda(Button, [Button]() { Button->OnClicked.Broadcast(); }));
            After(16.9f, FTimerDelegate::CreateWeakLambda(Story, [Story,Shot]()
            {
                UE_LOG(LogTemp, Display, TEXT("STORY_QA reopened viewport=%d chapter=%d focus=%d"),Story->IsInViewport(),Story->GetChapterIndex(),Story->HasKeyboardFocus());
                Shot(TEXT("09_reopened"));
            }));
            After(17.2f, FTimerDelegate::CreateWeakLambda(Story, [Story]()
            {
                Story->PreviewChapter(4);
                Story->TurnPage(1);
                UE_LOG(LogTemp, Display, TEXT("STORY_QA last-page close=%d"),Story->IsClosing());
            }));
            After(18.2f, FTimerDelegate::CreateLambda([] { FPlatformMisc::RequestExit(false); }));
        }), 7.f, false);
    }));

#endif
