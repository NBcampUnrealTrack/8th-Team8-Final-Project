#include "SFSettingsWidget.h"

#include "Components/Button.h"
#include "Game/GameInstance/SFGameInstance.h"
#include "Logging/SFLog.h"

void USFSettingsWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (IsValid(ContinueButton))
    {
        ContinueButton->OnClicked.AddUniqueDynamic(this, &USFSettingsWidget::HandleContinueButtonClicked);
    }
    else
    {
        SF_LOG(Error, "[SFSettingsWidget] ContinueButton is invalid.");
    }

    if (IsValid(ExitButton))
    {
        ExitButton->OnClicked.AddUniqueDynamic(this, &USFSettingsWidget::HandleExitButtonClicked);
    }
    else
    {
        SF_LOG(Error, "[SFSettingsWidget] ExitButton is invalid.");
    }

    SF_LOG(Log, "[SFSettingsWidget] Button binding completed | Widget: %s", *GetName());
}

void USFSettingsWidget::NativeDestruct()
{
    if (IsValid(ContinueButton))
    {
        ContinueButton->OnClicked.RemoveDynamic(this, &USFSettingsWidget::HandleContinueButtonClicked);
    }

    if (IsValid(ExitButton))
    {
        ExitButton->OnClicked.RemoveDynamic(this, &USFSettingsWidget::HandleExitButtonClicked);
    }

    Super::NativeDestruct();
}

void USFSettingsWidget::HandleContinueButtonClicked()
{
    SF_LOG(Log, "[SFSettingsWidget] Continue requested.");
    DeactivateWidget();
}

void USFSettingsWidget::HandleExitButtonClicked()
{
    USFGameInstance* GameInstance = Cast<USFGameInstance>(GetGameInstance());

    if (!IsValid(GameInstance))
    {
        SF_LOG(Error, "[SFSettingsWidget] SFGameInstance is invalid.");
        return;
    }

    SF_LOG(Log, "[SFSettingsWidget] Move to lobby requested.");

    if (!GameInstance->MoveToLobby())
    {
        SF_LOG(Error, "[SFSettingsWidget] Failed to move to lobby.");
    }
}