#include "SFPVPComputerController.h"
#include "SFPVPBattle.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "Game/Subsystem/SFBuildingGridSubsystem.h"
#include "Game/Subsystem/SFDataTableManager.h"
#include "Game/Subsystem/SFFogOfWarSubsystem.h"
#include "Game/GameState/SFTeamState.h"
#include "Game/Spawn/SFBuildingSpawnLibrary.h"
#include "Datas/Building/SFBuildingCatalog.h"
#include "Datas/Building/SFBuildingDefinition.h"
#include "Buildings/Building/ProductBuilding/SFProductionBuilding.h"
#include "Buildings/Building/HouseBuilding/Nexus/SFNexusBuilding.h"
#include "Buildings/Resource/ResourceBase.h"
#include "Interfaces/Building/Productable.h"
#include "ActorComponent/Product/ProductComponent.h"
#include "ActorComponent/Combat/CombatTargetComponent.h"
#include "Units/Commander/SFCommander.h"
#include "Units/Unit/UnitBase.h"

ASFPVPComputerController::ASFPVPComputerController()
{
    SetMyTeamIndex(1);SetReplicates(false);
    bShowMouseCursor=false;bEnableClickEvents=false;bEnableMouseOverEvents=false;
}
void ASFPVPComputerController::BeginPlay()
{
    Super::BeginPlay();
    if(HasAuthority())GetWorldTimerManager().SetTimer(ThinkTimer,this,&ThisClass::Think,2.f,true);
}
void ASFPVPComputerController::EndPlay(const EEndPlayReason::Type Reason)
{GetWorldTimerManager().ClearTimer(ThinkTimer);Super::EndPlay(Reason);}
void ASFPVPComputerController::InitializeOpponent(ASFPVPLobbyPlayerState* Seat)
{
    SetPlayerState(Seat);Seat->SetOwner(this);Seat->SetIsABot(true);
    Seat->Seat=1;Seat->bReady=true;Seat->SetPlayerName(TEXT("AI 지휘관"));Seat->ForceNetUpdate();
}

