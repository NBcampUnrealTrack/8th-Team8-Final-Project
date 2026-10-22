#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Editor.h"
#include "PlayInEditorDataTypes.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "HAL/IConsoleManager.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "InGame/Controller/SFInGamePlayerController.h"
#include "InGame/SFInGameHUDWidget.h"
#include "InGame/SFSelectINfo/SFMultiSelectionEntryWidget.h"
#include "ActorComponent/Stat/HealthComponent.h"
#include "Units/Commander/SFCommander.h"
#include "Units/Unit/SFUnitSpawnLibrary.h"
#include "Game/GameState/SFGameState.h"
#include "Game/Subsystem/SFCombatSubsystem.h"
#include "Buildings/Building/HouseBuilding/Nexus/SFNexusBuilding.h"
#include "Layout/WidgetPath.h"
#include "UI/ResultWidget.h"
#include "Components/Button.h"
#include "Components/ProgressBar.h"
#include "Components/InputComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "UObject/UnrealType.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Containers/Ticker.h"
#include "UnrealClient.h"

// Explicit, disposable PIE verification. No map or user settings are saved.
static FAutoConsoleCommandWithWorldAndArgs HUDRevisionQA(TEXT("SF.HUD.QA"),
    TEXT("PIE HUD probe: start W H, resize W H, dump name, space, health, hover, win, lose, exit."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args,UWorld* World)
    {
        if (Args.IsEmpty()) return;
        const FString Mode=Args[0];
        if (Mode==TEXT("start") && GEditor && !GEditor->PlayWorld)
        {
            FRequestPlaySessionParams Params;
            Params.EditorPlaySettings=DuplicateObject(GetDefault<ULevelEditorPlaySettings>(),GetTransientPackage());
            Params.EditorPlaySettings->NewWindowWidth=Args.Num()>1?FCString::Atoi(*Args[1]):1600;
            Params.EditorPlaySettings->NewWindowHeight=Args.Num()>2?FCString::Atoi(*Args[2]):900;
            Params.EditorPlaySettings->CenterNewWindow=true;
            GEditor->RequestPlaySession(Params); return;
        }
        if (!World || !World->IsGameWorld()) return;
        auto* PC=USFCombatSubsystem::Get(World)->GetLocalPlayerController();
        auto* HUD=PC?PC->GetInGameHUDWidget():nullptr;
        if (!HUD) return;
        if (Mode==TEXT("selection"))
        {
            if(auto* State=World->GetGameState<ASFGameState>())
                if(auto* Nexus=State->GetNexus(PC->GetMyTeamIndex()))
                {
                    for(const TCHAR* Row:{TEXT("Brawler"),TEXT("Berserker"),TEXT("Brawler"),TEXT("Brawler")})
                        for(int32 Attempt=0;Attempt<12;++Attempt)
                            if(USFUnitSpawnLibrary::SpawnUnitFromRow(Nexus,FName(Row),650,1400))break;
                    HUD->SelectAllOwnedUnits();
                }
        }
        if (Mode==TEXT("window") && Args.Num()>2)
            if(auto Window=World->GetGameViewport()->GetWindow())
                Window->MoveWindowTo(FVector2D(FCString::Atof(*Args[1]),FCString::Atof(*Args[2])));
        if (Mode==TEXT("armresult")) HUD->ArmResultPreviewForDebug();
        if (Mode==TEXT("click") && Args.Num()>1)
        {
            for (TObjectIterator<UButton> It;It;++It)
                if (It->GetWorld()==World && It->GetName()==Args[1] && It->IsVisible() && It->GetIsEnabled())
                {
                    const auto G=It->GetCachedGeometry(); if (G.GetLocalSize().IsNearlyZero()) continue;
                    const FVector2D Cursor=G.LocalToAbsolute(G.GetLocalSize()*.5);
                    FSlateApplication::Get().SetCursorPos(Cursor);
                    FPointerEvent Down(0,Cursor,Cursor,TSet<FKey>{EKeys::LeftMouseButton},EKeys::LeftMouseButton,0,FModifierKeysState());
                    FSlateApplication::Get().ProcessMouseMoveEvent(Down);
                    FSlateApplication::Get().ProcessMouseButtonDownEvent(nullptr,Down);
                    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Cursor](float)
                    {
                        FPointerEvent Up(0,Cursor,Cursor,TSet<FKey>{},EKeys::LeftMouseButton,0,FModifierKeysState());
                        FSlateApplication::Get().ProcessMouseButtonUpEvent(Up); return false;
                    }),.15f);
                    break;
                }
        }
        if (Mode==TEXT("resize") && Args.Num()>2)
        {
            if(World->WorldType!=EWorldType::PIE)
            {
                PC->ConsoleCommand(FString::Printf(TEXT("r.SetRes %dx%dw"),FCString::Atoi(*Args[1]),FCString::Atoi(*Args[2])),false);
                return;
            }
            if (auto Window=World->GetGameViewport()->GetWindow())
            {
                const FVector2D Desired(FCString::Atoi(*Args[1]),FCString::Atoi(*Args[2]));
                // Account for native/virtual window chrome; verify the game pixels.
                Window->Resize(Desired);
                const TWeakObjectPtr<UWorld> WeakWorld=World;
                FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakWorld,Desired](float)
                {
                    if (auto* W=WeakWorld.Get())
                        if (auto* Viewport=W->GetGameViewport())
                            if (auto Win=Viewport->GetWindow())
                                Win->Resize(Desired*2.-FVector2D(Viewport->Viewport->GetSizeXY()));
                    return false;
                }),.3f);
            }
        }
        if (Mode==TEXT("space"))
        {
            FKeyEvent Key(EKeys::SpaceBar,FModifierKeysState(),0,false,0,0);
            FSlateApplication::Get().ProcessKeyDownEvent(Key);
            FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float)
            {
                FKeyEvent Release(EKeys::SpaceBar,FModifierKeysState(),0,false,0,0);
                FSlateApplication::Get().ProcessKeyUpEvent(Release); return false;
            }),.15f);
        }
        if (Mode==TEXT("health"))
        {
            int Index=0;
            for (AActor* Actor:HUD->GetSelectedActors())
                if (auto* Health=Actor->FindComponentByClass<UHealthComponent>())
                {
                    const float Ratio=Index++%3==0?.29f:Index%3==2?.3f:1.f;
                    Health->SetHealth(Health->GetMaxHealth(),Health->GetMaxHealth()*Ratio);
                }
        }
        if (Mode==TEXT("hover"))
            for (TObjectIterator<USFMultiSelectionEntryWidget> It;It;++It)
                if (It->GetWorld()==World && It->GetRepresentedActor() && It->GetSelectionIndex()==(Args.Num()>1?FCString::Atoi(*Args[1]):0))
                    if (auto* Button=Cast<UButton>(It->GetWidgetFromName(TEXT("EntryButton"))))
                    {
                        const auto G=Button->GetCachedGeometry();
                        const FVector2D Cursor=G.LocalToAbsolute(G.GetLocalSize()*.5);
                        FSlateApplication::Get().SetCursorPos(Cursor);
                        FPointerEvent Move(0,Cursor,Cursor,FSlateApplication::Get().GetPressedMouseButtons(),EKeys::Invalid,0,FModifierKeysState());
                        FWidgetPath Path;
                        if(FSlateApplication::Get().GeneratePathToWidgetUnchecked(Button->TakeWidget(),Path))
                        {
                            TArray<FWidgetAndPointer> PointerWidgets;
                            for(int32 I=0;I<Path.Widgets.Num();++I)
                                PointerWidgets.Emplace(Path.Widgets[I],TOptional<FVirtualPointerPosition>());
                            Path=FWidgetPath(MakeArrayView(PointerWidgets));
                            FSlateApplication::Get().RoutePointerMoveEvent(Path,Move,false);
                        }
                        break;
                    }
        if (Mode==TEXT("win") || Mode==TEXT("lose"))
        {
            // Skip narrative solely in the disposable presentation fixture.
            HUD->PreviewResultForDebug(Mode==TEXT("win"));
        }
        if (Mode==TEXT("exit"))
            for (TObjectIterator<UResultWidget> It;It;++It)
                if (It->GetWorld()==World && It->IsInViewport())
                    for (const TCHAR* Name:{TEXT("Exit_Btn_W"),TEXT("Exit_Btn_F"),TEXT("EtherPreviewExit")})
                        if (auto* Button=Cast<UButton>(It->GetWidgetFromName(Name));Button && Button->IsVisible())
                        { Button->OnClicked.Broadcast(); return; }
        if (Mode==TEXT("dump"))
        {
            FString Report;
            const auto Focus=FSlateApplication::Get().GetUserFocusedWidget(0);
            Report+=FString::Printf(TEXT("focus=%s paused=%d time=%.5f viewport=%s\n"),Focus?*Focus->ToString():TEXT("none"),World->IsPaused(),World->GetTimeSeconds(),*World->GetGameViewport()->Viewport->GetSizeXY().ToString());
            for(AActor* Actor:HUD->GetSelectedActors()) Report+=TEXT("selected=")+Actor->GetName()+TEXT("\n");
            for (TActorIterator<AActor> It(World);It;++It)
                if (It->InputComponent)
                    for(const auto& Key:It->InputComponent->KeyBindings)
                        Report+=FString::Printf(TEXT("actorInput=%s key=%s consume=%d\n"),*It->GetName(),*Key.Chord.Key.ToString(),Key.bConsumeInput);
            for(TObjectIterator<USFMultiSelectionEntryWidget> It;It;++It)
                if(It->GetWorld()==World && It->GetRepresentedActor())
                {
                    auto* Button=Cast<UButton>(It->GetWidgetFromName(TEXT("EntryButton")));
                    auto* Bar=Cast<UProgressBar>(It->GetWidgetFromName(TEXT("HealthBar")));
                    Report+=FString::Printf(TEXT("entry=%s percent=%.3f color=%s brush=%s hover=%d button=%s canvas=%s\n"),
                        *It->GetRepresentedActor()->GetName(),Bar->GetPercent(),*Bar->GetFillColorAndOpacity().ToString(),
                        *Bar->GetWidgetStyle().FillImage.TintColor.GetSpecifiedColor().ToString(),Button->IsHovered(),
                        *Button->GetCachedGeometry().GetLocalSize().ToString(),*It->GetCachedGeometry().GetLocalSize().ToString());
                    const auto& Paint=Button->GetPaintSpaceGeometry();
                    const FVector2D P=Paint.LocalToAbsolute(FVector2D::ZeroVector);
                    const FVector2D E=Paint.LocalToAbsolute(Paint.GetLocalSize());
                    const FVector2D Tick=Button->GetTickSpaceGeometry().LocalToAbsolute(FVector2D::ZeroVector);
                    Report+=FString::Printf(TEXT("bounds index=%d hover=%d paint=%.3f,%.3f,%.3f,%.3f tick=%.3f,%.3f\n"),
                        It->GetSelectionIndex(),Button->IsHovered(),P.X,P.Y,E.X,E.Y,Tick.X,Tick.Y);
                }
            FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectSavedDir()/TEXT("HUDPolish")/(Args.Num()>1?FPaths::GetCleanFilename(Args[1])+TEXT(".txt"):TEXT("runtime.txt"))));
        }
    }));
#endif

