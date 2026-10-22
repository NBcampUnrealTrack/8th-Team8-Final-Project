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
#include "InGame/SFInGameHUDWidget.h"
#include "Units/Commander/SFCommander.h"
#include "Buildings/Building/HouseBuilding/Nexus/SFNexusBuilding.h"
#include "Datas/Building/SFBuildingCatalog.h"
#include "Datas/Building/SFBuildingDefinition.h"
#include "TimerManager.h"
#include "Engine/GameViewportClient.h"
#include "ActorComponent/Stat/HealthComponent.h"
#include "EngineUtils.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "InGame/Controller/SFRTSCameraPawn.h"
#include "Buildings/Building/ProductBuilding/SFProductionBuilding.h"
#include "Interfaces/Building/Productable.h"
#include "ActorComponent/Product/ProductComponent.h"
#include "Game/GameState/SFTeamState.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "UObject/UnrealType.h"
#include "SFPVPVideoQA.h"
#include "SFPVPPresentationQA.h"
#include "Game/Multiplayer/SFPVPComputerController.h"

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
            if(BattlePC)if(auto* Camera=Cast<ASFRTSCameraPawn>(BattlePC->GetPawn()))Camera->SetEdgeScrollEnabled(false);
            FString Command;FFileHelper::LoadFileToString(Command,*Path);Command.TrimStartAndEndInline();
            if(!Command.IsEmpty()&&Command!=Last)
            {
                Last=Command;FString Op,Value;Command.Split(TEXT(" "),&Op,&Value);
                UE_LOG(LogTemp,Display,TEXT("[PVPQA:%s] command=%s"),*Instance,*Command);
                if(Op==TEXT("network"))Sub->SetInternetMode(Value==TEXT("internet"));
                else if(Op==TEXT("login"))Sub->LoginInternet();
                else if(Op==TEXT("host"))Sub->Host(Value);
                else if(Op==TEXT("practice"))Sub->HostPractice();
                else if(Op==TEXT("ai")&&PC)PC->ServerSetAIOpponent(Value==TEXT("1"));
                else if(Op==TEXT("ai_race")&&PC)PC->ServerCycleAIRace();
                else if(Op==TEXT("find"))Sub->FindRooms();
                else if(Op==TEXT("join"))Sub->JoinRoom(FCString::Atoi(*Value));
                else if(Op==TEXT("connect"))Sub->ConnectAddress(Value);
                else if(Op==TEXT("race")&&PC)PC->ServerSetRace(static_cast<ESFRace>(FCString::Atoi(*Value)));
                else if(Op==TEXT("random")&&PC)PC->ServerSetRandomRace();
                else if(Op==TEXT("ready")&&PC)PC->ServerSetReady(Value==TEXT("1"));
                else if(Op==TEXT("start")&&PC)PC->ServerStartBattle();
                else if(SFPresentationQACommand(W,Op,Value)){}
                else if(Op==TEXT("all")&&BattlePC)BattlePC->SelectAll();
                else if(Op==TEXT("focus")&&BattlePC)BattlePC->FocusArmy();
                else if(Op==TEXT("view")&&BattlePC)
                {
                    TArray<FString> XYZ;Value.ParseIntoArrayWS(XYZ);
                    if(XYZ.Num()==3)if(auto* Camera=Cast<ASFRTSCameraPawn>(BattlePC->GetPawn()))
                        Camera->FocusOnWorldLocation(FVector(FCString::Atof(*XYZ[0]),FCString::Atof(*XYZ[1]),FCString::Atof(*XYZ[2])));
                }
                else if(Op==TEXT("record"))
                {
                    FString Folder,Duration;Value.Split(TEXT(" "),&Folder,&Duration);
                    SFStartPVPVideoQA(W,Folder,FCString::Atof(*Duration));
                }
                else if(Op==TEXT("input_audit")&&BattlePC)
                {
                    const auto* ProfileClass=LoadClass<ASFInGamePlayerController>(nullptr,TEXT("/Game/Game/IN_UI/JSYBluePrints/BP_SFInGamePlayerController.BP_SFInGamePlayerController_C"));
                    const auto* Profile=ProfileClass?ProfileClass->GetDefaultObject<ASFInGamePlayerController>():nullptr;
                    int32 Mismatches=Profile?0:1;FString Report;
                    if(Profile)for(TFieldIterator<FObjectPropertyBase> It(ASFInGamePlayerController::StaticClass());It;++It)
                    {
                        if(!It->PropertyClass->IsChildOf(UInputAction::StaticClass())&&!It->PropertyClass->IsChildOf(UInputMappingContext::StaticClass()))continue;
                        auto* Expected=It->GetObjectPropertyValue_InContainer(Profile);auto* Actual=It->GetObjectPropertyValue_InContainer(BattlePC);
                        Mismatches+=Expected!=Actual;
                        Report+=FString::Printf(TEXT("%s\tPVE=%s\tPVP=%s\n"),*It->GetName(),*GetPathNameSafe(Expected),*GetPathNameSafe(Actual));
                        if(const auto* Context=Cast<UInputMappingContext>(Actual))for(const auto& Mapping:Context->GetMappings())
                            Report+=FString::Printf(TEXT("  %s\t%s\n"),*Mapping.Key.ToString(),*GetNameSafe(Mapping.Action));
                    }
                    for(const TCHAR* Name:{TEXT("BaseMappingPriority"),TEXT("GameplayMappingPriority"),TEXT("BuildPlacementMappingPriority"),TEXT("BuildModeMappingPriority"),TEXT("bLockMouseToViewport"),TEXT("SelectionDragThreshold"),TEXT("FormationSpacing")})
                    {
                        auto* Property=FindFProperty<FProperty>(ASFInGamePlayerController::StaticClass(),Name);
                        const bool bEqual=Profile&&Property&&Property->Identical_InContainer(Profile,BattlePC);
                        Mismatches+=!bEqual;Report+=FString::Printf(TEXT("%s\tidentical=%d\n"),Name,bEqual);
                    }
                    const auto* CameraClass=LoadClass<ASFRTSCameraPawn>(nullptr,TEXT("/Game/Game/IN_UI/JSYBluePrints/BP_SFRTSCameraPawn.BP_SFRTSCameraPawn_C"));
                    const bool bSameCamera=CameraClass&&BattlePC->GetPawn()&&BattlePC->GetPawn()->GetClass()==CameraClass;
                    Mismatches+=!bSameCamera;Report+=FString::Printf(TEXT("PVE_CAMERA=%d\nMISMATCHES=%d\n"),bSameCamera,Mismatches);
                    FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectSavedDir()/TEXT("PVPQA")/(Instance+TEXT(".inputs"))),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
                    UE_LOG(LogTemp,Display,TEXT("[PVPQA:%s] PVE input mismatches=%d"),*Instance,Mismatches);
                }
                else if(Op==TEXT("pve_select")&&BattlePC)
                {if(auto* HUD=BattlePC->GetInGameHUDWidget())HUD->SelectActor(BattlePC->GetMyCommander());}
                else if(Op==TEXT("pve_move")&&BattlePC)
                {
                    TArray<FString> XYZ;Value.ParseIntoArrayWS(XYZ);
                    if(XYZ.Num()==3)BattlePC->IssueMoveCommand(FVector(FCString::Atof(*XYZ[0]),FCString::Atof(*XYZ[1]),FCString::Atof(*XYZ[2])));
                }
                else if(Op==TEXT("key")&&BattlePC)
                {
                    FString Key,Event;Value.Split(TEXT(" "),&Key,&Event);
                    BattlePC->InputKey(FKey(*Key),Event==TEXT("down")?IE_Pressed:IE_Released,Event==TEXT("down")?1.f:0.f,false);
                }
                else if(Op==TEXT("mouse_actor")&&BattlePC)
                {
                    FString Kind,TeamText;Value.Split(TEXT(" "),&Kind,&TeamText);const int32 Team=FCString::Atoi(*TeamText);
                    auto* BS=W->GetGameState<ASFPVPBattleState>();AActor* Actor=nullptr;
                    if(BS&&Team>=0&&Team<2)
                    {if(Kind==TEXT("commander")&&BS->Commanders.IsValidIndex(Team))Actor=BS->Commanders[Team];if(Kind==TEXT("nexus")&&BS->Nexuses.IsValidIndex(Team))Actor=BS->Nexuses[Team];}
                    FVector2D Screen;
                    if(IsValid(Actor)&&BattlePC->ProjectWorldLocationToScreen(Actor->GetActorLocation(),Screen)&&W->GetGameViewport())
                        W->GetGameViewport()->Viewport->SetMouse(FMath::RoundToInt(Screen.X),FMath::RoundToInt(Screen.Y));
                }
                else if(Op==TEXT("mouse")&&BattlePC)
                {TArray<FString> XY;Value.ParseIntoArrayWS(XY);if(XY.Num()==2)W->GetGameViewport()->Viewport->SetMouse(FCString::Atoi(*XY[0]),FCString::Atoi(*XY[1]));}
                else if(Op==TEXT("mouse_world")&&BattlePC)
                {
                    TArray<FString> XYZ;Value.ParseIntoArrayWS(XYZ);FVector2D Screen;
                    if(XYZ.Num()==3&&BattlePC->ProjectWorldLocationToScreen(FVector(FCString::Atof(*XYZ[0]),FCString::Atof(*XYZ[1]),FCString::Atof(*XYZ[2])),Screen))
                        W->GetGameViewport()->Viewport->SetMouse(FMath::RoundToInt(Screen.X),FMath::RoundToInt(Screen.Y));
                }
                else if(Op==TEXT("build")&&BattlePC)
                {
                    TArray<FString> Parts;Value.ParseIntoArrayWS(Parts);
                    if(Parts.Num()==4)BattlePC->ServerPlaceGameplayBuilding(FName(*Parts[0]),FTransform(FRotator(0,90,0),FVector(FCString::Atof(*Parts[1]),FCString::Atof(*Parts[2]),FCString::Atof(*Parts[3]))));
                }
                else if(Op==TEXT("produce")&&BattlePC)
                {for(TActorIterator<ASFProductionBuilding> It(W);It;++It)if(It->GetTeamIndex()==BattlePC->GetMyTeamIndex())IProductable::Execute_ProductUnits(*It,FName(*Value));}
                else if(Op==TEXT("attack_nexus")&&BattlePC)
                {
                    auto* BS=W->GetGameState<ASFPVPBattleState>();const int32 Team=FCString::Atoi(*Value);
                    if(BS&&BS->Nexuses.IsValidIndex(Team)&&BS->Nexuses[Team])
                    {TArray<AActor*> Units;for(TActorIterator<AUnitBase> It(W);It;++It)if(It->GetTeamIndex()==BattlePC->GetMyTeamIndex())Units.Add(*It);
                    BattlePC->ServerGameplayAction(ESFGameplayAction::Attack,Units,BS->Nexuses[Team]->GetActorLocation(),BS->Nexuses[Team]);}
                }
                else if(Op==TEXT("nexus_damage")&&W->GetAuthGameMode<ASFPVPBattleMode>())
                {if(auto* BS=W->GetGameState<ASFPVPBattleState>())if(BS->Nexuses.IsValidIndex(FCString::Atoi(*Value)))if(auto* N=BS->Nexuses[FCString::Atoi(*Value)].Get())N->FindComponentByClass<UHealthComponent>()->ApplyDamage(1000000.f);}
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
                    const auto* BS=W->GetGameState<ASFPVPBattleState>();
                    const FString AIOptions=BS&&BS->bAIEnabled?FString::Printf(TEXT("?SFAI=1?SFAIRace=%d"),int32(BS->GetSeat(1)->Race)):FString();
                    if(Stage>=1&&Stage<=7)W->ServerTravel(FString::Printf(TEXT("/Game/PVP/Maps/PVP_Stage%02d?game=/Script/StrikeForge.SFPVPBattleMode"),Stage)+AIOptions,true);
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
            if(BattlePC)SFWritePresentationQA(W,Instance);
            if(++Count%4==0)
            {
                FString Snapshot=FString::Printf(TEXT("map=%s net=%d busy=%d rooms=%d"),*W->GetMapName(),int32(W->GetNetMode()),Sub->IsBusy(),Sub->GetRooms().Num());
                if(State)for(int32 I=0;I<2;++I)if(auto* P=State->GetSeat(I))Snapshot+=FString::Printf(TEXT(" seat%d[race=%d ready=%d]"),I,int32(P->Race),P->bReady);
                if(State)for(int32 I=0;I<2;++I)if(auto* P=State->GetSeat(I))Snapshot+=FString::Printf(TEXT(" random%d=%d"),I,P->bRandomRace);
                if(State)Snapshot+=FString::Printf(TEXT(" ai=%d"),State->bAIEnabled);
                for(TActorIterator<ASFPVPComputerController> It(W);It;++It)Snapshot+=FString::Printf(TEXT(" aiWaves=%d aiLocal=%d"),It->GetAttackWaveCount(),It->IsLocalController());
                if(auto* Local=GI->GetFirstLocalPlayerController())Snapshot+=FString::Printf(TEXT(" ownSeat=%d titleCamera=%s"),Local->GetPlayerState<ASFPVPLobbyPlayerState>()?Local->GetPlayerState<ASFPVPLobbyPlayerState>()->Seat:INDEX_NONE,*GetNameSafe(Local->GetViewTarget()));
                if(auto* BS=Cast<ASFPVPBattleState>(State))
                {
                    Snapshot+=FString::Printf(TEXT(" battleReady=%d winner=%d result=%d battleStatus=%s"),BS->bBattleReady,BS->WinnerTeam,int32(BS->GetStageResult()),*BS->BattleStatus);
                    for(int32 Team=0;Team<2;++Team)if(auto* R=BS->GetTeamState(Team))Snapshot+=FString::Printf(TEXT(" resources%d=%d/%d population%d=%d/%d"),Team,R->GetMineral(),R->GetEther(),Team,R->GetCurrentPopulation(),R->GetMaximumPopulation());
                    FString Actors,Screens,Buildings,Products;
                    for(int32 Team=0;Team<2;++Team)
                    {
                        AActor* Items[]={BS->Nexuses.IsValidIndex(Team)?static_cast<AActor*>(BS->Nexuses[Team]):nullptr,BS->Commanders.IsValidIndex(Team)?static_cast<AActor*>(BS->Commanders[Team]):nullptr};
                        for(int32 Type=0;Type<2;++Type)if(auto* Actor=Items[Type];IsValid(Actor))
                        {const FVector P=Actor->GetActorLocation();Actors+=FString::Printf(TEXT("%s,%d,%.2f,%.2f,%.2f,%.2f\n"),Type==0?TEXT("nexus"):TEXT("commander"),Team,P.X,P.Y,P.Z,Actor->FindComponentByClass<UHealthComponent>()->GetCurrentHealth());
                        FVector2D Screen;if(BattlePC&&BattlePC->ProjectWorldLocationToScreen(P,Screen))Screens+=FString::Printf(TEXT("%s,%d,%.2f,%.2f\n"),Type==0?TEXT("nexus"):TEXT("commander"),Team,Screen.X,Screen.Y);}
                    }
                    FFileHelper::SaveStringToFile(Actors,*(FPaths::ProjectSavedDir()/TEXT("PVPQA")/(Instance+TEXT(".actors"))),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
                    FFileHelper::SaveStringToFile(Screens,*(FPaths::ProjectSavedDir()/TEXT("PVPQA")/(Instance+TEXT(".screens"))),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
                    for(TActorIterator<ASFBaseBuilding> It(W);It;++It)
                    {auto* Def=It->GetBuildingDefinition();const FVector P=It->GetActorLocation();auto* Product=It->FindComponentByClass<UProductComponent>();
                    Buildings+=FString::Printf(TEXT("%s,%d,%s,%d,%.1f,%.1f,%.1f,%d\n"),*It->GetName(),It->GetTeamIndex(),Def?*Def->BuildingId.ToString():TEXT("none"),It->IsTierActivated()?1:0,P.X,P.Y,P.Z,Product?Product->GetProductLine().Num():0);
                    if(auto* Production=Cast<ASFProductionBuilding>(*It))for(FName Row:IProductable::Execute_GetProductableData(Production).ProductableUnitFNames)Products+=FString::Printf(TEXT("%d,%s\n"),It->GetTeamIndex(),*Row.ToString());}
                    FFileHelper::SaveStringToFile(Buildings,*(FPaths::ProjectSavedDir()/TEXT("PVPQA")/(Instance+TEXT(".buildings"))),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
                    FFileHelper::SaveStringToFile(Products,*(FPaths::ProjectSavedDir()/TEXT("PVPQA")/(Instance+TEXT(".products"))),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
                    if(BattlePC)
                    {
                        auto* HUD=BattlePC->GetInGameHUDWidget();
                        Snapshot+=FString::Printf(TEXT(" hud=%d hudVisibility=%d selected=%d commander=%d stage=%d"),HUD!=nullptr,HUD?int32(HUD->GetVisibility()):-1,HUD?HUD->GetSelectedActors().Num():0,BattlePC->GetMyCommander()!=nullptr,int32(BS->GetStageState()));
                        FString CatalogText;if(auto* C=BS->GetTeamBuildingCatalog(BattlePC->GetBattleTeam()))
                        for(auto Ref:C->BuildableDefinitions)if(auto* Def=Ref.Get())CatalogText+=FString::Printf(TEXT("%s,%d\n"),*Def->BuildingId.ToString(),int32(Def->BuildingType));
                        FFileHelper::SaveStringToFile(CatalogText,*(FPaths::ProjectSavedDir()/TEXT("PVPQA")/(Instance+TEXT(".catalog"))),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
                    }
                }
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
