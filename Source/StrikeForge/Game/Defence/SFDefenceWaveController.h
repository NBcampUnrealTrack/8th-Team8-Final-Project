#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SFDefenceWaveController.generated.h"

class AUnitSpawner;
class AUnitBase;
class USFDefenceTimerWidget;

UENUM(BlueprintType)
enum class ESFDefencePhase : uint8
{
    WaitingForTutorial,
    Preparation,
    Wave,
    Stopped
};

USTRUCT(BlueprintType)
struct FSFDefenceUnitEntry
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName RowName;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftClassPtr<AUnitBase> UnitClass;
    /** Relative share of the wave budget. Higher tiers use smaller weights. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1")) int32 SpawnWeight = 1;
};

/** Place one controller and three standalone enemy spawners in a defence level. */
UCLASS()
class STRIKEFORGE_API ASFDefenceWaveController : public AActor
{
    GENERATED_BODY()
public:
    ASFDefenceWaveController();
    virtual void Tick(float DeltaSeconds) override;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "SF|Defence")
    TArray<TObjectPtr<AUnitSpawner>> Spawners;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SF|Defence", meta = (ClampMin = "1"))
    int32 TotalWaves = 5;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SF|Defence", meta = (ClampMin = "1"))
    float InitialPreparationDuration = 60.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SF|Defence", meta = (ClampMin = "1"))
    float PreparationDuration = 180.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SF|Defence", meta = (ClampMin = "1"))
    float WaveDuration = 300.f;
    /** Stage06 requires the full defence time even after its spawn budget is exhausted. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SF|Defence")
    bool bRequireFullWaveDuration = false;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SF|Defence", meta = (ClampMin = "0.1"))
    float SpawnInterval = 4.f;
    /** Baseline copies per type before the total budget is scaled and weighted. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SF|Defence", meta = (ClampMin = "1"))
    int32 FirstWaveCopiesPerType = 6;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SF|Defence", meta = (ClampMin = "1"))
    int32 AdditionalCopiesPerWave = 3;
    /** Scale the total budget before distributing it among unit types. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SF|Defence", meta = (ClampMin = "0.01"))
    float SpawnCountMultiplier = 1.f;
    /** Barracks/ranged tiers only: special factory units are intentionally excluded. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SF|Defence")
    TArray<FSFDefenceUnitEntry> UnitPool;

    UFUNCTION(BlueprintPure, Category = "SF|Defence") ESFDefencePhase GetPhase() const { return Phase; }
    UFUNCTION(BlueprintPure, Category = "SF|Defence") int32 GetWaveNumber() const { return WaveNumber; }
    UFUNCTION(BlueprintPure, Category = "SF|Defence") float GetRemainingSeconds() const { return RemainingSeconds; }
    UFUNCTION(BlueprintPure, Category = "SF|Defence") int32 GetRemainingSpawnCount() const;
    UFUNCTION(BlueprintCallable, Category = "SF|Defence") void StopWaves();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
#if WITH_DEV_AUTOMATION_TESTS
    friend class FSFDefenceWaveFlowTest;
#endif
    void BeginPreparation();
    void BeginWave();
    void CompleteWave();
    bool HasLivingEnemies() const;
    void SpawnBatch();
    void BuildSpawnOrder();
    UPROPERTY(Transient) TArray<TSubclassOf<AUnitBase>> LoadedClasses;
    UPROPERTY(Transient) TObjectPtr<USFDefenceTimerWidget> TimerWidget;
    ESFDefencePhase Phase = ESFDefencePhase::WaitingForTutorial;
    int32 WaveNumber = 0;
    float RemainingSeconds = 0.f;
    float SpawnCountdown = 0.f;
    int32 TotalPlannedUnits = 0;
    TArray<int32> PlannedPerDirection;
    TArray<int32> SpawnedPerDirection;
    TArray<int32> SpawnOrder;
};
