#include "SFInGamePlayerController.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystemInterface.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InGame/SFInGameHUDWidget.h"

ASFInGamePlayerController::ASFInGamePlayerController()
{
    bShowMouseCursor = true;
    bEnableClickEvents = true;
    bEnableMouseOverEvents = true;

    DefaultMouseCursor = EMouseCursor::Default;
}

void ASFInGamePlayerController::BeginPlay()
{
    Super::BeginPlay();

    ConfigureMouseInput();
    RegisterBaseMappingContext();
    SetSFControlMode(
        ESFControlMode::Gameplay
    );
    CreateInGameHUD();
}

void ASFInGamePlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    UEnhancedInputComponent* EnhancedInputComponent =
        Cast<UEnhancedInputComponent>(
            InputComponent
        );

    if (!IsValid(EnhancedInputComponent))
    {
        return;
    }

    if (IsValid(SelectAction))
    {
        EnhancedInputComponent->BindAction(
            SelectAction,
            ETriggerEvent::Started,
            this,
            &ASFInGamePlayerController::
            HandleSelectInput
        );
    }

    if (IsValid(ConfirmBuildAction))
    {
        EnhancedInputComponent->BindAction(
            ConfirmBuildAction,
            ETriggerEvent::Started,
            this,
            &ASFInGamePlayerController::
            HandleConfirmBuildInput
        );
    }

    if (IsValid(CancelBuildAction))
    {
        EnhancedInputComponent->BindAction(
            CancelBuildAction,
            ETriggerEvent::Started,
            this,
            &ASFInGamePlayerController::
            HandleCancelBuildInput
        );
    }
}

void ASFInGamePlayerController::SetSFControlMode(
    ESFControlMode NewControlMode
)
{
    if (
        bControlModeInitialized
        && CurrentControlMode == NewControlMode
        )
    {
        return;
    }

    UEnhancedInputLocalPlayerSubsystem* InputSubsystem =
        GetEnhancedInputSubsystem();

    if (!IsValid(InputSubsystem))
    {
        return;
    }

    UInputMappingContext* TargetMappingContext = nullptr;
    int32 TargetMappingPriority = 0;

    switch (NewControlMode)
    {
    case ESFControlMode::Gameplay:
        TargetMappingContext =
            GameplayMappingContext;

        TargetMappingPriority =
            GameplayMappingPriority;
        break;

    case ESFControlMode::BuildPlacement:
        TargetMappingContext =
            BuildPlacementMappingContext;

        TargetMappingPriority =
            BuildPlacementMappingPriority;
        break;

    default:
        return;
    }

    if (!IsValid(TargetMappingContext))
    {
        return;
    }

    FModifyContextOptions ModifyOptions;

    ModifyOptions
        .bIgnoreAllPressedKeysUntilRelease = true;

    if (IsValid(GameplayMappingContext))
    {
        InputSubsystem->RemoveMappingContext(
            GameplayMappingContext,
            ModifyOptions
        );
    }

    if (IsValid(BuildPlacementMappingContext))
    {
        InputSubsystem->RemoveMappingContext(
            BuildPlacementMappingContext,
            ModifyOptions
        );
    }

    InputSubsystem->AddMappingContext(
        TargetMappingContext,
        TargetMappingPriority,
        ModifyOptions
    );

    CurrentControlMode = NewControlMode;
    bControlModeInitialized = true;
}

ESFControlMode
ASFInGamePlayerController::GetSFControlMode() const
{
    return CurrentControlMode;
}

UEnhancedInputLocalPlayerSubsystem*
ASFInGamePlayerController::
GetEnhancedInputSubsystem() const
{
    ULocalPlayer* LocalPlayer =
        GetLocalPlayer();

    if (!IsValid(LocalPlayer))
    {
        return nullptr;
    }

    return ULocalPlayer::GetSubsystem<
        UEnhancedInputLocalPlayerSubsystem
    >(LocalPlayer);
}

void ASFInGamePlayerController::
ConfigureMouseInput()
{
    bShowMouseCursor = true;
    bEnableClickEvents = true;
    bEnableMouseOverEvents = true;

    FInputModeGameAndUI InputMode;

    InputMode.SetHideCursorDuringCapture(false);

    InputMode.SetLockMouseToViewportBehavior(
        bLockMouseToViewport
        ? EMouseLockMode::LockAlways
        : EMouseLockMode::DoNotLock
    );

    SetInputMode(InputMode);
}

void ASFInGamePlayerController::
RegisterBaseMappingContext()
{
    UEnhancedInputLocalPlayerSubsystem* InputSubsystem =
        GetEnhancedInputSubsystem();

    if (!IsValid(InputSubsystem))
    {
        return;
    }

    if (!IsValid(BaseMappingContext))
    {
        return;
    }

    InputSubsystem->AddMappingContext(
        BaseMappingContext,
        BaseMappingPriority
    );
}

void ASFInGamePlayerController::
HandleSelectInput()
{
    OnSelectInput.Broadcast();
}

void ASFInGamePlayerController::
HandleConfirmBuildInput()
{
    OnConfirmBuildInput.Broadcast();
}

void ASFInGamePlayerController::
HandleCancelBuildInput()
{
    OnCancelBuildInput.Broadcast();
}

void ASFInGamePlayerController::
CreateInGameHUD()
{
    if (!IsLocalController())
    {
        return;
    }

    if (!InGameHUDWidgetClass)
    {
        return;
    }

    InGameHUDWidget =
        CreateWidget<USFInGameHUDWidget>(
            this,
            InGameHUDWidgetClass
        );

    if (!IsValid(InGameHUDWidget))
    {
        return;
    }

    InGameHUDWidget->AddToViewport();
}