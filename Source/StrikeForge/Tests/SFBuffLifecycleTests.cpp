#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "ActorComponent/Buff/SFBuffComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

struct FSFBuffLifecycleTestAccess
{
    static void Tick(USFBuffComponent* Buff,float Delta){Buff->TickComponent(Delta,LEVELTICK_All,nullptr);}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFBuffLifecycleTest,"StrikeForge.Buff.LifecycleWithoutNiagara",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSFBuffLifecycleTest::RunTest(const FString&)
{
    UWorld::InitializationValues Values;
    Values.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
    UWorld* W=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Values);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
    W->InitializeActorsForPlay(FURL());
    auto* Actor=W->SpawnActor<ACharacter>();
    auto* Buff=NewObject<USFBuffComponent>(Actor);Buff->RegisterComponent();
    Actor->DispatchBeginPlay();
    const float Base=Actor->GetCharacterMovement()->MaxWalkSpeed;int32 Ends=0;
    FInputBuffData Data;Data.KeyName=TEXT("Lifecycle");Data.bIsBuff=true;Data.InitialTime=.2f;
    Data.TickEvent=[Buff](){Buff->Current_Status.Speed*=1.5f;Buff->ApplyBuffFlag(EBuffApply::Speed);};
    Data.EndEvent=[Buff,&Ends](){++Ends;Buff->ApplyBuffFlag(EBuffApply::Speed);};
    Buff->AddBuff(Data);
    TestEqual(TEXT("Effective speed changes immediately"),Actor->GetCharacterMovement()->MaxWalkSpeed,Base*1.5f);
    TestTrue(TEXT("Timer is available without a particle"),Buff->GetBuffList()[0]->GetTimeRate()>.99f);
    for(int32 I=0;I<6;++I)FSFBuffLifecycleTestAccess::Tick(Buff,.05f);
    TestEqual(TEXT("No-particle buff expires and leaves the HUD list"),Buff->GetBuffList().Num(),0);
    TestEqual(TEXT("End callback runs once"),Ends,1);
    TestEqual(TEXT("Speed restores after expiration"),Actor->GetCharacterMovement()->MaxWalkSpeed,Base);
    Buff->AddBuff(Data);Buff->AddBuff(Data);
    TestEqual(TEXT("Reapplication keeps one entry"),Buff->GetBuffList().Num(),1);
    TestEqual(TEXT("Reapplication does not end the replacement"),Ends,1);
    TestEqual(TEXT("Reapplication does not compound speed"),Actor->GetCharacterMovement()->MaxWalkSpeed,Base*1.5f);
    Buff->ClearBuff();Buff->ClearBuff();
    TestEqual(TEXT("Clear ends the replacement once"),Ends,2);
    TestEqual(TEXT("Clear restores speed"),Actor->GetCharacterMovement()->MaxWalkSpeed,Base);
    W->DestroyWorld(false);GEngine->DestroyWorldContext(W);
    return true;
}
#endif
