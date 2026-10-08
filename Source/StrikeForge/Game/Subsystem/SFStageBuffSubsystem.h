#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SFStageBuffSubsystem.generated.h"

class AUnitBase;
UENUM(BlueprintType)
enum class ESFStageReward : uint8 { Resources, Attack, Health };

USTRUCT(BlueprintType)
struct FSFStageBuffs
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) int32 ResourceRewards = 0;
    UPROPERTY(BlueprintReadOnly) int32 AttackStacks = 0;
    UPROPERTY(BlueprintReadOnly) int32 HealthStacks = 0;
};

/** Owned by the current world: retries and travel start with no rewards. */
UCLASS()
class STRIKEFORGE_API USFStageBuffSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintPure, Category="SF|Stage Rewards", meta=(WorldContext="WorldContextObject"))
    static USFStageBuffSubsystem* GetStageBuffs(const UObject* WorldContextObject);
    UFUNCTION(BlueprintPure) FSFStageBuffs GetBuffs(int32 Team) const;
    UFUNCTION(BlueprintPure) FText GetSummary(int32 Team) const;
    void GrantRandomReward(int32 Team);
    void GrantReward(int32 Team, ESFStageReward Reward);
    void ApplyToUnit(AUnitBase* Unit);
private:
    UPROPERTY() TMap<int32, FSFStageBuffs> TeamBuffs;
};
