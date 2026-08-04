#include "SFInGamePlayerController.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "SFInGameHUDWidget.h"

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
    RegisterInputMappingContext();
    CreateInGameHUD();
}

void ASFInGamePlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    UEnhancedInputComponent* EnhancedInputComponent =
        Cast<UEnhancedInputComponent>(InputComponent);

    if (!IsValid(EnhancedInputComponent))
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT(
                "SFInGamePlayerController: "
                "EnhancedInputComponent is invalid."
            )
        );

        return;
    }

    if (!IsValid(SelectAction))
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT(
                "SFInGamePlayerController: "
                "SelectAction is not assigned."
            )
        );

        return;
    }

    EnhancedInputComponent->BindAction(
        SelectAction,
        ETriggerEvent::Started,
        this,
        &ASFInGamePlayerController::HandleSelectInput
    );
}

void ASFInGamePlayerController::ConfigureMouseInput()
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

void ASFInGamePlayerController::RegisterInputMappingContext()
{
    ULocalPlayer* LocalPlayer = GetLocalPlayer();

    if (!IsValid(LocalPlayer))
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT(
                "SFInGamePlayerController: "
                "LocalPlayer is invalid."
            )
        );

        return;
    }

    UEnhancedInputLocalPlayerSubsystem* InputSubsystem =
        ULocalPlayer::GetSubsystem<
        UEnhancedInputLocalPlayerSubsystem
        >(LocalPlayer);

    if (!IsValid(InputSubsystem))
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT(
                "SFInGamePlayerController: "
                "Enhanced Input Subsystem is invalid."
            )
        );

        return;
    }

    if (!IsValid(InGameMappingContext))
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT(
                "SFInGamePlayerController: "
                "InGameMappingContext is not assigned."
            )
        );

        return;
    }

    InputSubsystem->AddMappingContext(
        InGameMappingContext,
        MappingPriority
    );
}

void ASFInGamePlayerController::HandleSelectInput()
{
    UE_LOG(
        LogTemp,
        Warning,
        TEXT("SF Select Input Received")
    );

    OnSelectInput.Broadcast();
}

void ASFInGamePlayerController::CreateInGameHUD()
{
    if (!IsLocalController())
    {
        return;
    }

    if (!InGameHUDWidgetClass)
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT(
                "SFInGamePlayerController: "
                "InGameHUDWidgetClass가 설정되지 않았습니다."
            )
        );

        return;
    }

    InGameHUDWidget =
        CreateWidget<USFInGameHUDWidget>(
            this,
            InGameHUDWidgetClass
        );

    if (!IsValid(InGameHUDWidget))
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT(
                "SFInGamePlayerController: "
                "인게임 HUD 생성 실패"
            )
        );

        return;
    }

    InGameHUDWidget->AddToViewport();

    UE_LOG(
        LogTemp,
        Warning,
        TEXT(
            "SFInGamePlayerController: "
            "인게임 HUD 생성 완료"
        )
    );
}