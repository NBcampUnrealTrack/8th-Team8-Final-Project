#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "SFInGamePlayerController.generated.h"

class UInputAction;
class UInputMappingContext;
class USFInGameHUDWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSFOnSelectInput);

UCLASS(Blueprintable)
class STRIKEFORGE_API ASFInGamePlayerController
    : public APlayerController
{
    GENERATED_BODY()

public:
    ASFInGamePlayerController();

    UPROPERTY(
        BlueprintAssignable,
        Category = "SF|Input"
    )
    FSFOnSelectInput OnSelectInput;

protected:
    virtual void BeginPlay() override;

    virtual void SetupInputComponent() override;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "SF|Input"
    )
    TObjectPtr<UInputMappingContext> InGameMappingContext;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "SF|UI"
    )
    TSubclassOf<USFInGameHUDWidget> InGameHUDWidgetClass;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "SF|Input"
    )
    TObjectPtr<UInputAction> SelectAction;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "SF|Input"
    )
    int32 MappingPriority = 0;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "SF|Input"
    )
    bool bLockMouseToViewport = true;

private:
    void RegisterInputMappingContext();

    void ConfigureMouseInput();

    void HandleSelectInput();

    UPROPERTY(
        Transient
    )
    TObjectPtr<USFInGameHUDWidget> InGameHUDWidget;

    void CreateInGameHUD();
};