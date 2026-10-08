#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Units/Unit/SFNeutralSlime.h"
#include "ActorComponent/Combat/CombatTargetComponent.h"
#include "ActorComponent/Stat/HealthComponent.h"
#include "ActorComponent/Buff/SFBuffComponent.h"
#include "Game/Subsystem/SFStageBuffSubsystem.h"
#include "Game/Subsystem/SFCombatSubsystem.h"
#include "Game/GameState/SFGameState.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

namespace SFSlimeTests
{
    struct FWorld
    {
        UWorld* World;
        ASFGameState* GS;
        FWorld()
        {
            UWorld::InitializationValues Values;
            Values.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
            World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
            GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
            World->InitializeActorsForPlay(FURL());
            GS = World->SpawnActor<ASFGameState>();
            World->SetGameState(GS);
            GS->FindOrAddTeam(0); GS->FindOrAddTeam(1);
        }
        ~FWorld() { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); }
        AUnitBase* Unit(int32 Team, FVector Position)
        {
            auto* Unit = World->SpawnActor<AUnitBase>(Position, FRotator::ZeroRotator);
            Unit->FindComponentByClass<UCombatTargetComponent>()->SetTeamIndex(Team);
            FUnitStatData Row; Row.Attack = 100; Row.MaxHealth = 1000; Row.PopulationCost = 0;
            Unit->InitStat(Row);
            Unit->DispatchBeginPlay();
            return Unit;
        }
        ASFNeutralSlime* Slime(ASFNeutralSlimeCamp* Camp, bool Large, FVector Position)
        {
            auto* Unit = World->SpawnActor<ASFNeutralSlime>(Position, FRotator::ZeroRotator);
            Unit->bLarge = Large; Unit->Camp = Camp; Camp->Members.Add(Unit);
            Unit->DispatchBeginPlay();
            return Unit;
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFNeutralRewardTest, "StrikeForge.NeutralSlime.RewardsAndStageScope",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFNeutralRewardTest::RunTest(const FString& Parameters)
{
    using namespace SFSlimeTests;
    {
        FWorld F;
        auto* Buffs = F.World->GetSubsystem<USFStageBuffSubsystem>();
        auto* Player = F.Unit(0, FVector(0,0,100));
        auto* Enemy = F.Unit(1, FVector(5000,0,100));
        Player->GetHealthComponent()->ApplyDamage(500);
        Buffs->GrantReward(0, ESFStageReward::Attack);
        TestEqual(TEXT("Existing attack +10%"), Player->GetCurrentAttackPower(), 110.f);
        Buffs->GrantReward(0, ESFStageReward::Health);
        TestEqual(TEXT("Existing max health +10%"), Player->GetHealthComponent()->GetMaxHealth(), 1100.f);
        TestEqual(TEXT("Health ratio preserved"), Player->GetHealthComponent()->GetCurrentHealth(), 550.f);
        TestEqual(TEXT("Enemy unchanged"), Enemy->GetCurrentAttackPower(), 100.f);
        auto* NewUnit = F.Unit(0, FVector(500,0,100));
        TestEqual(TEXT("Future unit attack inherits"), NewUnit->GetCurrentAttackPower(), 110.f);
        TestEqual(TEXT("Future unit health inherits"), NewUnit->GetHealthComponent()->GetMaxHealth(), 1100.f);
        Buffs->ApplyToUnit(NewUnit);
        TestEqual(TEXT("Applying twice does not duplicate"), NewUnit->GetHealthComponent()->GetMaxHealth(), 1100.f);
        Buffs->GrantReward(0, ESFStageReward::Resources);
        TestEqual(TEXT("Resource mineral"), F.GS->GetMineral(0), 1000);
        TestEqual(TEXT("Resource ether"), F.GS->GetEther(0), 1000);
        TestFalse(TEXT("Persistent HUD summary exists"), Buffs->GetSummary(0).IsEmpty());
        auto* Camp = F.World->SpawnActor<ASFNeutralSlimeCamp>();
        auto* SmallA = F.Slime(Camp, false, FVector(0,300,100));
        auto* SmallB = F.Slime(Camp, false, FVector(300,300,100));
        auto* Large = F.Slime(Camp, true, FVector(-300,300,100));
        TestEqual(TEXT("Team row 3"), SmallA->GetTeamIndex(), 3);
        TestFalse(TEXT("Initially passive"), SmallA->IsProvokedByTeam(0));
        TestNull(TEXT("Passing player does not auto attack neutral"), Player->FindBestTarget());
        TestTrue(TEXT("Explicit attack permitted"), Player->CanForceAttackTarget(SmallA));
        SmallA->FindComponentByClass<UCombatTargetComponent>()->ApplyCombatDamage(1, Player);
        TestTrue(TEXT("Hit provokes entire camp"), SmallB->IsProvokedByTeam(0));
        TestFalse(TEXT("Other team not provoked"), SmallB->IsProvokedByTeam(1));
        TestEqual(TEXT("Retaliation targets aggressor"), SmallB->CurrentTarget.Get(), static_cast<AActor*>(Player));
        for (auto* Small : {SmallA, SmallB}) Small->FindComponentByClass<UCombatTargetComponent>()->ApplyCombatDamage(10000, Player);
        TestEqual(TEXT("Two small kills pay 200"), F.GS->GetMineral(0), 1200);
        TestFalse(TEXT("Partial camp grants no reward"), Camp->IsCleared());
        Large->FindComponentByClass<UCombatTargetComponent>()->ApplyCombatDamage(10000, Player);
        TestTrue(TEXT("Whole camp cleared"), Camp->IsCleared());
        const auto B = Buffs->GetBuffs(0);
        TestEqual(TEXT("Exactly one random reward"), B.ResourceRewards+B.AttackStacks+B.HealthStacks, 4);
        TestEqual(TEXT("Large kill pays 200 plus any resource roll"), F.GS->GetMineral(0), 1400 + (B.ResourceRewards-1)*1000);
        const int32 Before = F.GS->GetMineral(0);
        Large->FindComponentByClass<UCombatTargetComponent>()->ApplyCombatDamage(10000, Player);
        Camp->MemberKilled(Large,0);
        TestEqual(TEXT("Repeated death cannot pay twice"), F.GS->GetMineral(0), Before);
        auto* SmallCamp = F.World->SpawnActor<ASFNeutralSlimeCamp>();
        SmallCamp->bGrantClearReward = false;
        const int32 BeforePairKills = F.GS->GetMineral(0);
        auto* PairA = F.Slime(SmallCamp, false, FVector(600,300,100));
        auto* PairB = F.Slime(SmallCamp, false, FVector(900,300,100));
        PairA->FindComponentByClass<UCombatTargetComponent>()->ApplyCombatDamage(10000, Player);
        TestFalse(TEXT("Small-only camp waits for both kills"), SmallCamp->IsCleared());
        PairB->FindComponentByClass<UCombatTargetComponent>()->ApplyCombatDamage(10000, Player);
        TestTrue(TEXT("No-reward camp is still marked cleared"), SmallCamp->IsCleared());
        const auto PairBuffs = Buffs->GetBuffs(0);
        TestEqual(TEXT("No-reward camp does not roll a buff or resource reward"), PairBuffs.ResourceRewards+PairBuffs.AttackStacks+PairBuffs.HealthStacks, 4);
        TestEqual(TEXT("Small individual kill payouts remain 100 each"), F.GS->GetMineral(0), BeforePairKills + 200);
        TestFalse(TEXT("No-reward description does not promise a camp reward"), PairA->GetSelectableInfo_Implementation().Description.ToString().Contains(TEXT("무작위 보상")));
    }
    FWorld Next;
    const auto B = Next.World->GetSubsystem<USFStageBuffSubsystem>()->GetBuffs(0);
    TestEqual(TEXT("New stage has no buffs"), B.ResourceRewards+B.AttackStacks+B.HealthStacks, 0);
    return true;
}
#endif
