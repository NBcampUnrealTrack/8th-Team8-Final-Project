#include "SFPVPEditorLibrary.h"
#include "SFPVPBattle.h"
#if WITH_EDITOR
#include "Engine/LevelScriptBlueprint.h"
#include "Engine/Level.h"
#include "EngineUtils.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "EdGraph/EdGraph.h"
#endif
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
