#pragma once
#include "CoreMinimal.h"
#include "InGame/Controller/SFInGamePlayerController.h"
#include "Datas/Building/SFBuildingType.h"
#include "SFPVPComputerController.generated.h"

/** Server-only opponent. Uses normal construction, production, combat and resource rules. */
UCLASS()
class STRIKEFORGE_API ASFPVPComputerController : public ASFInGamePlayerController
{
    GENERATED_BODY()
public:
    ASFPVPComputerController();
    virtual bool IsLocalController() const override { return false; }
    virtual bool ShouldParticipateInSeamlessTravel() const override { return false; }
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    void InitializeOpponent(class ASFPVPLobbyPlayerState* Seat);
    int32 GetAttackWaveCount() const { return AttackWaves; }
private:
    void Think();
    bool TryBuild(ESFBuildingType Type);
    void ProduceArmy();
    void CommandArmy();
    TWeakObjectPtr<class ASFBaseBuilding> OrderedBuilding;
    TSet<TWeakObjectPtr<class AUnitBase>> Attackers;
    FTimerHandle ThinkTimer;
    double BuildOrderedAt=0, StartedAt=0, LastWaveAt=0, LastCommanderOrder=0;
    int32 PlacementCursor=0, AttackWaves=0;
};
