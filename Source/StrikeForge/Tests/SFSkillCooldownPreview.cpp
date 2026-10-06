#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "HAL/IConsoleManager.h"
#include "InGame/Controller/SFInGamePlayerController.h"
#include "InGame/SFInGameHUDWidget.h"
#include "InGame/HUD/SFSkillCooldownWidget.h"
#include "InGame/HUD/Tutorial/SFStageIntroWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Buildings/Building/HouseBuilding/Nexus/SFNexusBuilding.h"
#include "Units/Commander/SFCommander.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Animation/AnimInstance.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

struct FSFCommanderCooldownTestAccess
{
    static void VisualSample(ASFCommander* Commander)
    {
        for (int32 Index = 0; Index < 3; ++Index)
            Commander->SkillCooldownEnds[Index] = Commander->GetWorld()->GetTimeSeconds() + 25.f - Index * 10.f;
    }
};

static FAutoConsoleCommandWithWorldAndArgs SkillCooldownPreview(
    TEXT("SF.Presentation.Check"), TEXT("Disposable PIE validation of narration, portraits and skill cooldowns."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
    {
        if (!World || World->WorldType != EWorldType::PIE || Args.IsEmpty()) return;
        const FString Mode = Args[0];
        const FString Folder = FPaths::ProjectSavedDir()/TEXT("TutorialPresentation");
        FString Report;
        bool Passed = true;
        auto Check = [&](bool Value, const FString& Label)
        { Passed &= Value; Report += (Value ? TEXT("PASS ") : TEXT("FAIL ")) + Label + TEXT("\n"); };
        auto* PC = World->GetFirstPlayerController<ASFInGamePlayerController>();
        auto* Commander = PC ? PC->GetMyCommander() : nullptr;
        auto* HUD = PC ? PC->GetInGameHUDWidget() : nullptr;
        auto* GS = World->GetGameState<ASFGameState>();
        Check(Commander && HUD && GS, TEXT("live game objects"));
        if (!Passed) return;
        if (Mode == TEXT("narration") || Mode == TEXT("portraits"))
        {
            TArray<UUserWidget*> Intros;
            UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Intros, USFStageIntroWidget::StaticClass(), true);
            Check(Intros.Num() == 1, TEXT("one story widget"));
            if (Intros.Num() == 1)
                for (const TCHAR* Name : {TEXT("LeftCharacterImage"), TEXT("RightCharacterImage")})
                {
                    auto* Image = Cast<UImage>(Intros[0]->GetWidgetFromName(Name));
                    Check(Image && (Mode == TEXT("narration") ? Image->GetVisibility() == ESlateVisibility::Collapsed :
                        Image->GetVisibility() != ESlateVisibility::Collapsed && Image->GetBrush().GetResourceObject()), Name);
                }
        }
        else
        {
            auto* Nexus = GS->GetNexus(PC->GetMyTeamIndex());
            auto* TierProperty = FindFProperty<FIntProperty>(ASFBaseBuilding::StaticClass(), TEXT("CurrentTier"));
            Check(Nexus && TierProperty, TEXT("Nexus tier property"));
            if (Nexus && TierProperty && Mode == TEXT("cast"))
            {
                TierProperty->SetPropertyValue_InContainer(Nexus, 0);
                Commander->ActiveCommanderSkill(2);
                Check(Commander->GetSkillCooldownRemaining(2) == 0.f, TEXT("locked cast consumes no cooldown"));
                TierProperty->SetPropertyValue_InContainer(Nexus, 2);
                for (int32 Tier = 1; Tier <= 3; ++Tier)
                {
                    Commander->GetMesh()->GetAnimInstance()->Montage_Stop(0.f);
                    Commander->ActiveCommanderSkill(Tier);
                    Check(FMath::IsNearlyEqual(Commander->GetSkillCooldownRemaining(Tier), 30.f, 0.01f),
                        FString::Printf(TEXT("skill %d starts at 30 seconds"), Tier));
                    Commander->ActiveCommanderSkill(Tier);
                    Check(FMath::IsNearlyEqual(Commander->GetSkillCooldownRemaining(Tier), 30.f, 0.01f), TEXT("repeat cast does not reset timer"));
                }
                Commander->ActiveCommanderSkill(0);
                Commander->ActiveCommanderSkill(4);
                Check(Commander->GetSkillCooldownRemaining(0) == 0.f && Commander->GetSkillCooldownRemaining(4) == 0.f,
                    TEXT("invalid tiers handled"));
            }
            if (Mode == TEXT("recast"))
            {
                const float Before = Commander->GetSkillCooldownRemaining(1);
                Commander->ActiveCommanderSkill(1);
                Check(Before > 0.f && Before < 29.f && FMath::IsNearlyEqual(Before, Commander->GetSkillCooldownRemaining(1)),
                    TEXT("cooling skill cannot be recast after montage ends"));
            }
            if (Mode == TEXT("expired"))
            {
                for (int32 Tier = 1; Tier <= 3; ++Tier)
                    Check(Commander->GetSkillCooldownRemaining(Tier) == 0.f, TEXT("timer expired"));
                Commander->ActiveCommanderSkill(1);
                Check(Commander->GetSkillCooldownRemaining(1) > 29.9f, TEXT("ready skill can be cast again"));
                Check(Commander->GetSkillCooldownRemaining(2) == 0.f, TEXT("other skills remain ready"));
            }
            if (Mode == TEXT("visual")) FSFCommanderCooldownTestAccess::VisualSample(Commander);
            HUD->SelectActor(Commander);
            HUD->RefreshCommanderSkillIcons();
            for (int32 Tier = 1; Tier <= 3; ++Tier)
            {
                Report += FString::Printf(TEXT("remaining%d=%.4f\n"), Tier, Commander->GetSkillCooldownRemaining(Tier));
                UWidget* Target = HUD->GetTutorialCommanderSkillWidget(Tier);
                auto* Panel = Target ? Target->GetTypedOuter<UUserWidget>() : nullptr;
                auto* Overlay = Panel ? Cast<UOverlay>(Panel->GetWidgetFromName(FName(*FString::Printf(TEXT("SkillSlotOverlay%02d"), Tier)))) : nullptr;
                int32 Count = 0;
                if (Overlay) for (UWidget* Child : Overlay->GetAllChildren()) if (Cast<USFSkillCooldownWidget>(Child)) ++Count;
                Check(Count == 1, TEXT("exactly one radial overlay on each skill"));
            }
        }
        FFileHelper::SaveStringToFile((Passed ? TEXT("PASSED\n") : TEXT("FAILED\n")) + Report, *(Folder/(Mode+TEXT(".txt"))));
        FScreenshotRequest::RequestScreenshot(Folder/(Mode+TEXT(".png")), true, false);
    }));
#endif
