#include "SFPVPBattle.h"
#include "SFPVPSessionSubsystem.h"
#include "InGame/Controller/SFRTSCameraPawn.h"
#include "InGame/SFInGameHUDWidget.h"
#include "Game/Subsystem/SFBuildingAssetSubsystem.h"
#include "Game/Spawn/SFBuildingSpawnLibrary.h"
#include "Datas/Building/SFBuildingCatalog.h"
#include "Datas/Building/SFBuildingDefinition.h"
#include "Buildings/Building/HouseBuilding/Nexus/SFNexusBuilding.h"
#include "Units/Commander/SFCommander.h"
#include "ActorComponent/Stat/HealthComponent.h"
#include "Net/UnrealNetwork.h"
#include "EngineUtils.h"
#include "NavigationSystem.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/HUD.h"
#include "GameFramework/GameSession.h"
#include "TimerManager.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "ActorComponent/Product/ProductComponent.h"
#include "ActorComponent/Progress/ProgressComponent.h"
#include "UI/LoadingWidget.h"
#include "SFPVPComputerController.h"
#include "Kismet/GameplayStatics.h"
#include "Game/Subsystem/SFCombatSubsystem.h"
#include "Game/Audio/SFBattleMusicComponent.h"

namespace
{
    FSoftObjectPath CommanderPath(ESFRace Race)
    {
        return FSoftObjectPath(Race==ESFRace::Beast?
            TEXT("/Game/Game/Race_Beast/Character/Unit_Commander/BP_Beast_Commander.BP_Beast_Commander_C"):
            TEXT("/Game/Game/Race_Etherial/Character/Unit_Commander/BP_Ether_Commander.BP_Ether_Commander_C"));
    }
}

void ASFPVPBattleState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ASFPVPBattleState,bBattleReady);DOREPLIFETIME(ASFPVPBattleState,BattleStatus);
    DOREPLIFETIME(ASFPVPBattleState,Base0);DOREPLIFETIME(ASFPVPBattleState,Base1);
    DOREPLIFETIME(ASFPVPBattleState,MatchRaces);DOREPLIFETIME(ASFPVPBattleState,Nexuses);
    DOREPLIFETIME(ASFPVPBattleState,Commanders);DOREPLIFETIME(ASFPVPBattleState,WinnerTeam);DOREPLIFETIME(ASFPVPBattleState,Resources);
    DOREPLIFETIME(ASFPVPBattleState,bAssetFailure);
}

