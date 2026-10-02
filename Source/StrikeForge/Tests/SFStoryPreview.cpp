#if WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Button.h"
#include "Engine/Engine.h"
#include "Game/GameInstance/SFGameInstance.h"
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
    TEXT("SF.Story.Preview"), TEXT("SF.Story.Preview [folder] [resolution] [stage3]: capture the chronicle and optionally verify Stage 3 unlock, then quit."),
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
        const bool bVerifyUnlock=Args.Num()>2 && Args[2]==TEXT("stage3");
        World->GetTimerManager().SetTimer(Ready, FTimerDelegate::CreateWeakLambda(World, [World, Folder,bVerifyUnlock]()
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
            UE_LOG(LogTemp, Display, TEXT("STORY_QA available chapters=%d"), Story->GetAvailableChapterCount());
            auto After = [World](float Time, FTimerDelegate Action)
            {
                FTimerHandle Timer;
                World->GetTimerManager().SetTimer(Timer, Action, Time, false);
            };
            auto Shot = [Folder](const FString& Name)
            { FScreenshotRequest::RequestScreenshot(Folder/(Name+TEXT(".png")), true, false); };
            if(bVerifyUnlock)
            {
                USFGameInstance* Progress=World->GetGameInstance<USFGameInstance>();
                if(!Progress || Progress->GetHighestClearedStageOrder()!=0)
                { UE_LOG(LogTemp,Error,TEXT("STORY_QA unlock verification needs a fresh campaign")); FPlatformMisc::RequestExit(false); return; }
                auto Check=[](bool bPassed,const TCHAR* Name)
                {
                    if(bPassed) { UE_LOG(LogTemp,Display,TEXT("STORY_QA PASS %s"),Name); }
                    else { UE_LOG(LogTemp,Error,TEXT("STORY_QA FAIL %s"),Name); }
                };
                Check(Story->GetAvailableChapterCount()==5,TEXT("new campaign has five chapters"));
                Check(Progress->MarkStageCleared(TEXT("Stage01")),TEXT("clear stage 1"));
                Story->OpenStory();
                Check(Story->GetAvailableChapterCount()==5,TEXT("stage 1 keeps expansion locked"));
                Check(Progress->MarkStageCleared(TEXT("Stage02")),TEXT("clear stage 2"));
                Story->OpenStory();
                Check(Progress->GetStageProgressState(TEXT("Stage03"))==ESFStageProgressState::Unlocked,TEXT("stage 3 is playable but uncleared"));
                Check(Story->GetAvailableChapterCount()==5,TEXT("stage 2 keeps expansion locked"));
                Story->PreviewChapter(99);
                Check(Story->GetChapterIndex()==4,TEXT("locked chapters cannot be reached"));
                After(1.f,FTimerDelegate::CreateLambda([Shot] { Shot(TEXT("locked_after_stage2")); }));
                After(1.5f,FTimerDelegate::CreateWeakLambda(Story,[Story] { Story->TurnPage(1); }));
                After(2.3f,FTimerDelegate::CreateWeakLambda(Story,[Story,Check]
                { Check(!Story->IsInViewport() && !Story->HasLiveIllustration(),TEXT("locked last page closes and cleans up")); }));
                After(2.5f,FTimerDelegate::CreateWeakLambda(Button,[Story,Button,Progress,Check]
                {
                    Check(Progress->MarkStageCleared(TEXT("Stage03")),TEXT("clear stage 3"));
                    Button->OnClicked.Broadcast();
                    Check(Story->IsInViewport() && Story->GetAvailableChapterCount()==8,TEXT("same modal reopens with eight chapters"));
                }));
                for(int32 Page=5;Page<8;++Page)
                {
                    const float Time=4.5f+(Page-5)*2.5f;
                    After(Time,FTimerDelegate::CreateWeakLambda(Story,[Story,Page,Check]
                    {
                        Story->PreviewChapter(Page-1);
                        Story->TurnPage(1);
                        Check(Story->GetChapterIndex()==Page && !Story->IsClosing(),TEXT("navigate into unlocked chapter"));
                    }));
                    After(Time+1.2f,FTimerDelegate::CreateLambda([Shot,Page] { Shot(FString::Printf(TEXT("unlocked_chapter%d"),Page+1)); }));
                }
                After(11.5f,FTimerDelegate::CreateWeakLambda(Story,[Story,Check]
                { Story->TurnPage(1); Check(Story->IsClosing(),TEXT("chapter 8 closes the story")); }));
                After(12.3f,FTimerDelegate::CreateWeakLambda(Story,[Story,Check]
                { Check(!Story->IsInViewport() && !Story->HasLiveIllustration(),TEXT("expanded story closes and cleans up")); }));
                After(12.5f,FTimerDelegate::CreateWeakLambda(Button,[Story,Button,Check]
                {
                    Button->OnClicked.Broadcast();
                    Check(Story->GetAvailableChapterCount()==8 && Story->GetChapterIndex()==0,TEXT("unlock retained on next open"));
                }));
                After(14.5f,FTimerDelegate::CreateWeakLambda(Story,[Story,Check]
                {
                    Story->TurnPage(-1);
                    Check(Story->GetChapterIndex()==0,TEXT("previous does not go below chapter 1"));
                    Story->CloseStory();
                }));
                After(15.5f,FTimerDelegate::CreateLambda([] { FPlatformMisc::RequestExit(false); }));
                return;
            }
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
            const int32 Count=Story->GetAvailableChapterCount();
            for (int32 Page=1; Page<Count; ++Page)
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
            const float End=14.f+(Count-5)*2.6f;
            After(End, FTimerDelegate::CreateWeakLambda(Story, [Story]()
            {
                FKeyEvent Key(EKeys::Escape, FModifierKeysState(), 0, false, 0, 0);
                FSlateApplication::Get().ProcessKeyDownEvent(Key);
                UE_LOG(LogTemp, Display, TEXT("STORY_QA Escape closing=%d"),Story->IsClosing());
            }));
            After(End+.25f, FTimerDelegate::CreateLambda([Shot] { Shot(TEXT("07_rolling_closed")); }));
            After(End+1.f, FTimerDelegate::CreateWeakLambda(Button, [Story,Button,Shot]()
            {
                UE_LOG(LogTemp, Display, TEXT("STORY_QA closed viewport=%d restoredFocus=%d"), Story->IsInViewport(), Button->HasKeyboardFocus());
                UE_LOG(LogTemp, Display, TEXT("STORY_QA closed Niagara=%d"), Story->HasLiveIllustration());
                Shot(TEXT("08_return_to_lobby"));
            }));
            After(End+1.2f, FTimerDelegate::CreateWeakLambda(Button, [Button]() { Button->OnClicked.Broadcast(); }));
            After(End+2.9f, FTimerDelegate::CreateWeakLambda(Story, [Story,Shot]()
            {
                UE_LOG(LogTemp, Display, TEXT("STORY_QA reopened viewport=%d chapter=%d focus=%d"),Story->IsInViewport(),Story->GetChapterIndex(),Story->HasKeyboardFocus());
                Shot(TEXT("09_reopened"));
            }));
            After(End+3.2f, FTimerDelegate::CreateWeakLambda(Story, [Story]()
            {
                Story->PreviewChapter(Story->GetAvailableChapterCount()-1);
                Story->TurnPage(1);
                UE_LOG(LogTemp, Display, TEXT("STORY_QA last-page close=%d"),Story->IsClosing());
            }));
            After(End+4.2f, FTimerDelegate::CreateLambda([] { FPlatformMisc::RequestExit(false); }));
        }), 7.f, false);
    }));

#endif
