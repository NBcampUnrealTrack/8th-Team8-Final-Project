#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "HAL/IConsoleManager.h"
#include "InGame/Controller/SFInGamePlayerController.h"
#include "InGame/SFInGameHUDWidget.h"
#include "InGame/HUD/Tutorial/SFTutorialSubsystem.h"
#include "InGame/HUD/Tutorial/SFTutorialSpotlightWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Units/Commander/SFCommander.h"
#include "Buildings/Building/HouseBuilding/Nexus/SFNexusBuilding.h"
#include "Buildings/Resource/ResourceBase.h"
#include "Components/Image.h"
#include "Components/Border.h"
#include "Components/WidgetComponent.h"
#include "EngineUtils.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

// Disposable PIE probe: checks actual HUD brush/geometry and zero-based Nexus tiers.
struct FSFCommanderContentPreviewAccess
{
    static bool Check(USFTutorialSpotlightWidget* Widget, FString& Report)
    {
        Report += FString::Printf(TEXT("spotlight target=%s visible=%d center=%s extent=%s geometry=%s\n"),
            *GetNameSafe(Widget->ResolveScreenTargetWidget()), Widget->bTargetVisible,
            *Widget->HoleCenterNorm.ToString(), *Widget->HoleExtentNorm.ToString(), *Widget->GetCachedGeometry().GetLocalSize().ToString());
        const bool bSparkles = Widget->SkillHighlightEdges.Num() == 4 && Widget->SkillHighlightGlows.Num() == 4 && Widget->SkillSparkles.Num() == 8;
        Report += FString::Printf(TEXT("skill_sparkles=%d pulse_phase=%.3f\n"), bSparkles, Widget->PulseTime);
        if (bSparkles)
            Report += FString::Printf(TEXT("edge_alpha=%.3f glow_alpha=%.3f spark_alpha=%.3f\n"),
                Widget->SkillHighlightEdges[0]->GetBrushColor().A, Widget->SkillHighlightGlows[0]->GetBrushColor().A, Widget->SkillSparkles[0]->GetBrushColor().A);
        return bSparkles && Widget->bHasStep && Widget->bTargetVisible && Widget->ResolveScreenTargetWidget() && Widget->CurrentStep.bShowHighlight;
    }
};