bool ASFPVPComputerController::TryBuild(ESFBuildingType Type)
{
    auto* S=GetWorld()->GetGameState<ASFPVPBattleState>();auto* Catalog=S?S->GetTeamBuildingCatalog(1):nullptr;
    auto* Commander=GetMyCommander();if(!Catalog||!Commander||Commander->IsAwaitingRespawn())return false;
    USFBuildingDefinition* Def=nullptr;
    for(const auto& Ref:Catalog->BuildableDefinitions)if(auto* D=Ref.Get();D&&D->BuildingType==Type){Def=D;break;}
    if(!Def||!Def->FindTier(0)||!S->CanAffordResource(1,Def->FindTier(0)->Construction.ConstructCost)||
        S->GetNexusTier(1)+1<Def->RequiredNexusTier||!S->AreBuildingRequirementsMet(1,Def->BuildingId)||!S->IsBuildingAllowedInCurrentStage(Def->BuildingId))return false;
    auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    auto* Grid=GetWorld()->GetSubsystem<USFBuildingGridSubsystem>();if(!Nav||!Grid)return false;
    const bool bMine=Type==ESFBuildingType::MineralMine||Type==ESFBuildingType::EtherMine;
    TArray<FVector> Candidates;
    if(bMine)
    {
        TArray<AResourceBase*> Nodes;
        for(TActorIterator<AResourceBase> It(GetWorld());It;++It)
            if(!It->IsDepleted()&&It->GetResourceType()==(Type==ESFBuildingType::EtherMine?EResourceType::Ether:EResourceType::Mineral))Nodes.Add(*It);
        // Prefer our half of the map, but alternate base locations can put every deposit
        // across the midpoint. Keep those as a fallback rather than losing the economy.
        auto Score=[&](const AResourceBase& Node)
        {
            const double Own=FVector::DistSquared2D(Node.GetActorLocation(),S->Base1);
            return Own+(Own>FVector::DistSquared2D(Node.GetActorLocation(),S->Base0)?5.e8:0.);
        };
        Nodes.Sort([&](const AResourceBase& A,const AResourceBase& B){return Score(A)<Score(B);});
        for(auto* Node:Nodes)
        {
            FVector P=Grid->GridInfoToWorldCenter(Node->GetRegisteredResourceGridInfo());P.Z=Node->GetActorLocation().Z;
            for(const FVector2D Offset:{FVector2D(0,0),FVector2D(100,0),FVector2D(-100,0),FVector2D(0,100),FVector2D(0,-100)})
                Candidates.Add(P+FVector(Offset.X,Offset.Y,0));
            if(Candidates.Num()>=15)break;
        }
    }
    else
    {
        const float MaxRadius=FMath::Clamp(FVector::Dist2D(S->Base0,S->Base1)*.43f,1100.f,2600.f);
        for(int32 I=0;I<8;++I)
        {
            const int32 Index=PlacementCursor++;const float Angle=2*PI*(Index%16)/16.f;
            const float Radius=FMath::Min(900.f+250.f*((Index/16)%7),MaxRadius);
            Candidates.Add(S->Base1+FVector(FMath::Cos(Angle),FMath::Sin(Angle),0)*Radius);
        }
    }
    int32 Attempts=0;
    for(FVector P:Candidates)
    {
        FNavLocation Ground;
        if(!Nav->ProjectPointToNavigation(P,Ground,bMine?FVector(400,400,250):FVector(75,75,200)))continue;
        if(!bMine)
        {
            if(FVector::Dist2D(P,Ground.Location)>75)continue;
            const FIntPoint Size=USFBuildingGridSubsystem::GetRotatedGridSize(Def->GridSize,FRotator(0,90,0));
            bool bClear=true;
            for(const FVector2D Corner:{FVector2D(-1,-1),FVector2D(-1,1),FVector2D(1,-1),FVector2D(1,1)})
            {
                const FVector Edge=Ground.Location+FVector(Corner.X*Size.X*50,Corner.Y*Size.Y*50,0);FNavLocation Walkable;
                if(!Nav->ProjectPointToNavigation(Edge,Walkable,FVector(75,75,150))||FVector::Dist2D(Edge,Walkable.Location)>75){bClear=false;break;}
            }
            if(!bClear)continue;
            P=Ground.Location;
        }
        auto Reachable=[&](const FVector& Goal)
        {
            auto* Path=UNavigationSystemV1::FindPathToLocationSynchronously(GetWorld(),Commander->GetActorLocation(),Goal);
            return Path&&Path->IsValid()&&!Path->IsPartial();
        };
        bool bReachable=Reachable(Ground.Location);
        // Deposits can leave an isolated navigation tile at their center. The builder
        // works from outside the mine footprint, so also test its perimeter approaches.
        if(!bReachable&&bMine)
        {
            const float Radius=FMath::Max(Def->GridSize.X,Def->GridSize.Y)*50.f+220.f;
            for(int32 I=0;I<8&&!bReachable;++I)
            {
                const float Angle=2*PI*I/8.f;FNavLocation Edge;
                if(Nav->ProjectPointToNavigation(P+FVector(FMath::Cos(Angle),FMath::Sin(Angle),0)*Radius,Edge,FVector(150,150,300)))bReachable=Reachable(Edge.Location);
            }
        }
        if(!bReachable)
        {UE_LOG(LogTemp,Verbose,TEXT("[PVPAI] no approach %s at %s"),*Def->BuildingId.ToString(),*P.ToString());continue;}
        if(auto* Building=TryPlaceGameplayBuilding(Def->BuildingId,FTransform(FRotator(0,90,0),P)))
        {
            OrderedBuilding=Building;BuildOrderedAt=GetWorld()->GetTimeSeconds();
            UE_LOG(LogTemp,Display,TEXT("[PVPAI] build=%s location=%s mineral=%d ether=%d"),*Def->BuildingId.ToString(),*Building->GetActorLocation().ToString(),S->GetMineral(1),S->GetEther(1));
            return true;
        }
        UE_LOG(LogTemp,Verbose,TEXT("[PVPAI] placement rejected %s at %s"),*Def->BuildingId.ToString(),*P.ToString());
        if(++Attempts>=6)break;
    }
    return false;
}

