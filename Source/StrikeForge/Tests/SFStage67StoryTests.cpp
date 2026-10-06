#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "InGame/HUD/Tutorial/SFStageIntroData.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFStage67StoryTest, "StrikeForge.Campaign.Stage67Stories",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFStage67StoryTest::RunTest(const FString& Parameters)
{
    const UDataTable* Table = LoadObject<UDataTable>(nullptr, TEXT("/Game/Game/IN_UI/DT/DT_SFStageIntro.DT_SFStageIntro"));
    if (!TestNotNull(TEXT("Story table"), Table)) return false;
    for (ESFRace Race : {ESFRace::Human, ESFRace::Beast})
    {
        const FString Faction = Race == ESFRace::Human ? TEXT("Human") : TEXT("Beast");
        for (bool bVictory : {false, true})
        {
            const FString Ending = bVictory ? TEXT("Clear") : TEXT("");
            const FName Expected(*(TEXT("Stage07_") + Faction + Ending));
            TestEqual(TEXT("Final story follows selected faction"), SFStageStory::ResolveRowName(Table, TEXT("Stage07"), Race, bVictory), Expected);
            TestEqual(TEXT("Stage06 uses its shared story"), SFStageStory::ResolveRowName(Table, TEXT("Stage06"), Race, bVictory), FName(*(TEXT("Stage06") + Ending)));
            TestEqual(TEXT("Stage03 keeps its original story"), SFStageStory::ResolveRowName(Table, TEXT("Stage03"), Race, bVictory), FName(*(TEXT("Stage03") + Ending)));
            for (FName Name : {Expected, FName(*(TEXT("Stage06") + Ending))})
            {
                const FSFStageIntroDataRow* Row = Table->FindRow<FSFStageIntroDataRow>(Name, TEXT("StoryTest"));
                if (!TestNotNull(TEXT("Resolved story exists"), Row)) continue;
                TestEqual(TEXT("Starts at first passage"), Row->StartNodeIndex, 0);
                TestTrue(TEXT("Narrative has no gameplay locks"), Row->LockedFeatures.IsEmpty());
                TestTrue(TEXT("Narrative ends normally"), Row->Nodes.Num() > 1 && Row->Nodes.Last().NodeType == ESFStageIntroNodeType::End);
                for (int32 i = 0; i + 1 < Row->Nodes.Num(); ++i)
                {
                    TestEqual(TEXT("Passages connect to next node"), Row->Nodes[i].NextNodeIndex, i + 1);
                    TestFalse(TEXT("No empty story passages"), Row->Nodes[i].BodyText.IsEmpty());
                }
            }
        }
    }
    TestEqual(TEXT("Unknown race does not choose another faction"), SFStageStory::ResolveRowName(Table, TEXT("Stage07"), ESFRace::None), FName(TEXT("Stage07")));
    return true;
}
#endif
