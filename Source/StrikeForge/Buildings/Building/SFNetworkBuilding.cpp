#include "SFBaseBuilding.h"
#include "Net/UnrealNetwork.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "ActorComponent/Progress/ProgressComponent.h"
#include "ActorComponent/UI/SFOverheadStatusComponent.h"
#include "Datas/Building/SFBuildingDefinition.h"
#include "Game/Subsystem/SFBuildingGridSubsystem.h"
#include "Game/GameState/SFGameState.h"
#include "NavigationSystem.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "Game/Subsystem/SFBuildingSynergySubsystem.h"

void ASFBaseBuilding::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ASFBaseBuilding,NetworkView);
}

void ASFBaseBuilding::PreReplication(IRepChangedPropertyTracker& ChangedPropertyTracker)
{
    Super::PreReplication(ChangedPropertyTracker);
    NetworkView.Definition=BuildingDefinition;NetworkView.Team=GetTeamIndex();NetworkView.Tier=CurrentTier;
    NetworkView.State=CurrentBuildingState;NetworkView.SelectionRow=RowName;
    NetworkView.Mesh=StaticMeshComponent->GetStaticMesh();NetworkView.CompletedMesh=ActiveTierAssets.CompletedMesh;
    NetworkView.Progress=ProgressComponent->GetCurrentProgressRatio();
}

void ASFBaseBuilding::OnRep_NetworkView()
{
    if(!HasActorBegunPlay()||!NetworkView.Definition)return;
    const bool bSynergyChanged=CurrentBuildingState!=NetworkView.State||CurrentTier!=NetworkView.Tier||GetTeamIndex()!=NetworkView.Team;
    const bool bActivate=NetworkView.State==EBuildingState::Completed&&(!bIsTierActivated||CurrentTier!=NetworkView.Tier);
    if(bIsTierActivated&&(NetworkView.State!=EBuildingState::Completed||CurrentTier!=NetworkView.Tier))DeactivateActiveTier();
    BuildingDefinition=NetworkView.Definition;RowName=NetworkView.SelectionRow;CurrentTier=NetworkView.Tier;
    SetMyTeamIndex(NetworkView.Team);CurrentBuildingState=NetworkView.State;
    if(!CacheLoadedDefinitionData())return;
    OnBuildingDefinitionLoaded();
    ActiveTierAssets.TierIndex=CurrentTier;ActiveTierAssets.CompletedMesh=NetworkView.CompletedMesh;
    if(const auto* Tier=GetCurrentTierInfo())ActiveTierAssets.DestructionCollection=Tier->Visual.DestructionCollection.Get();
    StaticMeshComponent->SetStaticMesh(NetworkView.Mesh);StaticMeshComponent->SetMaterial(0,nullptr);
    BoxComponent->SetCollisionProfileName(TEXT("SpawnedBuilding"));SetBoxExtent();
    if(!bGridRegistered)TryRegisterInitialGrid();
    auto* Synergy=USFBuildingSynergySubsystem::Get(this);
    if(Synergy)
    {
        Synergy->OnSynergyStateChanged.AddUniqueDynamic(this,&ThisClass::HandleBuildingSynergyStateChanged);
        Synergy->RegisterBuilding(this);
    }
    const float PreviousRatio=ProgressComponent->CurrentRatio;
    ProgressComponent->CurrentRatio=NetworkView.Progress;
    if(!FMath::IsNearlyEqual(PreviousRatio,NetworkView.Progress))ProgressComponent->OnChangedProgressRatio.Broadcast(PreviousRatio,NetworkView.Progress);
    StatusComponent->HandleProgressRatio(NetworkView.Progress);
    if(bActivate)
    {
        bIsTierActivated=true;Tags.AddUnique(TEXT("MinimapBuilding"));OnTierActivated();
        if(auto* GS=GetWorld()->GetGameState<ASFGameState>())GS->RecordCompletedBuilding(GetTeamIndex(),BuildingDefinition->BuildingId,BuildingDefinition->SelectionUIRowName);
        OnBuildingTierActivated.Broadcast(CurrentTier);
    }
    if(Synergy&&(bActivate||bSynergyChanged))Synergy->NotifyBuildingChanged(this);
}

bool ASFBaseBuilding::ReserveNetworkPlacement()
{
    auto* Grid=GetWorld()->GetSubsystem<USFBuildingGridSubsystem>();
    auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    if(!Grid||!Nav||HasLivingUnitInFootprint())return false;
    FBuildingGridInfo Info;FVector Center;
    if(!Grid->TrySnapWorldCenterToGrid(GetActorLocation(),USFBuildingGridSubsystem::GetRotatedGridSize(ConfiguredGridSize,GetActorRotation()),Info,Center))return false;
    if(!Grid->EvaluatePlayerBuildingGrid(this,Info).bCanPlaceOnGrid)return false;
    FNavLocation Ground;
    if(!Nav->ProjectPointToNavigation(Center,Ground,FVector(400,400,200))||FMath::Abs(Center.Z-Ground.Location.Z)>180)return false;
    SetActorLocation(Center);
    return TryRegisterGrid(Info);
}
