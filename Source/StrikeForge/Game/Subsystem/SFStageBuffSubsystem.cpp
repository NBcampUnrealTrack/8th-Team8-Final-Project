#include "SFStageBuffSubsystem.h"
#include "Units/Unit/UnitBase.h"
#include "ActorComponent/Buff/SFBuffComponent.h"
#include "ActorComponent/Stat/HealthComponent.h"
#include "Game/GameState/SFGameState.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"

USFStageBuffSubsystem* USFStageBuffSubsystem::GetStageBuffs(const UObject* WorldContextObject)
{
    UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
    return World ? World->GetSubsystem<USFStageBuffSubsystem>() : nullptr;
}

FSFStageBuffs USFStageBuffSubsystem::GetBuffs(int32 Team) const
{
    const auto* Found = TeamBuffs.Find(Team);
    return Found ? *Found : FSFStageBuffs();
}

FText USFStageBuffSubsystem::GetSummary(int32 Team) const
{
    const auto B = GetBuffs(Team);
    if (B.ResourceRewards + B.AttackStacks + B.HealthStacks == 0) return FText::GetEmpty();
    FString Text = TEXT("슬라임 보상 · 이번 스테이지 한정");
    if (B.ResourceRewards) Text += FString::Printf(TEXT("\n광물 +%d / 에테르 +%d (지급 완료)"), 1000 * B.ResourceRewards, 1000 * B.ResourceRewards);
    if (B.AttackStacks) Text += FString::Printf(TEXT("\n모든 아군 유닛 공격력 +%d%%"), 10 * B.AttackStacks);
    if (B.HealthStacks) Text += FString::Printf(TEXT("\n모든 아군 유닛 체력 +%d%%"), 10 * B.HealthStacks);
    return FText::FromString(Text);
}

void USFStageBuffSubsystem::GrantRandomReward(int32 Team)
{
    GrantReward(Team, static_cast<ESFStageReward>(FMath::RandRange(0, 2)));
}

void USFStageBuffSubsystem::GrantReward(int32 Team, ESFStageReward Reward)
{
    ASFGameState* GS = GetWorld()->GetGameState<ASFGameState>();
    if (!GS || Team < 0 || Team == 3 || !GS->GetTeamState(Team)) return;
    auto& B = TeamBuffs.FindOrAdd(Team);
    switch (Reward)
    {
    case ESFStageReward::Resources:
        ++B.ResourceRewards;
        GS->AddResource(Team, EResourceType::Mineral, 1000);
        GS->AddResource(Team, EResourceType::Ether, 1000);
        break;
    case ESFStageReward::Attack: ++B.AttackStacks; break;
    case ESFStageReward::Health: ++B.HealthStacks; break;
    }
    for (TActorIterator<AUnitBase> It(GetWorld()); It; ++It)
        if (It->GetTeamIndex() == Team) ApplyToUnit(*It);
}

void USFStageBuffSubsystem::ApplyToUnit(AUnitBase* Unit)
{
    if (!IsValid(Unit) || !Unit->GetHealthComponent() || Unit->GetHealthComponent()->GetIsDead()) return;
    const auto B = GetBuffs(Unit->GetTeamIndex());
    if (B.AttackStacks == 0 && B.HealthStacks == 0) return;
    USFBuffComponent* Buff = Unit->FindComponentByClass<USFBuffComponent>();
    if (!Buff) return;
    const TWeakObjectPtr<USFBuffComponent> WeakBuff(Buff);
    const float Attack = 1.f + .1f * B.AttackStacks, Health = 1.f + .1f * B.HealthStacks;
    
    FInputBuffData BuffInfo;
    BuffInfo.KeyName = TEXT("StageSlimeReward");
    BuffInfo.DisplayName = FText::FromString(TEXT("슬라임 보상 · 이번 스테이지 한정"));
    BuffInfo.Description = GetSummary(Unit->GetTeamIndex()).ToString();
    BuffInfo.TickEvent = [WeakBuff, Attack, Health]()
        {
            if (auto* C = WeakBuff.Get())
            {
                C->Current_Status.AtkMul *= Attack;
                C->Current_Status.HPMul *= Health;
                C->ApplyBuffFlag(EBuffApply::Atk | EBuffApply::MaxHealth);
            }
        };
    BuffInfo.bIsBuff = true;
    Buff->AddBuff(BuffInfo);
}
