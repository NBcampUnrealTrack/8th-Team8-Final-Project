#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Game/Multiplayer/SFPVPComputerController.h"
#include "InGame/Controller/SFInGamePlayerController.h"
#include "Game/Subsystem/SFCombatSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFLocalPresentationTest,"StrikeForge.Multiplayer.LocalPresentationController",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)

bool FSFLocalPresentationTest::RunTest(const FString&)
{
    UWorld::InitializationValues Values;
    Values.AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Values);
    if(!TestNotNull(TEXT("Test world"),World))return false;
    // Exercise the order produced when a non-local controller survives travel first.
    auto* Remote=World->SpawnActor<ASFPVPComputerController>();
    auto* Local=World->SpawnActor<ASFInGamePlayerController>();
    if(Remote&&Local)
    {
        // This fixture does not begin play, so register the controllers explicitly.
        if(!World->GetFirstPlayerController()) { World->AddController(Remote);World->AddController(Local); }
        Remote->SetMyTeamIndex(1);Local->SetMyTeamIndex(0);
        auto* Combat=World->GetSubsystem<USFCombatSubsystem>();
        TestTrue(TEXT("The first controller belongs to the other participant"),World->GetFirstPlayerController()==Remote&&!Remote->IsLocalController());
        TestTrue(TEXT("Presentation finds the local controller"),Combat->GetLocalPlayerController()==Local);
        TestEqual(TEXT("Host vision/audio/palette use team 0"),Combat->GetLocalPlayerTeam(),0);
        Remote->SetMyTeamIndex(0);Local->SetMyTeamIndex(1);
        TestEqual(TEXT("The same code supports local team 1"),Combat->GetLocalPlayerTeam(),1);
    }
    else AddError(TEXT("Could not create both controllers"));
    World->DestroyWorld(false);
    return true;
}
#endif