static FAutoConsoleCommandWithWorldAndArgs CommanderContentCheck(
    TEXT("SF.Commander.CheckContent"), TEXT("PIE QA: validate skill HUD and save a screenshot."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
    {
        if (!World || World->WorldType != EWorldType::PIE) return;
        auto* PC = World->GetFirstPlayerController<ASFInGamePlayerController>();
        auto* Commander = PC ? PC->GetMyCommander() : nullptr;
        auto* HUD = PC ? PC->GetInGameHUDWidget() : nullptr;
        auto* GS = World->GetGameState<ASFGameState>();
        bool Passed = Commander && HUD && GS;
        FString Report;
        auto Check = [&Passed, &Report](bool Value, const FString& What)
        { Passed &= Value; Report += FString::Printf(TEXT("%s %s\n"), Value ? TEXT("PASS") : TEXT("FAIL"), *What); };
        if (Passed)
        {
            const auto* Tutorial = USFTutorialSubsystem::Get(World);
            const int32 ExpectedStep = Args.IsEmpty() ? 0 : FCString::Atoi(*Args[0].Right(1))-1;
            Check(Tutorial && Tutorial->IsGuideActive() && Tutorial->GetCurrentGuideStepIndex()==ExpectedStep,
                TEXT("expected skill guide step is active"));
            TArray<UUserWidget*> Spotlights;
            UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Spotlights, USFTutorialSpotlightWidget::StaticClass(), false);
            Check(Spotlights.Num()==1 && FSFCommanderContentPreviewAccess::Check(CastChecked<USFTutorialSpotlightWidget>(Spotlights[0]), Report),
                TEXT("yellow rectangle tracks the live skill slot"));
            HUD->RefreshCommanderSkillIcons();
            for (int32 Tier = 1; Tier <= 3; ++Tier)
            {
                UWidget* Slot = HUD->GetTutorialCommanderSkillWidget(Tier);
                Check(Slot && Slot->GetCachedGeometry().GetLocalSize().X > 0.f,
                    FString::Printf(TEXT("skill %d target geometry"), Tier));
                const FVector2D Position = Slot ? Slot->GetCachedGeometry().GetAbsolutePosition() : FVector2D::ZeroVector;
                Report += FString::Printf(TEXT("slot%d position=%s\n"), Tier, *Position.ToString());
                auto* Panel = Slot ? Slot->GetTypedOuter<UUserWidget>() : nullptr;
                auto* Icon = Panel ? Cast<UImage>(Panel->GetWidgetFromName(FName(*FString::Printf(TEXT("SkillIcon%02d"), Tier)))) : nullptr;
                Check(Icon && Icon->GetBrush().GetResourceObject() &&
                    Icon->GetBrush().GetResourceObject()->GetName().StartsWith(TEXT("T_Skill_")), TEXT("generated icon is in HUD brush"));
                Check(Commander->SkillIcons.IsValidIndex(Tier-1) && !Commander->SkillIcons[Tier-1].IsNull(),
                    FString::Printf(TEXT("skill %d texture reference"), Tier));
            }
            auto* Nexus = GS->GetNexus(PC->GetMyTeamIndex());
            auto* TierProperty = FindFProperty<FIntProperty>(ASFBaseBuilding::StaticClass(), TEXT("CurrentTier"));
            Check(Nexus && TierProperty, TEXT("local Nexus for unlock checks"));
            if (Nexus && TierProperty)
            {
                const int32 Original = Nexus->GetCurrentTier();
                for (int32 NexusTier = 0; NexusTier < 3; ++NexusTier)
                {
                    TierProperty->SetPropertyValue_InContainer(Nexus, NexusTier);
                    Check(Commander->GetUnlockedSkillTier() == NexusTier + 1, TEXT("Nexus tier conversion"));
                    for (int32 SkillTier = 0; SkillTier <= 4; ++SkillTier)
                        Check(Commander->IsSkillTierUnlocked(SkillTier) == (SkillTier >= 1 && SkillTier <= NexusTier + 1), TEXT("tier unlock gate"));
                }
                TierProperty->SetPropertyValue_InContainer(Nexus, Original);
            }
            for (TActorIterator<AResourceBase> It(World); It; ++It)
            {
                TArray<UWidgetComponent*> Components;
                It->GetComponents<UWidgetComponent>(Components);
                for (const auto* Component : Components)
                    Check(Component->GetName() != TEXT("ResourceMarker"), TEXT("no resource nameplate"));
            }
            if (auto* Context = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/Game/IN_UI/Input/IMC/IMC_SFGameplay.IMC_SFGameplay")))
                for (const FEnhancedActionKeyMapping& Mapping : Context->GetMappings())
                    if (Mapping.Action && Mapping.Action->GetName().Contains(TEXT("Skill")))
                        Report += Mapping.Action->GetName() + TEXT("=") + Mapping.Key.ToString() + TEXT("\n");
        }
        const FString Name = Args.IsEmpty() ? TEXT("skills") : FPaths::GetCleanFilename(Args[0]);
        const FString Folder = FPaths::ProjectSavedDir()/TEXT("CommanderContent");
        FFileHelper::SaveStringToFile((Passed ? TEXT("PASSED\n") : TEXT("FAILED\n")) + Report, *(Folder/(Name+TEXT(".txt"))));
        FScreenshotRequest::RequestScreenshot(Folder/(Name+TEXT(".png")), true, false);
    }));

static FAutoConsoleCommandWithWorld AdvanceCommanderGuide(
    TEXT("SF.Commander.AdvanceGuide"), TEXT("PIE QA: advance the current skill explanation."),
    FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
    {
        if (World && World->WorldType == EWorldType::PIE)
            if (auto* Tutorial = USFTutorialSubsystem::Get(World)) Tutorial->CompleteCurrentGuideStep();
    }));
#endif