void ASFPVPComputerController::Think()
{
    auto* S=GetWorld()->GetGameState<ASFPVPBattleState>();
    if(!HasAuthority()||!S||!S->bAIEnabled||!S->bBattleReady||!S->IsStagePlaying()||S->WinnerTeam!=INDEX_NONE)return;
    if(StartedAt==0){StartedAt=GetWorld()->GetTimeSeconds();UE_LOG(LogTemp,Display,TEXT("[PVPAI] started mineral=%d ether=%d"),S->GetMineral(1),S->GetEther(1));}
    if(S->Commanders.IsValidIndex(1)&&GetMyCommander()!=S->Commanders[1])RegisterCommander(S->Commanders[1]);
    TMap<ESFBuildingType,int32> Counts;bool bHouseUnderConstruction=false;
    for(TActorIterator<ASFBaseBuilding> It(GetWorld());It;++It)if(It->GetTeamIndex()==1&&!It->IsActorBeingDestroyed())
        if(auto* Def=It->GetBuildingDefinition())
        {++Counts.FindOrAdd(Def->BuildingType);bHouseUnderConstruction|=Def->BuildingType==ESFBuildingType::House&&!It->IsTierActivated();}
    bool bBuilderBusy=OrderedBuilding.IsValid()&&!OrderedBuilding->ProvidesVision();
    if(bBuilderBusy&&GetWorld()->GetTimeSeconds()-BuildOrderedAt>30)
    {ClearPendingBuildingRef();OrderedBuilding.Reset();bBuilderBusy=false;UE_LOG(LogTemp,Display,TEXT("[PVPAI] retry unreachable build"));}
    if(!bBuilderBusy)
    {
        OrderedBuilding.Reset();
        const int32 Army=S->GetCurrentPopulation(1),Capacity=S->GetMaximumPopulation(1);
        bool bOrdered=false;
        if(Counts.FindRef(ESFBuildingType::MineralMine)<1)bOrdered=TryBuild(ESFBuildingType::MineralMine);
        if(!bOrdered&&Counts.FindRef(ESFBuildingType::EtherMine)<1)bOrdered=TryBuild(ESFBuildingType::EtherMine);
        if(!bOrdered&&Counts.FindRef(ESFBuildingType::Barracks)<1)bOrdered=TryBuild(ESFBuildingType::Barracks);
        if(!bOrdered&&!bHouseUnderConstruction&&Capacity-Army<5&&Counts.FindRef(ESFBuildingType::House)<8)bOrdered=TryBuild(ESFBuildingType::House);
        if(!bOrdered&&Counts.FindRef(ESFBuildingType::RangedProduction)<1&&Counts.FindRef(ESFBuildingType::Barracks)>0)bOrdered=TryBuild(ESFBuildingType::RangedProduction);
        if(!bOrdered&&Counts.FindRef(ESFBuildingType::MineralMine)<2&&Army>=4)bOrdered=TryBuild(ESFBuildingType::MineralMine);
        if(!bOrdered&&Army>=6&&S->Nexuses.IsValidIndex(1))if(auto* N=S->Nexuses[1].Get();N&&N->IsTierActivated()&&N->HasNextUpgradeTier()&&S->CanAffordResource(1,N->GetUpgradeCost()))N->UpgradeBuilding();
    }
    ProduceArmy();CommandArmy();
}

void ASFPVPComputerController::ProduceArmy()
{
    auto* S=GetWorld()->GetGameState<ASFPVPBattleState>();
    auto* Data=GetGameInstance()->GetSubsystem<USFDataTableManager>();
    if(!Data||S->GetCurrentPopulation(1)>=24)return;
    for(TActorIterator<ASFProductionBuilding> It(GetWorld());It;++It)
    {
        if(It->GetTeamIndex()!=1||!It->IsTierActivated())continue;
        auto* Product=It->FindComponentByClass<UProductComponent>();if(!Product||!Product->GetProductLine().IsEmpty())continue;
        if(!Product->HasAssemblePoint())It->SetAssembleVectorPoint(S->Base1+(S->Base0-S->Base1).GetSafeNormal2D()*800);
        const auto Available=IProductable::Execute_GetProductableData(*It).ProductableUnitFNames;
        for(FName Row:Available)
        {
            const auto* Stats=Data->GetDataRow<FUnitStatData>(EDataTableType::Unit,Row);
            if(!Stats||!S->CanAffordPopulation(1,Stats->PopulationCost))continue;
            FResources Budget=Stats->UnitProductionCost;
            if(OrderedBuilding.IsValid()&&!OrderedBuilding->ProvidesVision())
            {const auto Reserved=OrderedBuilding->GetConstructCost();Budget.Mineral+=Reserved.Mineral;Budget.Ether+=Reserved.Ether;}
            if(!S->CanAffordResource(1,Budget))continue;
            IProductable::Execute_ProductUnits(*It,Row);
            UE_LOG(LogTemp,Display,TEXT("[PVPAI] produce=%s"),*Row.ToString());break;
        }
    }
}

