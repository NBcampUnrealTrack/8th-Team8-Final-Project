#include "SFInGamePlayerController.h"
#include "InGame/SFInGameHUDWidget.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "ActorComponent/Controller/BuildingPlacement.h"
#include "ActorComponent/Combat/CombatTargetComponent.h"
#include "ActorComponent/Stat/HealthComponent.h"
#include "ActorComponent/Product/ProductComponent.h"
#include "Buildings/Building/SFBaseBuilding.h"
#include "Buildings/Building/ProductBuilding/SFProductionBuilding.h"
#include "Datas/Building/SFBuildingCatalog.h"
#include "Datas/Building/SFBuildingDefinition.h"
#include "Game/Spawn/SFBuildingSpawnLibrary.h"
#include "Game/Subsystem/SFFogOfWarSubsystem.h"
#include "Interfaces/Building/Productable.h"
#include "Units/Commander/SFCommander.h"
#include "Units/Unit/UnitBase.h"
#include "NavigationSystem.h"
#include "UObject/UnrealType.h"
#include "Game/Audio/SFUnitVoiceSubsystem.h"
#include "Sound/SoundBase.h"

bool ASFInGamePlayerController::IsPVPGameplay() const
{
    const auto* State=GetWorld()?GetWorld()->GetGameState<ASFGameState>():nullptr;
    return State&&State->IsPVPMatch();
}

void ASFInGamePlayerController::ConfigurePVEGameplayProfile()
{
    // Read the actual campaign profile, so key bindings/HUD changes have one source.
    const UClass* Class=LoadClass<ASFInGamePlayerController>(nullptr,
        TEXT("/Game/Game/IN_UI/JSYBluePrints/BP_SFInGamePlayerController.BP_SFInGamePlayerController_C"));
    const auto* Profile=Class?Class->GetDefaultObject<ASFInGamePlayerController>():nullptr;
    if(!Profile)return;
    BaseMappingPriority=Profile->BaseMappingPriority;
    GameplayMappingPriority=Profile->GameplayMappingPriority;
    BuildPlacementMappingPriority=Profile->BuildPlacementMappingPriority;
    BuildModeMappingPriority=Profile->BuildModeMappingPriority;
    bLockMouseToViewport=Profile->bLockMouseToViewport;
    SelectionDragThreshold=Profile->SelectionDragThreshold;
    FormationSpacing=Profile->FormationSpacing;
    for(TFieldIterator<FObjectPropertyBase> It(StaticClass());It;++It)
    {
        if(It->PropertyClass->IsChildOf(UInputAction::StaticClass()) ||
            It->PropertyClass->IsChildOf(UInputMappingContext::StaticClass()) ||
            It->GetFName()==TEXT("InGameHUDWidgetClass"))
            It->CopyCompleteValue_InContainer(this,Profile);
    }
    if(!InGameHUDWidgetClass)InGameHUDWidgetClass=LoadClass<USFInGameHUDWidget>(nullptr,TEXT("/Game/Game/IN_UI/HUD/WBP_SFInGameHUD.WBP_SFInGameHUD_C"));
    if(const FProperty* Material=FindFProperty<FProperty>(UBuildingPlacement::StaticClass(),TEXT("GridCellDecalMaterial")))
        Material->CopyCompleteValue_InContainer(PlacementComponent,Profile->PlacementComponent);
}

