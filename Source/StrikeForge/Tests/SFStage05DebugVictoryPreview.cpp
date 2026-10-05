#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "HAL/IConsoleManager.h"
#include "Components/InputComponent.h"
#include "InGame/Controller/SFInGamePlayerController.h"
#include "Engine/World.h"

// Exercise the actual K binding during disposable PIE integration checks.
static FAutoConsoleCommandWithWorld Stage05PressVictoryKey(
    TEXT("SF.Story.PressVictoryKey"), TEXT("PIE QA: invoke the player's editor K binding."),
    FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
    {
        if (!World || World->WorldType != EWorldType::PIE) return;
        auto* PC = World->GetFirstPlayerController<ASFInGamePlayerController>();
        if (!PC || !PC->InputComponent) return;
        for (const FInputKeyBinding& Binding : PC->InputComponent->KeyBindings)
        {
            if (Binding.Chord.Key == EKeys::K && Binding.KeyEvent == IE_Pressed)
            {
                Binding.KeyDelegate.Execute(EKeys::K);
                return;
            }
        }
    }));
#endif
