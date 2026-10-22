#include "SFPVPEditorLibrary.h"
#include "SFPVPBattle.h"
#include "UI/SFTitlePresentation.h"
#if WITH_EDITOR
#include "Engine/LevelScriptBlueprint.h"
#include "Engine/Level.h"
#include "EngineUtils.h"
#include "Engine/TargetPoint.h"
#include "Components/SceneComponent.h"
#include "Game/Spawn/SFBuildingSpawnAnchor.h"
#include "Buildings/Resource/ResourceBase.h"
#include "InGame/HUD/Portrait/SFUnitPortraitActor.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#endif
int32 USFPVPEditorLibrary::RestoreBattlefieldBases(UWorld* World,UWorld* CampaignWorld)
{
#if WITH_EDITOR
    if(!World||!CampaignWorld||!World->GetOutermost()->GetName().StartsWith(TEXT("/Game/PVP/Maps/")))return -1;
    TArray<ASFBuildingSpawnAnchor*> Anchors;
    int32 Teams[2]={0,0};
    for(AActor* Source:CampaignWorld->PersistentLevel->Actors)
        if(auto* Anchor=Cast<ASFBuildingSpawnAnchor>(Source);IsValid(Anchor)&&Anchor->GetBuildingType()==ESFBuildingType::Nexus&&Anchor->GetTeamIndex()>=0&&Anchor->GetTeamIndex()<2)
        {Anchors.Add(Anchor);++Teams[Anchor->GetTeamIndex()];}
    if(Teams[0]!=1||Teams[1]!=1)return -1;
    TArray<AActor*> Remove;
    for(TActorIterator<AActor> It(World);It;++It)
        if(It->ActorHasTag(TEXT("PVP_Base0"))||It->ActorHasTag(TEXT("PVP_Base1")))Remove.Add(*It);
    for(auto* Actor:Remove)World->EditorDestroyActor(Actor,true);
    int32 Bases=0;
    for(auto* Anchor:Anchors)
    {
        // load_asset leaves a source world's components unregistered. Refresh its cached
        // component transform before reading it, otherwise every copied marker lands at 0,0,0.
        if(auto* Root=Anchor->GetRootComponent())Root->UpdateComponentToWorld();
        FActorSpawnParameters Params;Params.OverrideLevel=World->PersistentLevel;
        const FTransform Transform=Anchor->GetActorTransform();
        auto* Marker=World->SpawnActor<ATargetPoint>(ATargetPoint::StaticClass(),Transform,Params);
        if(!Marker)continue;
        Marker->Tags={TEXT("PVP_Restored"),Anchor->GetTeamIndex()==0?FName(TEXT("PVP_Base0")):FName(TEXT("PVP_Base1"))};
        ++Bases;
    }
    World->MarkPackageDirty();return Bases;
#else
    return -1;
#endif
}
FString USFPVPEditorLibrary::DescribeLevelScript(UWorld* World)
{
    FString Text;
#if WITH_EDITOR
    if(World)for(ULevel* Level:World->GetLevels())if(auto* BP=Level->GetLevelScriptBlueprint())
    {
        TArray<UEdGraph*> Graphs;BP->GetAllGraphs(Graphs);
        for(auto* Graph:Graphs)for(UEdGraphNode* Node:Graph->Nodes)
        {
            Text+=FString::Printf(TEXT("%s | %s | %s\n"),*Node->GetName(),*Node->GetClass()->GetName(),*Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
            for(auto* Pin:Node->Pins)for(auto* Link:Pin->LinkedTo)
                Text+=FString::Printf(TEXT("  %s -> %s.%s\n"),*Pin->PinName.ToString(),*Link->GetOwningNode()->GetName(),*Link->PinName.ToString());
        }
    }
#endif
    return Text;
}
bool USFPVPEditorLibrary::MigrateTitle(UWorld* World)
{
#if WITH_EDITOR
    if(!World||World->GetOutermost()->GetName()!=TEXT("/Game/Core/Levels/Start"))return false;
    // The audited Start graph contains only widget/cursor/input setup and an empty Tick event.
    // ASFTitleController now performs that setup for each local player with validity checks.
    for(ULevel* Level:World->GetLevels())if(auto* BP=Level->GetLevelScriptBlueprint())
    {
        TArray<UEdGraph*> Graphs;BP->GetAllGraphs(Graphs);
        for(auto* Graph:Graphs){const auto Nodes=Graph->Nodes;for(UEdGraphNode* Node:Nodes)Graph->RemoveNode(Node);}
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);
    }
    World->GetWorldSettings()->DefaultGameMode=ASFTitleMode::StaticClass();World->MarkPackageDirty();return true;
#else
    return false;
#endif
}
FString USFPVPEditorLibrary::RestoreBattlefieldResources(UWorld* World,UWorld* CampaignWorld)
{
#if WITH_EDITOR
    if(!World||!CampaignWorld||!World->GetOutermost()->GetName().StartsWith(TEXT("/Game/PVP/Maps/")))return TEXT("Invalid world");
    TArray<AActor*> Remove;
    for(TActorIterator<AActor> It(World);It;++It)if(It->ActorHasTag(TEXT("PVP_Restored")))Remove.Add(*It);
    for(auto* Actor:Remove)World->EditorDestroyActor(Actor,true);
    int32 Resources=0;
    for(AActor* Source:CampaignWorld->PersistentLevel->Actors)
    {
        if(!IsValid(Source))continue;
        if(Source->IsA<AResourceBase>()||Source->IsA<ASFUnitPortraitActor>())
        {
            if(auto* Root=Source->GetRootComponent())Root->UpdateComponentToWorld();
            FActorSpawnParameters Params;Params.OverrideLevel=World->PersistentLevel;Params.Template=Source;
            Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            if(auto* Copy=World->SpawnActor<AActor>(Source->GetClass(),Source->GetActorTransform(),Params))
            {Copy->Tags.AddUnique(TEXT("PVP_Restored"));Copy->SetReplicates(Source->IsA<AResourceBase>());if(Source->IsA<AResourceBase>())++Resources;}
        }
    }
    const int32 Bases=RestoreBattlefieldBases(World,CampaignWorld);
    World->MarkPackageDirty();return FString::Printf(TEXT("resources=%d bases=%d"),Resources,Bases);
#else
    return TEXT("Editor only");
#endif
}

