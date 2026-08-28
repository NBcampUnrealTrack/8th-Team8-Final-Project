#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "SFSettingsWidget.generated.h"

class UButton;

UCLASS(Abstract, Blueprintable)
class STRIKEFORGE_API USFSettingsWidget : public UCommonActivatableWidget
{
    GENERATED_BODY()

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    UPROPERTY(BlueprintReadOnly, Category = "SF|Settings", meta = (BindWidget))
    TObjectPtr<UButton> ContinueButton = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "SF|Settings", meta = (BindWidget))
    TObjectPtr<UButton> ExitButton = nullptr;

private:
    UFUNCTION()
    void HandleContinueButtonClicked();

    UFUNCTION()
    void HandleExitButtonClicked();
};