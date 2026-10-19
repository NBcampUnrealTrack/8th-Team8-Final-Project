#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "SFTitlePresentation.generated.h"

/** A title screen belongs to the local player; dedicated servers never construct UMG. */
UCLASS()
class STRIKEFORGE_API ASFTitleController : public APlayerController
{
    GENERATED_BODY()
public:
    ASFTitleController();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    UFUNCTION(Server,Reliable) void ServerOpenPVPLobby();
private:
    void ApplyTitleCamera();
    UPROPERTY(Transient) TObjectPtr<class UStartWidget> TitleWidget;
    UPROPERTY() TSoftClassPtr<UStartWidget> TitleWidgetClass;
    FTimerHandle CameraTimer;
};

UCLASS()
class STRIKEFORGE_API ASFTitleMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    ASFTitleMode();
};
