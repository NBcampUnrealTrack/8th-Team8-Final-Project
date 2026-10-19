#include "SFTitlePresentation.h"
#include "StartWidget.h"
#include "SFMenuCameraActor.h"
#include "Game/Multiplayer/SFPVPLobby.h"
#include "Game/Multiplayer/SFPVPSessionSubsystem.h"
#include "Camera/CameraComponent.h"
#include "EngineUtils.h"
#include "TimerManager.h"

ASFTitleMode::ASFTitleMode()
{
    PlayerControllerClass=ASFTitleController::StaticClass();
    PlayerStateClass=ASFPVPLobbyPlayerState::StaticClass();
    DefaultPawnClass=nullptr;
    bUseSeamlessTravel=true;
}
ASFTitleController::ASFTitleController()
{bAutoManageActiveCameraTarget=false;TitleWidgetClass=TSoftClassPtr<UStartWidget>(FSoftObjectPath(TEXT("/Game/Core/UI/WBP_Start.WBP_Start_C")));}
void ASFTitleController::BeginPlay()
{
    Super::BeginPlay();
    if(!IsLocalController())return;
    const auto Class=TitleWidgetClass.LoadSynchronous();
    if(Class)TitleWidget=CreateWidget<UStartWidget>(this,Class);
    if(TitleWidget)
    {
        TitleWidget->AddToPlayerScreen();
        FInputModeUIOnly Mode;Mode.SetWidgetToFocus(TitleWidget->TakeWidget());Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        SetInputMode(Mode);bShowMouseCursor=true;
    }
    ApplyTitleCamera();
    // A client controller can begin play before the level's camera has finished initializing.
    GetWorldTimerManager().SetTimer(CameraTimer,this,&ThisClass::ApplyTitleCamera,.2f,true);
}
void ASFTitleController::ApplyTitleCamera()
{
    if(!IsLocalController())return;
    for(TActorIterator<ASFMenuCameraActor> It(GetWorld());It;++It)
    {
        It->GetCameraComponent()->bConstrainAspectRatio=false;
        if(GetViewTarget()!=*It)SetViewTarget(*It);
        return;
    }
}
void ASFTitleController::ServerOpenPVPLobby_Implementation()
{
    if(GetWorld()->GetAuthGameMode<ASFTitleMode>())GetGameInstance()->GetSubsystem<USFPVPSessionSubsystem>()->OpenBrowser();
}
void ASFTitleController::EndPlay(const EEndPlayReason::Type Reason)
{
    GetWorldTimerManager().ClearTimer(CameraTimer);
    if(TitleWidget)TitleWidget->RemoveFromParent();
    Super::EndPlay(Reason);
}
