#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "HAL/IConsoleManager.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "Engine/LevelScriptBlueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EngineUtils.h"
#include "Components/PrimitiveComponent.h"
#include "UObject/UnrealType.h"
#include "InGame/Controller/SFInGamePlayerController.h"
#include "Units/Commander/SFCommander.h"
#include "Units/Interfaces/Movable.h"
#include "ActorComponent/Combat/CombatTargetComponent.h"

// Editor asset maintenance only: this command never starts PIE or spawns a runtime commander.
static FAutoConsoleCommandWithWorldAndArgs Stage04CommanderGraph(
    TEXT("SF.Stage04.CommanderGraph"), TEXT("Inspect level wiring, or restore the authored Stage04 blueprint commander spawn."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args,UWorld* World)
    {
        if (!World || World->IsGameWorld()) return;
        if (Args.Contains(TEXT("ground")))
        {
            for (TActorIterator<AActor> It(World);It;++It)
            {
                TInlineComponentArray<UPrimitiveComponent*> Components;It->GetComponents(Components);
                for (UPrimitiveComponent* C:Components)
                    if (C->GetCollisionEnabled()!=ECollisionEnabled::NoCollision)
                        UE_LOG(LogTemp,Display,TEXT("S4_GROUND actor=%s component=%s profile=%s landscape=%d visibility=%d"),*It->GetActorLabel(),*C->GetName(),*C->GetCollisionProfileName().ToString(),int32(C->GetCollisionResponseToChannel(ECC_GameTraceChannel1)),int32(C->GetCollisionResponseToChannel(ECC_Visibility)));
            }
            return;
        }
        UBlueprint* BP=World->PersistentLevel->GetLevelScriptBlueprint(true);
        if (Args.Contains(TEXT("ether"))) BP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/Game/Race_Etherial/Character/Unit_Commander/BP_Ether_Commander.BP_Ether_Commander"));
        if (Args.Contains(TEXT("beast"))) BP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/Game/Race_Beast/Character/Unit_Commander/BP_Beast_Commander.BP_Beast_Commander"));
        if (!BP) return;
        TArray<UEdGraph*> Graphs;BP->GetAllGraphs(Graphs);
        if (Args.Contains(TEXT("inspect")))
        {
            for (UEdGraph* Graph:Graphs) for (UEdGraphNode* Node:Graph->Nodes)
            {
                if (!Node) continue;
                UE_LOG(LogTemp,Display,TEXT("S4_GRAPH_NODE %s | %s | enabled=%d"),*Node->GetName(),*Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString(),int32(Node->GetDesiredEnabledState()));
                for (UEdGraphPin* Pin:Node->Pins)
                {
                    UE_LOG(LogTemp,Display,TEXT("S4_GRAPH_PIN %s.%s value=%s object=%s"),*Node->GetName(),*Pin->PinName.ToString(),*Pin->DefaultValue,*GetPathNameSafe(Pin->DefaultObject));
                    for (UEdGraphPin* Linked:Pin->LinkedTo) UE_LOG(LogTemp,Display,TEXT("S4_GRAPH_LINK %s.%s -> %s.%s"),*Node->GetName(),*Pin->PinName.ToString(),*Linked->GetOwningNode()->GetName(),*Linked->PinName.ToString());
                }
            }
            return;
        }
        if (!Args.Contains(TEXT("repair")) || World->GetOutermost()->GetName()!=TEXT("/Game/Game/Stage/PT_Stage_4")) return;
        UEdGraphNode* Spawn=nullptr;UEdGraphNode* Reference=nullptr;UEdGraphNode* Validity=nullptr;
        for (UEdGraph* Graph:Graphs) for (UEdGraphNode* Node:Graph->Nodes)
        {
            if (Node->GetFName()==TEXT("K2Node_SpawnActorFromClass_1")) Spawn=Node;
            if (Node->GetFName()==TEXT("K2Node_Literal_0")) Reference=Node;
            if (Node->GetFName()==TEXT("K2Node_MacroInstance_2")) Validity=Node;
        }
        AActor* Anchor=nullptr;
        for (TActorIterator<AActor> It(World);It;++It)
            if (It->ActorHasTag(TEXT("SF_Stage04_CommanderSpawn"))) { Anchor=*It;break; }
        UClass* CommanderClass=LoadClass<AActor>(nullptr,TEXT("/Game/Game/Race_Etherial/Character/Unit_Commander/BP_Ether_Commander.BP_Ether_Commander_C"));
        if (!Spawn || !Reference || !Validity || !Anchor || !CommanderClass) return;
        UEdGraphPin* Transform=Spawn->FindPin(TEXT("SpawnTransform"));
        UEdGraphPin* Input=Validity->FindPin(TEXT("InputObject"));
        FObjectPropertyBase* ObjectRef=FindFProperty<FObjectPropertyBase>(Reference->GetClass(),TEXT("ObjectRef"));
        if (!Transform || Transform->LinkedTo.Num()!=1 || !Input || !ObjectRef) return;
        UEdGraphNode* GetTransform=Transform->LinkedTo[0]->GetOwningNode();
        UEdGraphPin* TransformTarget=GetTransform->FindPin(TEXT("self"));
        UEdGraphPin* ClassPin=Spawn->FindPin(TEXT("Class"));
        if (!TransformTarget || !ClassPin) return;

        BP->Modify();Spawn->Modify();Reference->Modify();Validity->Modify();GetTransform->Modify();
        Input->BreakAllPinLinks();
        for (UEdGraphPin* Pin:Reference->Pins) Pin->BreakAllPinLinks();
        ObjectRef->SetObjectPropertyValue_InContainer(Reference,Anchor);
        Reference->ReconstructNode();
        TransformTarget->BreakAllPinLinks();
        Reference->Pins[0]->MakeLinkTo(TransformTarget);
        Reference->NodePosX=GetTransform->NodePosX-260;
        Reference->NodePosY=GetTransform->NodePosY;
        ClassPin->DefaultObject=CommanderClass;
        Spawn->PinDefaultValueChanged(ClassPin);
        Spawn->ReconstructNode();
        Spawn->SetEnabledState(ENodeEnabledState::Enabled);
        Spawn->FindPinChecked(TEXT("bMyCommander"))->DefaultValue=TEXT("True");
        Spawn->FindPinChecked(TEXT("CollisionHandlingOverride"))->DefaultValue=TEXT("AlwaysSpawn");
        Spawn->FindPinChecked(TEXT("TransformScaleMethod"))->DefaultValue=TEXT("OverrideRootScale");
        Spawn->FindPinChecked(TEXT("ReturnValue"))->MakeLinkTo(Input);
        Spawn->GetGraph()->NotifyGraphChanged();BP->MarkPackageDirty();
        UE_LOG(LogTemp,Display,TEXT("S4_BLUEPRINT_SPAWN_RESTORED class=%s anchor=%s"),*CommanderClass->GetName(),*Anchor->GetActorLabel());
    }));
#endif