void ASFInGamePlayerController::ServerGameplayAction_Implementation(ESFGameplayAction Action,const TArray<AActor*>& Actors,
    FVector_NetQuantize Location,AActor* Target,FName Row,int32 Index,bool bAppend)
{
    auto* State=GetWorld()->GetGameState<ASFGameState>();
    if(!State||!State->IsPVPMatch()||!State->IsStagePlaying()||Actors.Num()>200||Location.ContainsNaN()||Location.SizeSquared()>1.e12)return;
    if(Target)
    {
        const auto* Combat=IsValid(Target)?Target->FindComponentByClass<UCombatTargetComponent>():nullptr;
        if(!Combat||!Combat->IsAlive()||Combat->GetTeamIndex()==MyTeamIndex)return;
        if(const auto* Fog=USFFogOfWarSubsystem::Get(this);Fog&&!Fog->IsLocationVisible(MyTeamIndex,Target->GetActorLocation()))return;
    }
    auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    FNavLocation Projected;
    if((Action==ESFGameplayAction::Move||Action==ESFGameplayAction::Attack||Action==ESFGameplayAction::Rally)&&!Target)
    {
        if(!Nav||!Nav->ProjectPointToNavigation(Location,Projected,FVector(250,250,500)))return;
        Location=Projected.Location;
    }
    TArray<AActor*> Owned;
    for(AActor* Actor:Actors)
    {
        const auto* Combat=IsValid(Actor)?Actor->FindComponentByClass<UCombatTargetComponent>():nullptr;
        if(Combat&&Combat->IsAlive()&&Combat->GetTeamIndex()==MyTeamIndex)Owned.AddUnique(Actor);
    }
    bool bForceMove=false;
    if(Action==ESFGameplayAction::Move)for(auto* Actor:Owned)if(const auto* Unit=Cast<AUnitBase>(Actor))
        bForceMove|=Unit->GetCombatState()==EUnitCombatState::Chasing||Unit->GetCombatState()==EUnitCombatState::Attacking||Unit->IsWaitingForGroupMove();
    FSFForcedMoveGroup Group;Group.GroupId=NextForcedMoveGroupId++;
    for(int32 I=0;I<Owned.Num();++I)
    {
        AActor* Actor=Owned[I];auto* Unit=Cast<AUnitBase>(Actor);auto* Building=Cast<ASFBaseBuilding>(Actor);
        FVector Goal=Target?Target->GetActorLocation():FVector(Location);
        if(Action==ESFGameplayAction::Move&&Owned.Num()>1&&!bAppend)
        {
            const float Angle=2*PI*I/Owned.Num();
            Goal+=FVector(FMath::Cos(Angle),FMath::Sin(Angle),0)*FormationSpacing*FMath::Sqrt(float(Owned.Num()));
            FNavLocation Slot;if(Nav->ProjectPointToNavigation(Goal,Slot,FVector(150,150,300)))Goal=Slot.Location;
        }
        switch(Action)
        {
        case ESFGameplayAction::Move:
            if(Unit)
            {
                RemoveUnitFromForcedMoveGroups(Unit,false);Unit->SetWaitingForGroupMove(bForceMove);
                if(bForceMove)Group.Units.Add(Unit);
                Unit->ExecuteCommandWithQueue(bForceMove?ESFUnitCommand::Move:ESFUnitCommand::AttackMove,Goal,nullptr,bForceMove?false:bAppend);
            }
            else if(Actor->Implements<UMovable>())IMovable::Execute_MoveToLocation(Actor,Goal);
            break;
        case ESFGameplayAction::Attack:
            if(Unit){RemoveUnitFromForcedMoveGroups(Unit,true);Unit->ExecuteCommandWithQueue(ESFUnitCommand::Attack,Goal,Target,bAppend);}
            else if(Actor->Implements<UMovable>())IMovable::Execute_MoveToLocation(Actor,Goal);
            break;
        case ESFGameplayAction::Stop:
            if(Unit){RemoveUnitFromForcedMoveGroups(Unit,true);Unit->ExecuteCommand(ESFUnitCommand::Stop,Actor->GetActorLocation(),nullptr);}
            else if(Actor->Implements<UMovable>())IMovable::Execute_StopMove(Actor);
            break;
        case ESFGameplayAction::Skill:
            if(auto* Commander=Cast<ASFCommander>(Actor);Commander&&Index>=1&&Index<=3)Commander->ActiveCommanderSkill(Index);
            break;
        case ESFGameplayAction::Produce:
            if(auto* Production=Cast<ASFProductionBuilding>(Building))
                if(IProductable::Execute_GetProductableData(Production).ProductableUnitFNames.Contains(Row))IProductable::Execute_ProductUnits(Production,Row);
            break;
        case ESFGameplayAction::CancelProduct:
            if(Building)if(auto* Product=Building->FindComponentByClass<UProductComponent>())Product->RemoveProductInLine(Index);
            break;
        case ESFGameplayAction::Upgrade: if(Building)Building->UpgradeBuilding(1);break;
        case ESFGameplayAction::CancelConstruction: if(Building)Building->CancelConstruction();break;
        case ESFGameplayAction::Rally:
            if(auto* Production=Cast<ASFProductionBuilding>(Building))Production->SetAssembleVectorPoint(Location);
            break;
        }
    }
    if(Group.Units.Num())
    {
        ActiveForcedMoveGroups.Add(MoveTemp(Group));
        if(!GetWorldTimerManager().IsTimerActive(ForcedMoveGroupCheckTimer))
            GetWorldTimerManager().SetTimer(ForcedMoveGroupCheckTimer,this,&ThisClass::CheckForcedMoveGroups,.1f,true);
    }
    if(Action==ESFGameplayAction::Move&&!Owned.IsEmpty())ClientGameplayMoveVoice(Owned);
    UE_LOG(LogTemp,Display,TEXT("[PVPGameplay] team=%d action=%d accepted=%d"),MyTeamIndex,int32(Action),Owned.Num());
}