bool ASFPVPBattleState::PrepareGameplay()
{
    if(bAssetFailure){FailStageInitialization();return false;}
    if(MatchRaces.Num()!=2)return false;
    if(GetStageLoadingProgress()<.1f)SetStageLoadingProgress(.1f);
    auto* Assets=GetGameInstance()->GetSubsystem<USFBuildingAssetSubsystem>();
    bool bLoaded=true;
    for(const ESFRace Race:MatchRaces)
    {
        if(!RequestedRaces.Contains(Race))
        {
            RequestedRaces.Add(Race);
            Assets->RequestRaceDataAsync(Race,FSFOnRaceBuildingDataReady::CreateWeakLambda(this,[this](ESFRace,bool bOK)
                {if(!bOK){bAssetFailure=true;BattleStatus=TEXT("종족 데이터를 불러오지 못했습니다.");}}));
        }
        auto* Catalog=Assets->GetLoadedCatalog(Race);
        if(!Assets->IsRaceDataReady(Race)||!Catalog){bLoaded=false;continue;}
        TArray<FSoftObjectPath> Definitions;Catalog->GetDefinitionPaths(Definitions);
        for(const auto& Path:Definitions)
        {
            auto* Definition=Cast<USFBuildingDefinition>(Path.ResolveObject());
            if(!Definition){bLoaded=false;continue;}
            // Tier 0 is enough to enter the match. Stream upgrades after both sides arrive.
            for(int32 Tier=0;Tier<(bBattleReady?Definition->GetTierCount():1);++Tier)
            {
                const bool bRequired=Tier==0;
                const FString Key=Path.ToString()+FString::FromInt(Tier);
                if(!bRequired&&RequestedBuildingTiers.Contains(Key))continue;
                if(Definition->AreRuntimeAssetsLoaded(Tier))continue;
                if(bRequired)bLoaded=false;
                if(RequestedBuildingTiers.Contains(Key))continue;
                RequestedBuildingTiers.Add(Key);
                Assets->RequestBuildingAssetsAsync(Definition,Tier,FSFOnBuildingAssetsReady::CreateWeakLambda(this,[this,bRequired](USFBuildingDefinition*,int32,bool bOK)
                    {if(!bOK&&bRequired){bAssetFailure=true;BattleStatus=TEXT("건물 데이터를 불러오지 못했습니다.");}}));
            }
        }
    }
    if(!CommanderLoad)
    {
        TArray<FSoftObjectPath> Paths;for(auto Race:MatchRaces)Paths.AddUnique(CommanderPath(Race));
        CommanderLoad=UAssetManager::GetStreamableManager().RequestAsyncLoad(Paths);
    }
    if(!bLoaded||!CommanderLoad||!CommanderLoad->HasLoadCompleted())return false;
    if(GetStageLoadingProgress()<.9f)SetStageLoadingProgress(.9f);
    if(GetStageState()==ESFStageState::Initializing)
    {
        FString StageRow=GetWorld()->GetMapName();StageRow.RemoveFromStart(TEXT("PVP_"));
        BeginRaceDataLoading();InitializeStage(FName(*StageRow),{});
        for(int32 Team=0;Team<2;++Team)RegisterTeam(Team,MatchRaces[Team],Assets->GetLoadedCatalog(MatchRaces[Team]));
        BeginInitialBuildingAssetsLoading();MarkStageReadyToStart();
    }
    for(FConstPlayerControllerIterator It=GetWorld()->GetPlayerControllerIterator();It;++It)
        if(auto* PC=Cast<ASFPVPBattleController>(It->Get());PC&&PC->GetBattleTeam()>=0)
        {PC->SetMyTeamIndex(PC->GetBattleTeam());RegisterPlayer(PC->GetBattleTeam(),PC);}
    for(int32 Team=0;Team<2;++Team)if(Nexuses.IsValidIndex(Team)&&IsValid(Nexuses[Team]))RegisterNexus(Team,Nexuses[Team]);
    if(bBattleReady&&GetStageState()==ESFStageState::ReadyToStart)StartStage();
    if(!HasAuthority())OnRep_Resources();
    ApplyMatchResult();return true;
}

void ASFPVPBattleState::SyncResources()
{
    if(!HasAuthority())return;
    Resources.SetNum(2);
    for(int32 I=0;I<2;++I)if(const auto* Team=GetTeamState(I))
    {
        auto& R=Resources[I];R.Mineral=Team->GetMineral();R.Ether=Team->GetEther();
        R.MineralRate=Team->GetMineralIncomeRate();R.EtherRate=Team->GetEtherIncomeRate();
        R.Population=Team->GetCurrentPopulation();R.Capacity=Team->GetMaximumPopulation();
    }
}
void ASFPVPBattleState::OnRep_Resources()
{
    if(HasAuthority())return;
    for(int32 I=0;I<Resources.Num();++I)
    {const auto& R=Resources[I];FindOrAddTeam(I)->ApplyNetworkValues(R.Mineral,R.Ether,R.MineralRate,R.EtherRate,R.Population,R.Capacity);}
}
void ASFPVPBattleState::ApplyMatchResult()
{
    if(WinnerTeam==INDEX_NONE||!IsStagePlaying())return;
    const auto* PC=USFCombatSubsystem::Get(this)->GetLocalPlayerController();
    if (!PC || PC->GetMyTeamIndex() < 0) return;
    FinishStage(PC->GetMyTeamIndex()==WinnerTeam?ESFStageResult::Victory:ESFStageResult::Defeat);
}

