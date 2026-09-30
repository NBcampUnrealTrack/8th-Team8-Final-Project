#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "HAL/IConsoleManager.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "Engine/LevelScriptBlueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EngineUtils.h"
#include "UObject/UnrealType.h"

static FAutoConsoleCommandWithWorldAndArgs Stage04CommanderGraph(
    TEXT("SF.Stage04.CommanderGraph"), TEXT("Inspect the Stage04 legacy commander spawn; repair disables only that node."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args,UWorld* World)
    {
        if (!World || World->IsGameWorld() || World->GetOutermost()->GetName()!=TEXT("/Game/Game/Stage/PT_Stage_4")) return;
        UBlueprint* BP=World->PersistentLevel->GetLevelScriptBlueprint(true);
        if (!BP) return;
        TArray<UEdGraph*> Graphs; BP->GetAllGraphs(Graphs);
        for (UEdGraph* Graph:Graphs) for (UEdGraphNode* Node:Graph->Nodes)
        {
            if (!Node || Node->GetClass()->GetFName()!=TEXT("K2Node_SpawnActorFromClass")) continue;
            bool bLegacyBeast=false;
            for (UEdGraphPin* Pin:Node->Pins)
            {
                UE_LOG(LogTemp,Display,TEXT("S4_SPAWN_PIN %s %s value=%s object=%s links=%d"),*Node->GetName(),*Pin->PinName.ToString(),*Pin->DefaultValue,*GetPathNameSafe(Pin->DefaultObject),Pin->LinkedTo.Num());
                for (UEdGraphPin* Linked:Pin->LinkedTo) UE_LOG(LogTemp,Display,TEXT("S4_LINK %s -> %s.%s (%s)"),*Pin->PinName.ToString(),*Linked->GetOwningNode()->GetName(),*Linked->PinName.ToString(),*Linked->GetOwningNode()->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
                if (Pin->PinName==TEXT("Class") && GetPathNameSafe(Pin->DefaultObject).Contains(TEXT("BP_Beast_Commander_C"))) bLegacyBeast=true;
            }
            if (bLegacyBeast && Args.Contains(TEXT("repair")))
            {
                AActor* AuthoredCommander=nullptr;
                for (TActorIterator<AActor> It(World);It;++It)
                    if (It->GetClass()->GetName()==TEXT("BP_Ether_Commander_C")) { AuthoredCommander=*It;break; }
                UClass* LiteralClass=FindObject<UClass>(nullptr,TEXT("/Script/BlueprintGraph.K2Node_Literal"));
                UEdGraphPin* Result=Node->FindPin(TEXT("ReturnValue"));
                if (!AuthoredCommander || !LiteralClass || !Result) return;
                if (!Result->LinkedTo.IsEmpty())
                {
                    UEdGraphNode* Reference=NewObject<UEdGraphNode>(Graph,LiteralClass,NAME_None,RF_Transactional);
                    FObjectPropertyBase* Property=FindFProperty<FObjectPropertyBase>(LiteralClass,TEXT("ObjectRef"));
                    if (!Property) return;
                    Property->SetObjectPropertyValue_InContainer(Reference,AuthoredCommander);
                    Reference->CreateNewGuid();Reference->NodePosX=Node->NodePosX;Reference->NodePosY=Node->NodePosY+280;
                    Reference->AllocateDefaultPins();
                    Graph->Modify();Graph->AddNode(Reference,false,false);
                    TArray<UEdGraphPin*> Consumers=Result->LinkedTo;Result->BreakAllPinLinks();
                    for (UEdGraphPin* Consumer:Consumers) Reference->Pins[0]->MakeLinkTo(Consumer);
                }
                Node->Modify();Node->SetEnabledState(ENodeEnabledState::Disabled);BP->MarkPackageDirty();
                UE_LOG(LogTemp,Display,TEXT("S4_DISABLED_LEGACY_SPAWN %s"),*Node->GetPathName());
            }
        }
    }));

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFStage04CommanderSetupTest,"StrikeForge.Stage.Stage04.NoLegacyCommanderSpawn",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSFStage04CommanderSetupTest::RunTest(const FString& Parameters)
{
    UWorld* World=LoadObject<UWorld>(nullptr,TEXT("/Game/Game/Stage/PT_Stage_4.PT_Stage_4"));
    if (!TestNotNull(TEXT("Existing authored Stage04 map"),World)) return false;
    UBlueprint* BP=World->PersistentLevel->GetLevelScriptBlueprint(true);
    if (!BP) return true;
    TArray<UEdGraph*> Graphs;BP->GetAllGraphs(Graphs);
    for (UEdGraph* Graph:Graphs) for (UEdGraphNode* Node:Graph->Nodes)
    {
        if (!Node || Node->GetClass()->GetFName()!=TEXT("K2Node_SpawnActorFromClass")) continue;
        for (UEdGraphPin* Pin:Node->Pins)
            if (Pin->PinName==TEXT("Class") && GetPathNameSafe(Pin->DefaultObject).Contains(TEXT("BP_Beast_Commander_C")))
                TestEqual(TEXT("Origin spawn must not replace the authored Etherian commander"),Node->GetDesiredEnabledState(),ENodeEnabledState::Disabled);
    }
    return true;
}
#endif