void ASFInGamePlayerController::ClientGameplayMoveVoice_Implementation(const TArray<AActor*>& Speakers)
{
    auto* Voice=GetWorld()->GetSubsystem<USFUnitVoiceSubsystem>();
    for(AActor* Speaker:Speakers)if(IsValid(Speaker))
    {
        USoundBase* Sound=nullptr;
        if(const auto* Unit=Cast<AUnitBase>(Speaker))Sound=Unit->GetMoveCommandSound();
        else if(const auto* Commander=Cast<ASFCommander>(Speaker))Sound=Commander->MoveCommandSound;
        if(Sound)Voice->RequestVoice(Speaker,Sound,true,true);
    }
    UE_LOG(LogTemp,Display,TEXT("[PVPPresentation] move response team=%d speakers=%d voice=%d"),MyTeamIndex,Speakers.Num(),Voice->GetActiveVoice()!=nullptr);
}

void ASFInGamePlayerController::ServerPlaceGameplayBuilding_Implementation(FName BuildingId,FTransform Transform)
{
    TryPlaceGameplayBuilding(BuildingId,Transform);
}
ASFBaseBuilding* ASFInGamePlayerController::TryPlaceGameplayBuilding(FName BuildingId,FTransform Transform)
{
    if(!HasAuthority())return nullptr;
    auto* State=GetWorld()->GetGameState<ASFGameState>();
    auto* Catalog=State?State->GetTeamBuildingCatalog(MyTeamIndex):nullptr;
    auto* Definition=Catalog?Catalog->FindLoadedDefinition(BuildingId):nullptr;
    const auto* FirstTier=Definition?Definition->FindTier(0):nullptr;
    if(!State||!State->IsPVPMatch()||!State->IsStagePlaying()||!Definition||!FirstTier||!MyCommander.IsValid()||MyCommander->IsAwaitingRespawn()||
        Transform.ContainsNaN()||Transform.GetLocation().SizeSquared()>1.e12||Definition->BuildingType==ESFBuildingType::Nexus||
        !State->IsBuildingAllowedInCurrentStage(BuildingId)||!State->AreBuildingRequirementsMet(MyTeamIndex,BuildingId)||
        State->GetNexusTier(MyTeamIndex)+1<Definition->RequiredNexusTier||!State->CanAffordResource(MyTeamIndex,FirstTier->Construction.ConstructCost))return nullptr;
    // Ignore scale and pitch/roll supplied by the client.
    Transform.SetScale3D(FVector::OneVector);Transform.SetRotation(FRotator(0,Transform.Rotator().Yaw,0).Quaternion());
    auto* Building=USFBuildingSpawnLibrary::SpawnBuildingFromDefinition(this,Definition,MyTeamIndex,Transform,ESFBuildingSpawnMode::PlayerPlacedMode,0);
    if(!Building)return nullptr;
    if(!Building->ReserveNetworkPlacement()){Building->Destroy();return nullptr;}
    ClearPendingBuildingRef();PendingSpawnedBuildingRef=Building;
    Building->SetOwner(this);Building->SetReplicates(true);Building->bAlwaysRelevant=true;
    MyCommander->MoveToBuild(Building->GetActorLocation(),Building->GetBuildBounds(),Building->GetActorRotation().Yaw);
    return Building;
}