ASFPVPBattleMode::ASFPVPBattleMode()
{
    PlayerControllerClass=ASFPVPBattleController::StaticClass();PlayerStateClass=ASFPVPLobbyPlayerState::StaticClass();
    GameStateClass=ASFPVPBattleState::StaticClass();HUDClass=AHUD::StaticClass();
    DefaultPawnClass=ASFRTSCameraPawn::StaticClass();bUseSeamlessTravel=true;
}
void ASFPVPBattleMode::BeginPlay()
{
    Super::BeginPlay();WaitStarted=GetWorld()->GetTimeSeconds();
    GetWorldTimerManager().SetTimer(SpawnTimer,this,&ThisClass::TrySpawnArmies,.25f,true);
}
void ASFPVPBattleMode::InitGameState()
{
    Super::InitGameState();
    auto* S=GetGameState<ASFPVPBattleState>();
    if(UGameplayStatics::ParseOption(OptionsString,TEXT("SFAI"))!=TEXT("1"))return;
    S->bAIEnabled=true;S->RoomName=TEXT("AI 연습 대전");
    Computer=GetWorld()->SpawnActor<ASFPVPComputerController>();
    if(!Computer){S->FailPreparation(TEXT("AI 지휘관을 생성하지 못했습니다."));return;}
    auto* Seat=Computer->GetPlayerState<ASFPVPLobbyPlayerState>();
    if(!Seat)Seat=GetWorld()->SpawnActor<ASFPVPLobbyPlayerState>();
    if(!Seat){S->FailPreparation(TEXT("AI 참가 정보를 생성하지 못했습니다."));return;}
    Seat->Race=UGameplayStatics::ParseOption(OptionsString,TEXT("SFAIRace"))==TEXT("1")?ESFRace::Beast:ESFRace::Human;
    Computer->InitializeOpponent(Seat);
}
void ASFPVPBattleMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
    if(auto* PC=Cast<ASFPVPBattleController>(NewPlayer))PC->SetMyTeamIndex(PC->GetBattleTeam());
    if(!NewPlayer->GetPawn())
    {
        UClass* CameraClass=LoadClass<ASFRTSCameraPawn>(nullptr,
            TEXT("/Game/Game/IN_UI/JSYBluePrints/BP_SFRTSCameraPawn.BP_SFRTSCameraPawn_C"));
        if(!CameraClass)
        {
            GetGameState<ASFPVPBattleState>()->FailPreparation(TEXT("PVE 카메라 입력 설정을 불러오지 못했습니다."));
            return;
        }
        FActorSpawnParameters Params;Params.Owner=NewPlayer;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        if(auto* Camera=GetWorld()->SpawnActor<ASFRTSCameraPawn>(CameraClass,FVector::ZeroVector,FRotator::ZeroRotator,Params))
        {Camera->SetReplicates(true);Camera->bOnlyRelevantToOwner=true;Camera->SetReplicateMovement(false);NewPlayer->Possess(Camera);}
    }
}
void ASFPVPBattleMode::PreLogin(const FString&,const FString&,const FUniqueNetIdRepl&,FString& Error)
{Error=TEXT("PVP_MATCH_ALREADY_STARTED");}

void ASFPVPBattleMode::GetSeamlessTravelActorList(bool bToTransition,TArray<AActor*>& Actors)
{
    if(auto* S=GameState.Get())
    {
        const auto Players=S->PlayerArray;
        for(APlayerState* P:Players)if(P&&P->IsABot())S->RemovePlayerState(P);
    }
    Super::GetSeamlessTravelActorList(bToTransition,Actors);
}

bool ASFPVPBattleMode::FindCampaignBases(FTransform& A,FTransform& B) const
{
    auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    if(!Nav||Nav->IsNavigationBuildInProgress())return false;
    TArray<FTransform> Authored[2];
    for(TActorIterator<AActor> It(GetWorld());It;++It)
        for(int32 Team=0;Team<2;++Team)
            if(It->ActorHasTag(Team==0?TEXT("PVP_Base0"):TEXT("PVP_Base1")))Authored[Team].Add(It->GetActorTransform());
    // These markers copy the campaign Nexus anchors, including their facing.
    // A baked navigation hole under a Nexus is expected; never move the base to a random tile.
    if(Authored[0].Num()!=1||Authored[1].Num()!=1)return false;
    A=Authored[0][0];B=Authored[1][0];
    A.SetScale3D(FVector::OneVector);B.SetScale3D(FVector::OneVector);
    return !A.ContainsNaN()&&!B.ContainsNaN()&&FVector::Dist2D(A.GetLocation(),B.GetLocation())>1000;
}

