#if WITH_DEV_AUTOMATION_TESTS
#include "HAL/IConsoleManager.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Engine/World.h"
#include "Game/GameInstance/SFGameInstance.h"
#include "UI/SFAchievementToastWidget.h"

// Visual-only preview; does not award progress or touch campaign saves.
static FAutoConsoleCommandWithWorldAndArgs SFAchievementPreview(
    TEXT("SF.Achievements.Preview"), TEXT("Preview [stage] [animation time]; clear removes preview widgets."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
    {
        if (!World) return;
        TArray<UUserWidget*> Existing;
        UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World,Existing,USFAchievementToastWidget::StaticClass(),false);
        for (auto* Widget : Existing) Widget->RemoveFromParent();
        if (!Args.IsEmpty() && Args[0] == TEXT("clear")) return;
        auto* GI = World->GetGameInstance<USFGameInstance>();
        if (!GI || !World->GetFirstPlayerController()) return;
        const auto Entries = GI->GetPVEAchievements();
        const int32 Index = Args.IsEmpty() ? 0 : FMath::Clamp(FCString::Atoi(*Args[0])-1,0,Entries.Num()-1);
        auto* Toast = CreateWidget<USFAchievementToastWidget>(World->GetFirstPlayerController());
        Toast->InitializeAchievement(Entries[Index]);
        if (Args.Num()>1) Toast->PreviewAt(FCString::Atof(*Args[1]));
        Toast->AddToViewport(1000);
    }));
#endif
