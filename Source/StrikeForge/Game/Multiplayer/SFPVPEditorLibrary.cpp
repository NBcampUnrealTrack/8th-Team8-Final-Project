#include "SFPVPEditorLibrary.h"
#include "SFPVPBattle.h"
#include "UI/SFTitlePresentation.h"
#if WITH_EDITOR
#include "Engine/LevelScriptBlueprint.h"
#include "Engine/Level.h"
#include "EngineUtils.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#endif
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