void ASFPVPBattleMode::TrySpawnArmies()
{
    auto* S=GetGameState<ASFPVPBattleState>();if(!S)return;
    S->SyncResources();S->ApplyMatchResult();if(S->bBattleReady||S->WinnerTeam!=INDEX_NONE)return;
    if(GetWorld()->GetTimeSeconds()-WaitStarted>120)
    {S->FailPreparation(TEXT("전장 준비 시간이 초과되었습니다. 다시 방을 만들어 주세요."));S->PrepareGameplay();GetWorldTimerManager().ClearTimer(SpawnTimer);return;}
    if(!S->GetSeat(0)||!S->GetSeat(1))return;
    if(S->MatchRaces.Num()!=2){S->MatchRaces={S->GetSeat(0)->Race,S->GetSeat(1)->Race};S->ForceNetUpdate();}
    if(!S->PrepareGameplay())return;
    if(Computer)S->RegisterPlayer(1,Computer);
    int32 Ready=0;
    for(FConstPlayerControllerIterator It=GetWorld()->GetPlayerControllerIterator();It;++It)
        if(auto* PC=Cast<ASFPVPBattleController>(It->Get());PC&&PC->HasClientLoadedCurrentWorld()&&PC->IsPresentationReady())++Ready;
    if(Ready!=(S->bAIEnabled?1:2))return;
    FTransform Bases[2];if(!FindCampaignBases(Bases[0],Bases[1]))
    {
        S->BattleStatus=TEXT("PVE 넥서스 시작 위치와 전장을 준비하고 있습니다.");
        return;
    }
    auto* Assets=GetGameInstance()->GetSubsystem<USFBuildingAssetSubsystem>();
    auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    const auto BeforeSpawn=S->Resources;
    TArray<AActor*> Spawned;S->Nexuses.SetNum(2);S->Commanders.SetNum(2);
    for(int32 Team=0;Team<2;++Team)
    {
        auto* Def=Assets->GetLoadedCatalog(S->MatchRaces[Team])->NexusDefinition.Get();
        const FTransform TM=Bases[Team];
        auto* Nexus=Cast<ASFNexusBuilding>(USFBuildingSpawnLibrary::SpawnBuildingFromDefinition(this,Def,Team,TM,ESFBuildingSpawnMode::ViewPortPlacedMode,0));
        if(!IsValid(Nexus)||Nexus->IsActorBeingDestroyed())break;
        Nexus->SetReplicates(true);Nexus->bAlwaysRelevant=true;S->Nexuses[Team]=Nexus;Spawned.Add(Nexus);
        const FVector Forward=(Bases[1-Team].GetLocation()-Bases[Team].GetLocation()).GetSafeNormal2D();
        const FVector Extent=Nexus->GetBuildBounds().TransformBy(Nexus->GetActorTransform()).GetBox().GetExtent();
        const float Clearance=FMath::Abs(Forward.X)*Extent.X+FMath::Abs(Forward.Y)*Extent.Y+220;
        FNavLocation Ground;
        if(!Nav->ProjectPointToNavigation(Nexus->GetActorLocation()+Forward*Clearance,Ground,FVector(500,500,500)))break;
        UClass* Class=Cast<UClass>(CommanderPath(S->MatchRaces[Team]).ResolveObject());if(!Class)break;
        const float Half=Class->GetDefaultObject<ASFCommander>()->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        const FTransform CT(Forward.Rotation(),Ground.Location+FVector(0,0,Half+3));
        auto* Commander=GetWorld()->SpawnActorDeferred<ASFCommander>(Class,CT,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
        if(!Commander)break;
        Commander->InitializePlayerCommander(Team);Commander->SetReplicates(true);Commander->SetReplicateMovement(true);
        Commander->bAlwaysRelevant=true;Commander->SetNetUpdateFrequency(30);Commander->FinishSpawning(CT);
        S->Commanders[Team]=Commander;Spawned.Add(Commander);
        if(auto* PC=S->GetPlayer(Team))PC->RegisterCommander(Commander);
    }
    if(Spawned.Num()!=4)
    {
        for(auto* Actor:Spawned)Actor->Destroy();S->Nexuses.Reset();S->Commanders.Reset();
        for(int32 Team=0;Team<BeforeSpawn.Num();++Team)if(auto* Values=S->GetTeamState(Team))
        {const auto& R=BeforeSpawn[Team];Values->ApplyNetworkValues(R.Mineral,R.Ether,R.MineralRate,R.EtherRate,R.Population,R.Capacity);}
        S->BattleStatus=TEXT("시작 위치를 다시 준비하고 있습니다.");return;
    }
    S->Base0=S->Nexuses[0]->GetActorLocation();S->Base1=S->Nexuses[1]->GetActorLocation();
    for(int32 Team=0;Team<2;++Team)
    {
        S->AddResource(Team,EResourceType::Mineral,500-S->GetMineral(Team));
        S->AddResource(Team,EResourceType::Ether,-S->GetEther(Team));
    }
    S->bBattleReady=true;S->BattleStatus.Empty();S->SyncResources();S->PrepareGameplay();S->ForceNetUpdate();
    UE_LOG(LogTemp,Display,TEXT("[PVPBattle] Nexuses=2 Commanders=2 bases=%s / %s"),*S->Base0.ToString(),*S->Base1.ToString());
}
void ASFPVPBattleMode::NexusDestroyed(int32 Team)
{
    auto* S=GetGameState<ASFPVPBattleState>();
    if(!S||!S->bBattleReady||S->WinnerTeam!=INDEX_NONE||(Team!=0&&Team!=1))return;
    S->WinnerTeam=1-Team;S->ForceNetUpdate();S->ApplyMatchResult();
    for(TActorIterator<AUnitBase> It(GetWorld());It;++It)if(It->PVPUnitId!=INDEX_NONE)
    {It->ExecuteCommand(ESFUnitCommand::Stop,It->GetActorLocation(),nullptr);It->SetActorTickEnabled(false);}
    for(auto Commander:S->Commanders)if(IsValid(Commander))IMovable::Execute_StopMove(Commander.Get());
    for(TActorIterator<ASFBaseBuilding> It(GetWorld());It;++It)
    {
        if(auto* Product=It->FindComponentByClass<UProductComponent>())Product->PauseProductLine();
        if(auto* Progress=It->FindComponentByClass<UProgressComponent>())Progress->StopProgress();
    }
    UE_LOG(LogTemp,Display,TEXT("[PVPBattle] Nexus destroyed=%d winner=%d"),Team,S->WinnerTeam);
}
void ASFPVPBattleMode::Logout(AController* Exiting)
{
    auto* S=GetGameState<ASFPVPBattleState>();const auto* PS=Exiting->GetPlayerState<ASFPVPLobbyPlayerState>();
    if(!GetWorld()->bIsTearingDown&&S&&S->bBattleReady&&S->WinnerTeam==INDEX_NONE&&PS){S->WinnerTeam=1-PS->Seat;S->ForceNetUpdate();S->ApplyMatchResult();}
    GetGameInstance()->GetSubsystem<USFPVPSessionSubsystem>()->UnregisterRoomPlayer(Exiting);Super::Logout(Exiting);
}

int32 ASFPVPBattleController::GetBattleTeam() const
{const auto* P=GetPlayerState<ASFPVPLobbyPlayerState>();return P?P->Seat:INDEX_NONE;}
void ASFPVPBattleController::BeginPlay()
{
    SetMyTeamIndex(GetBattleTeam());ConfigurePVEGameplayProfile();Super::BeginPlay();
    if(IsLocalController())if(UClass* Class=LoadClass<ULoadingWidget>(nullptr,TEXT("/Game/Core/UI/WBP_Loading.WBP_Loading_C")))
    {
        LoadingWidget=CreateWidget<ULoadingWidget>(this,Class);
        if(LoadingWidget){LoadingWidget->AddToViewport(1000);LoadingWidget->SetStageTitle(NSLOCTEXT("PVP","LoadingBattle","결투의 전장"));}
    }
}
void ASFPVPBattleController::SetupInputComponent()
{ConfigurePVEGameplayProfile();Super::SetupInputComponent();}
void ASFPVPBattleController::EndPlay(const EEndPlayReason::Type Reason)
 {if(IsValid(BattleMusic))BattleMusic->StopPlaylist();Selected.Empty();if(LoadingWidget)LoadingWidget->RemoveFromParent();Super::EndPlay(Reason);}
void ASFPVPBattleController::PlayerTick(float DeltaTime)
{
    SetMyTeamIndex(GetBattleTeam());Super::PlayerTick(DeltaTime);if(!IsLocalController())return;
    auto* S=GetWorld()->GetGameState<ASFPVPBattleState>();if(!S||GetBattleTeam()<0)return;
    // PostSeamlessTravel destroys audio components whose Sound is still null.
    // Prepare our asynchronous playlist after that controller travel cleanup.
    if(!IsValid(BattleMusic))
    {
        BattleMusic=NewObject<USFBattleMusicComponent>(this);
        AddInstanceComponent(BattleMusic);
        BattleMusic->RegisterComponent();
        BattleMusic->PreparePlaylist();
    }
    if(S->IsStagePlaying()&&S->WinnerTeam==INDEX_NONE)BattleMusic->StartPlaylist();else BattleMusic->StopPlaylist();
    if(S->PrepareGameplay()&&!bSentReady){bSentReady=true;ServerPresentationReady();}
    if(auto* Camera=Cast<ASFRTSCameraPawn>(GetPawn());Camera&&S->bBattleReady)
    {
        if(!bFocused)
        {
            FBox Bounds(ForceInit);for(TActorIterator<ANavMeshBoundsVolume> It(GetWorld());It;++It)Bounds+=It->GetComponentsBoundingBox(true);
            if(Bounds.IsValid)Camera->SetStageCameraBounds(Bounds,100.f);
            Camera->ApplyPVPZoom();
            Camera->FocusOnWorldLocation(GetBattleTeam()==0?S->Base0:S->Base1);bFocused=true;
        }
        if(GetViewTarget()!=Camera)SetViewTarget(Camera);
        if (bFocused && S->IsStagePlaying()) GetGameInstance()->GetSubsystem<USFPVPSessionSubsystem>()->HideTravelLoading();
    }
    if(S->Commanders.IsValidIndex(GetBattleTeam())&&IsValid(S->Commanders[GetBattleTeam()]))
        if(GetMyCommander()!=S->Commanders[GetBattleTeam()])RegisterCommander(S->Commanders[GetBattleTeam()]);
    Selected.Empty();if(const auto* HUD=GetInGameHUDWidget())for(const auto& Actor:HUD->GetSelectedActors())if(auto* Unit=Cast<AUnitBase>(Actor.Get()))Selected.Add(Unit);
}
void ASFPVPBattleController::ServerPresentationReady_Implementation(){bPresentationReady=true;}
void ASFPVPBattleController::SelectAll(){if(auto* HUD=GetInGameHUDWidget())HUD->SelectAllOwnedUnits();}
void ASFPVPBattleController::CommandSelected(ESFUnitCommand Command,FVector Location,AUnitBase* Target)
{
    TArray<AUnitBase*> Units;for(auto Unit:Selected)if(Unit.IsValid())Units.Add(Unit.Get());ServerCommand(Units,Command,Location,Target);
}
void ASFPVPBattleController::ServerCommand_Implementation(const TArray<AUnitBase*>& Units,ESFUnitCommand Command,FVector_NetQuantize Location,AUnitBase* Target)
{
    TArray<AActor*> Actors;for(auto* Unit:Units)Actors.Add(Unit);
    ServerGameplayAction(Command==ESFUnitCommand::Stop?ESFGameplayAction::Stop:Command==ESFUnitCommand::Attack?ESFGameplayAction::Attack:ESFGameplayAction::Move,Actors,Location,Target);
}
void ASFPVPBattleController::FocusArmy()
{if(auto* Camera=Cast<ASFRTSCameraPawn>(GetPawn()))if(auto* S=GetWorld()->GetGameState<ASFPVPBattleState>())Camera->FocusOnWorldLocation(GetMyCommander()?GetMyCommander()->GetActorLocation():GetBattleTeam()==0?S->Base0:S->Base1);}
void ASFPVPBattleController::LeaveBattle(){GetGameInstance()->GetSubsystem<USFPVPSessionSubsystem>()->Leave(false);}
