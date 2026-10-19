#if WITH_DEV_AUTOMATION_TESTS
#include "HAL/IConsoleManager.h"
#include "Containers/Ticker.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Game/Multiplayer/SFPVPSessionSubsystem.h"
#include "Game/Multiplayer/SFPVPLobby.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "UI/StartWidget.h"
#include "Components/Button.h"
#include "UI/SettingWidget.h"
#include "UnrealClient.h"
#include "Game/Multiplayer/SFPVPBattle.h"
#include "ActorComponent/Stat/HealthComponent.h"
#include "EngineUtils.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"

// Opt-in development harness; each standalone process has its own command mailbox.
// Exercises the same session APIs and server RPCs as the UI, across real travel.
static FAutoConsoleCommandWithWorldAndArgs SFPVPQA(
    TEXT("SF.PVP.QA"), TEXT("SF.PVP.QA <instance>: enable Saved/PVPQA/<instance>.command for multi-process QA."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args,UWorld* World)
    {
        if (!World || Args.IsEmpty()) return;
        TWeakObjectPtr<UGameInstance> GI=World->GetGameInstance();
        const FString Instance=FPaths::GetCleanFilename(Args[0]);
        const FString Path=FPaths::ProjectSavedDir()/TEXT("PVPQA")/(Instance+TEXT(".command"));
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
            [GI,Path,Instance,Last=FString(),Count=0](float) mutable
        {
            if (!GI.IsValid()) return false;
            UWorld* W=GI->GetWorld();if(!W)return true;
            auto* Sub=GI->GetSubsystem<USFPVPSessionSubsystem>();
            auto* PC=Cast<ASFPVPLobbyController>(GI->GetFirstLocalPlayerController());
            auto* State=W->GetGameState<ASFPVPLobbyState>();
            auto* BattlePC=Cast<ASFPVPBattleController>(GI->GetFirstLocalPlayerController());
            FString Command;FFileHelper::LoadFileToString(Command,*Path);Command.TrimStartAndEndInline();
            if(!Command.IsEmpty()&&Command!=Last)
            {
                Last=Command;FString Op,Value;Command.Split(TEXT(" "),&Op,&Value);
                UE_LOG(LogTemp,Display,TEXT("[PVPQA:%s] command=%s"),*Instance,*Command);
                if(Op==TEXT("network"))Sub->SetInternetMode(Value==TEXT("internet"));
                else if(Op==TEXT("login"))Sub->LoginInternet();
                else if(Op==TEXT("host"))Sub->Host(Value);
                else if(Op==TEXT("find"))Sub->FindRooms();
                else if(Op==TEXT("join"))Sub->JoinRoom(FCString::Atoi(*Value));
                else if(Op==TEXT("connect"))Sub->ConnectAddress(Value);
                else if(Op==TEXT("race")&&PC)PC->ServerSetRace(static_cast<ESFRace>(FCString::Atoi(*Value)));
                else if(Op==TEXT("random")&&PC)PC->ServerSetRandomRace();
                else if(Op==TEXT("ready")&&PC)PC->ServerSetReady(Value==TEXT("1"));
                else if(Op==TEXT("start")&&PC)PC->ServerStartBattle();
                else if(Op==TEXT("all")&&BattlePC)BattlePC->SelectAll();
                else if(Op==TEXT("focus")&&BattlePC)BattlePC->FocusArmy();
                else if(Op==TEXT("advance")&&BattlePC)
                {
                    BattlePC->SelectAll();if(auto* BS=W->GetGameState<ASFPVPBattleState>())
                        BattlePC->CommandSelected(ESFUnitCommand::AttackMove,BattlePC->GetBattleTeam()==0?BS->Base1:BS->Base0);
                }
                else if(Op==TEXT("move")&&BattlePC)
                {
                    TArray<FString> Parts;Value.ParseIntoArrayWS(Parts);
                    if(Parts.Num()==4)for(TActorIterator<AUnitBase> It(W);It;++It)if(It->PVPUnitId==FCString::Atoi(*Parts[0]))
                        BattlePC->ServerCommand({*It},ESFUnitCommand::Move,FVector(FCString::Atof(*Parts[1]),FCString::Atof(*Parts[2]),FCString::Atof(*Parts[3])),nullptr);
                }
                else if(Op==TEXT("maptest")&&W->GetAuthGameMode<ASFPVPBattleMode>())
                {
                    const int32 Stage=FCString::Atoi(*Value);
                    if(Stage>=1&&Stage<=7)W->ServerTravel(FString::Printf(TEXT("/Game/PVP/Maps/PVP_Stage%02d?game=/Script/StrikeForge.SFPVPBattleMode"),Stage),true);
                }
                else if(Op==TEXT("leave"))Sub->Leave(false);
                else if(Op==TEXT("title"))Sub->Leave(true);
                else if(Op==TEXT("shot"))FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("PVPQA")/(FPaths::GetCleanFilename(Value)+TEXT(".png")),true,false);
                else if(Op==TEXT("quit"))FPlatformMisc::RequestExit(false);
                else if(Op==TEXT("pvp")||Op==TEXT("pve")||Op==TEXT("settings"))
                {
                    TArray<UUserWidget*> Widgets;UWidgetBlueprintLibrary::GetAllWidgetsOfClass(W,Widgets,UStartWidget::StaticClass(),false);
                    if(!Widgets.IsEmpty())if(auto* B=Cast<UButton>(Widgets[0]->GetWidgetFromName(Op==TEXT("pvp")?TEXT("PVP_Btn"):Op==TEXT("settings")?TEXT("Setting_Btn"):TEXT("Start_Btn"))))B->OnClicked.Broadcast();
                }
                else if(Op==TEXT("settings_close"))
                {
                    TArray<UUserWidget*> Widgets;UWidgetBlueprintLibrary::GetAllWidgetsOfClass(W,Widgets,USettingWidget::StaticClass(),false);
                    for(auto* Widget:Widgets)if(auto* B=Cast<UButton>(Widget->GetWidgetFromName(TEXT("BackUi_Btn"))))B->OnClicked.Broadcast();
                }
            }
            if(++Count%4==0)
            {
                FString Snapshot=FString::Printf(TEXT("map=%s net=%d busy=%d rooms=%d"),*W->GetMapName(),int32(W->GetNetMode()),Sub->IsBusy(),Sub->GetRooms().Num());
                if(State)for(int32 I=0;I<2;++I)if(auto* P=State->GetSeat(I))Snapshot+=FString::Printf(TEXT(" seat%d[race=%d ready=%d]"),I,int32(P->Race),P->bReady);
                if(State)for(int32 I=0;I<2;++I)if(auto* P=State->GetSeat(I))Snapshot+=FString::Printf(TEXT(" random%d=%d"),I,P->bRandomRace);
                if(auto* Local=GI->GetFirstLocalPlayerController())Snapshot+=FString::Printf(TEXT(" ownSeat=%d titleCamera=%s"),Local->GetPlayerState<ASFPVPLobbyPlayerState>()?Local->GetPlayerState<ASFPVPLobbyPlayerState>()->Seat:INDEX_NONE,*GetNameSafe(Local->GetViewTarget()));
                if(auto* BS=Cast<ASFPVPBattleState>(State))Snapshot+=FString::Printf(TEXT(" battleReady=%d battleStatus=%s"),BS->bBattleReady,*BS->BattleStatus);
                if(BattlePC)Snapshot+=FString::Printf(TEXT(" localTeam=%d view=%s camera=%s"),BattlePC->GetMyTeamIndex(),*GetNameSafe(BattlePC->GetViewTarget()),*BattlePC->PlayerCameraManager->GetCameraLocation().ToString());
                FString Units;
                for(TActorIterator<AUnitBase> It(W);It;++It)if(It->PVPUnitId!=INDEX_NONE)
                {
                    const FVector L=It->GetActorLocation();
                    const auto* Anim=It->GetMesh()->GetAnimInstance();
                    Units+=FString::Printf(TEXT("%d,%d,%.2f,%.2f,%.2f,%.2f,%d,%d\n"),It->PVPUnitId,It->GetTeamIndex(),L.X,L.Y,L.Z,It->GetHealthComponent()->GetCurrentHealth(),int32(It->GetCombatState()),Anim&&Anim->GetCurrentActiveMontage()?1:0);
                }
                FFileHelper::SaveStringToFile(Units,*(FPaths::ProjectSavedDir()/TEXT("PVPQA")/(Instance+TEXT(".units"))),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
                Snapshot+=TEXT(" status=")+Sub->GetStatus();
                Snapshot+=FString::Printf(TEXT(" internet=%d configured=%d authenticated=%d"),Sub->IsInternetMode(),Sub->IsInternetConfigured(),Sub->IsInternetLoggedIn());
                UE_LOG(LogTemp,Display,TEXT("[PVPQA:%s] %s"),*Instance,*Snapshot);
                FFileHelper::SaveStringToFile(Snapshot,*(FPaths::ProjectSavedDir()/TEXT("PVPQA")/(Instance+TEXT(".state"))),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
            }
            return true;
        }),.5f);
    }));
#endif
