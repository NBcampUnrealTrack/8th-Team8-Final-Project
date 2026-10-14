#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SFAmbushDefenseDirector.generated.h"

class AUnitSpawner;
class ULoadingWidget;

UENUM(BlueprintType)
enum class ESFAmbushPhase : uint8 { Waiting, Secured, Defending, Escaped };

/** Map-local survival objective. Does not change victory rules on other stages. */
UCLASS()
class STRIKEFORGE_API ASFAmbushDefenseDirector : public AActor
{
    GENERATED_BODY()
public:
    ASFAmbushDefenseDirector();
    virtual void Tick(float DeltaSeconds) override;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ambush", meta=(ClampMin="0")) float PreparationSeconds = 8.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ambush", meta=(ClampMin="1")) float HoldSeconds = 240.f;
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Ambush") TArray<TObjectPtr<AUnitSpawner>> Spawners;
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Ambush") TArray<TObjectPtr<AActor>> IntactBridge;
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Ambush") TArray<TObjectPtr<AActor>> BrokenBridge;
    UPROPERTY(EditDefaultsOnly, Category="Ambush") TSoftClassPtr<ULoadingWidget> LoadingWidgetClass;
    UFUNCTION(BlueprintPure, Category="Ambush") ESFAmbushPhase GetPhase() const { return Phase; }
    UFUNCTION(BlueprintPure, Category="Ambush") float GetRemainingSeconds() const { return RemainingSeconds; }
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    void SetBridgeBroken(bool bBroken);
    void StopSpawners();
    void EnsureLoadingScreen();
    UPROPERTY(VisibleInstanceOnly, Category="Ambush") ESFAmbushPhase Phase = ESFAmbushPhase::Waiting;
    UPROPERTY(VisibleInstanceOnly, Category="Ambush") float RemainingSeconds = 0.f;
    UPROPERTY(Transient) TObjectPtr<ULoadingWidget> LoadingScreen;
    bool bLoadingScreenHandled = false;
};
