#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/Slider.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimMontage.h"
#include "Units/Unit/UnitBase.h"
#include "UObject/UnrealType.h"
#include "UI/SettingWidget.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"
#include "HAL/IConsoleManager.h"
#include "Game/GameInstance/SFGameInstance.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Components/Button.h"
#include "UObject/Script.h"

static FAutoConsoleCommandWithWorld SFSettingsPreview(TEXT("SF.Playtest.SettingsPreview"),
    TEXT("Capture the actual settings widget and quit."), FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
    {
        FTimerHandle Ready;
        World->GetTimerManager().SetTimer(Ready, FTimerDelegate::CreateWeakLambda(World, [World]()
        {
            UClass* Class = LoadClass<USettingWidget>(nullptr, TEXT("/Game/Core/UI/WBP_Setting.WBP_Setting_C"));
            auto* Widget = CreateWidget<USettingWidget>(World->GetFirstPlayerController(), Class);
            if (!Widget) return;
            Widget->AddToViewport(100);
            Widget->OpenSetting();
            FTimerHandle Shot, Quit;
            World->GetTimerManager().SetTimer(Shot, FTimerDelegate::CreateLambda([]()
            { FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("PlaytestFeedback/settings.png"), true, false); }), 3.f, false);
            World->GetTimerManager().SetTimer(Quit, FTimerDelegate::CreateLambda([]()
            { FPlatformMisc::RequestExit(false); }), 5.f, false);
        }), 1.f, false);
    }));

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFPlaytestAssetAudit, "StrikeForge.Playtest.AssetAudit",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFPlaytestAssetAudit::RunTest(const FString& Parameters)
{
    auto* Class = LoadClass<UUserWidget>(nullptr, TEXT("/Game/Core/UI/WBP_Setting.WBP_Setting_C"));
    if (!TestNotNull(TEXT("Settings blueprint"), Class)) return false;
    auto* Tree = CastChecked<UWidgetBlueprintGeneratedClass>(Class)->GetWidgetTreeArchetype();
    Tree->ForEachWidget([this](UWidget* Widget)
    {
        FString Detail;
        if (auto* Text = Cast<UTextBlock>(Widget)) Detail = Text->GetText().ToString();
        if (auto* Slot = Cast<UCanvasPanelSlot>(Widget->Slot))
            Detail += FString::Printf(TEXT(" position=%s size=%s"), *Slot->GetPosition().ToString(), *Slot->GetSize().ToString());
        AddInfo(FString::Printf(TEXT("WIDGET %s %s parent=%s %s"), *Widget->GetClass()->GetName(),
            *Widget->GetName(), *GetNameSafe(Widget->GetParent()), *Detail));
    });
    for (const TCHAR* Name : {TEXT("MasterVolumeSlider"), TEXT("MouseSensitivitySlider")})
        TestNotNull(Name, Cast<USlider>(Tree->FindWidget(Name)));
    UClass* BeastClass = LoadClass<AUnitBase>(nullptr, TEXT("/Game/Core/Units/Beast_Fox/BP_Fox_WarBeast.BP_Fox_WarBeast_C"));
    if (!TestNotNull(TEXT("WarBeast blueprint"), BeastClass)) return false;
    auto* Beast = BeastClass->GetDefaultObject<AUnitBase>();
    AddInfo(FString::Printf(TEXT("BEAST capsule=%.1f halfHeight=%.1f mesh=%s animation=%s"),
        Beast->GetCapsuleComponent()->GetScaledCapsuleRadius(), Beast->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(),
        *GetNameSafe(Beast->GetMesh()->GetSkeletalMeshAsset()), *GetNameSafe(Beast->GetMesh()->GetAnimClass())));
    auto* Montage = Cast<UAnimMontage>(FindFProperty<FObjectPropertyBase>(AUnitBase::StaticClass(), TEXT("AttackMontage"))->GetObjectPropertyValue_InContainer(Beast));
    if (!TestNotNull(TEXT("Attack montage"), Montage)) return false;
    for (const auto& Notify : Montage->Notifies)
        AddInfo(FString::Printf(TEXT("HIT_NOTIFY %s at %.3f"), *GetNameSafe(Notify.Notify), Notify.GetTriggerTime()));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFSettingsInteractionTest, "StrikeForge.Playtest.SettingsInteraction",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFSettingsInteractionTest::RunTest(const FString& Parameters)
{
    FEditorScriptExecutionGuard ScriptGuard;
    USFGameInstance* Instance = NewObject<USFGameInstance>(GEngine);
    Instance->InitializeStandalone();
    UWorld* World = Instance->GetWorld();
    APlayerController* PC = World->SpawnActor<APlayerController>();
    ULocalPlayer* Local = NewObject<ULocalPlayer>(GEngine);
    Instance->AddLocalPlayer(Local, FPlatformUserId::CreateFromInternalId(0));
    PC->SetPlayer(Local);
    UClass* Class = LoadClass<USettingWidget>(nullptr, TEXT("/Game/Core/UI/WBP_Setting.WBP_Setting_C"));
    USettingWidget* Widget = CreateWidget<USettingWidget>(PC, Class);
    TSharedRef<SWidget> Slate = Widget->TakeWidget();
    USlider* Sensitivity = Cast<USlider>(Widget->GetWidgetFromName(TEXT("MouseSensitivitySlider")));
    TestEqual(TEXT("Sensitivity includes slow cursor movement"), Sensitivity->GetMinValue(), .1f);
    TestEqual(TEXT("Sensitivity includes fast cursor movement"), Sensitivity->GetMaxValue(), 3.f);
    TestNotNull(TEXT("Native settings panel"), Widget->GetWidgetFromName(TEXT("SettingsModalRoot")));
    for (const TCHAR* Name : {TEXT("BackUi_Btn")})
    {
        // The complete decorated button now contains its caption and shares one hit target.
        Widget->SetVisibility(ESlateVisibility::Visible);
        UButton* Button = Cast<UButton>(Widget->GetWidgetFromName(Name));
        if (!TestNotNull(Name, Button)) continue;
        Button->OnClicked.Broadcast();
        TestTrue(TEXT("Full button closes settings"), Widget->GetVisibility() == ESlateVisibility::Collapsed);
    }
    Instance->Shutdown();
    World->DestroyWorld(false);
    GEngine->DestroyWorldContext(World);
    return true;
}
#endif