void ASFPVPComputerController::CommandArmy()
{
    auto* S=GetWorld()->GetGameState<ASFPVPBattleState>();auto* Fog=USFFogOfWarSubsystem::Get(this);
    const double Now=GetWorld()->GetTimeSeconds();TArray<AUnitBase*> Army,Reinforcements;AActor* Threat=nullptr;
    for(auto It=Attackers.CreateIterator();It;++It)if(!It->IsValid()||It->Get()->GetCombatState()==EUnitCombatState::Dead)It.RemoveCurrent();
    for(TActorIterator<AUnitBase> It(GetWorld());It;++It)if(It->GetTeamIndex()==1&&It->GetCombatState()!=EUnitCombatState::Dead)
    {Army.Add(*It);if(!Attackers.Contains(*It))Reinforcements.Add(*It);}
    for(TActorIterator<AActor> It(GetWorld());It;++It)if(It->IsA<AUnitBase>()||It->IsA<ASFCommander>())
        if(const auto* Target=It->FindComponentByClass<UCombatTargetComponent>();Target&&Target->IsAlive()&&Target->GetTeamIndex()==0&&
            FVector::DistSquared2D(It->GetActorLocation(),S->Base1)<FMath::Square(2200.f)&&(!Fog||Fog->IsLocationVisible(1,It->GetActorLocation())))
            if(!Threat||FVector::DistSquared2D(It->GetActorLocation(),S->Base1)<FVector::DistSquared2D(Threat->GetActorLocation(),S->Base1))Threat=*It;
    if(!Threat&&Reinforcements.Num()>0&&((Reinforcements.Num()>=4&&Now-StartedAt>=50&&Now-LastWaveAt>=30)||Now-FMath::Max(LastWaveAt,StartedAt)>110))
    {
        for(auto* Unit:Reinforcements){Unit->ExecuteCommand(ESFUnitCommand::AttackMove,S->Base0,nullptr);Attackers.Add(Unit);}
        LastWaveAt=Now;++AttackWaves;UE_LOG(LogTemp,Display,TEXT("[PVPAI] attack_wave=%d units=%d"),AttackWaves,Reinforcements.Num());
    }
    if(Threat)for(auto* Unit:Army)
        if(Unit->GetCombatState()!=EUnitCombatState::Attacking&&Unit->GetCombatState()!=EUnitCombatState::Chasing)
            Unit->ExecuteCommand(ESFUnitCommand::AttackMove,Threat->GetActorLocation(),nullptr);
    // Defenders resume the offensive after the threat dies instead of waiting at its last position.
    if(!Threat)for(auto Unit:Attackers)if(Unit.IsValid()&&Unit->GetVelocity().SizeSquared2D()<9.f&&
        Unit->GetCombatState()!=EUnitCombatState::Attacking&&Unit->GetCombatState()!=EUnitCombatState::Chasing)
        Unit->ExecuteCommand(ESFUnitCommand::AttackMove,S->Base0,nullptr);
    auto* Commander=GetMyCommander();if(!Commander||Commander->IsAwaitingRespawn())return;
    const bool bBuilding=OrderedBuilding.IsValid()&&!OrderedBuilding->ProvidesVision();
    if(!bBuilding&&Now-LastCommanderOrder>12)
    {
        FVector Support=S->Base1+(S->Base0-S->Base1).GetSafeNormal2D()*800;
        if(Attackers.Num()>=3)for(auto Unit:Attackers)if(Unit.IsValid()){Support=Unit->GetActorLocation();break;}
        IMovable::Execute_MoveToLocation(Commander,Support);LastCommanderOrder=Now;
    }
    int32 Nearby=0;for(auto* Unit:Army)Nearby+=FVector::DistSquared2D(Unit->GetActorLocation(),Commander->GetActorLocation())<FMath::Square(750.f);
    if(Nearby>=2||Threat)
    {
        for(int32 Tier=3;Tier>=1;--Tier)if(Commander->IsSkillTierUnlocked(Tier)&&Commander->GetSkillCooldownRemaining(Tier)<=0&&
            (Tier!=3||Threat)){Commander->ActiveCommanderSkill(Tier);break;}
    }
}