int32 USFPVPEditorLibrary::PrepareBattlefield(UWorld* World)
{
#if WITH_EDITOR
    // Only touch dedicated PVP copies, never the campaign source levels.
    if(!World||!World->GetOutermost()->GetName().StartsWith(TEXT("/Game/PVP/Maps/")))return -1;
    for(ULevel* Level:World->GetLevels())
    {
        if(auto* BP=Level->GetLevelScriptBlueprint(true))
        {
            TArray<UEdGraph*> Graphs;BP->GetAllGraphs(Graphs);
            for(auto* Graph:Graphs){const auto Nodes=Graph->Nodes;for(UEdGraphNode* Node:Nodes)Graph->RemoveNode(Node);}
            FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);
        }
    }
    TArray<AActor*> Remove;
    for(TActorIterator<AActor> It(World);It;++It)
    {
        bool bGameplay=false;
        for(UClass* C=It->GetClass();C;C=C->GetSuperClass())if(C->GetOutermost()->GetName()==TEXT("/Script/StrikeForge"))bGameplay=true;
        const FString Path=It->GetClass()->GetPathName();
        if(Path.StartsWith(TEXT("/Game/Core/"))||Path.StartsWith(TEXT("/Game/Game/Stage/")))bGameplay=true;
        if(bGameplay)Remove.Add(*It);
    }
    for(auto* Actor:Remove)World->EditorDestroyActor(Actor,true);
    World->GetWorldSettings()->DefaultGameMode=ASFPVPBattleMode::StaticClass();
    World->MarkPackageDirty();return Remove.Num();
#else
    return -1;
#endif
}
