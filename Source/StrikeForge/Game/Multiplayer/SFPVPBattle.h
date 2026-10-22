#pragma once
#include "CoreMinimal.h"
#include "SFPVPLobby.h"
#include "InGame/Controller/SFInGamePlayerController.h"
#include "Units/Unit/UnitBase.h"
#include "SFPVPBattle.generated.h"

class ASFCommander;
class ASFNexusBuilding;

USTRUCT()
struct FSFPVPTeamResources
{
    GENERATED_BODY()
    UPROPERTY() int32 Mineral=0;
    UPROPERTY() int32 Ether=0;
    UPROPERTY() float MineralRate=0;
    UPROPERTY() float EtherRate=0;
    UPROPERTY() int32 Population=0;
    UPROPERTY() int32 Capacity=0;
};

UCLASS()
class STRIKEFORGE_API ASFPVPBattleState : public ASFPVPLobbyState
{
    GENERATED_BODY()
public:
    virtual bool IsPVPMatch() const override { return true; }
    UPROPERTY(Replicated) bool bBattleReady=false;
    UPROPERTY(Replicated) FString BattleStatus=TEXT("두 플레이어의 전장 입장을 기다리고 있습니다.");
    UPROPERTY(Replicated) FVector Base0=FVector::ZeroVector;
    UPROPERTY(Replicated) FVector Base1=FVector::ZeroVector;
    UPROPERTY(Replicated) TArray<ESFRace> MatchRaces;
    UPROPERTY(Replicated) TArray<TObjectPtr<ASFNexusBuilding>> Nexuses;
    UPROPERTY(Replicated) TArray<TObjectPtr<ASFCommander>> Commanders;
    UPROPERTY(Replicated) int32 WinnerTeam=INDEX_NONE;
    UPROPERTY(ReplicatedUsing=OnRep_Resources) TArray<FSFPVPTeamResources> Resources;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    bool PrepareGameplay();
    void SyncResources();
    void ApplyMatchResult();
    void FailPreparation(const FString& Reason) { bAssetFailure=true;BattleStatus=Reason;ForceNetUpdate(); }
private:
    UFUNCTION() void OnRep_Resources();
    TSet<ESFRace> RequestedRaces;
    TSet<FString> RequestedBuildingTiers;
    TSharedPtr<struct FStreamableHandle> CommanderLoad;
    UPROPERTY(Replicated) bool bAssetFailure=false;
};

UCLASS()
class STRIKEFORGE_API ASFPVPBattleMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    ASFPVPBattleMode();
    virtual void InitGameState() override;
    virtual void GetSeamlessTravelActorList(bool bToTransition,TArray<AActor*>& Actors) override;
    virtual void BeginPlay() override;
    virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
    virtual void PreLogin(const FString& Options,const FString& Address,const FUniqueNetIdRepl& UniqueId,FString& Error) override;
    virtual void Logout(AController* Exiting) override;
    void NexusDestroyed(int32 Team);
private:
    void TrySpawnArmies();
    bool FindCampaignBases(FTransform& A,FTransform& B) const;
    FTimerHandle SpawnTimer;
    double WaitStarted=0;
    UPROPERTY() TObjectPtr<class ASFPVPComputerController> Computer;
};

UCLASS()
class STRIKEFORGE_API ASFPVPBattleController : public ASFInGamePlayerController
{
    GENERATED_BODY()
public:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void SetupInputComponent() override;
    virtual void PlayerTick(float DeltaTime) override;
    UFUNCTION(Server,Reliable) void ServerPresentationReady();
    UFUNCTION(Server,Reliable) void ServerCommand(const TArray<AUnitBase*>& Units,ESFUnitCommand Command,FVector_NetQuantize Location,AUnitBase* Target);
    void SelectAll();
    void CommandSelected(ESFUnitCommand Command,FVector Location,AUnitBase* Target=nullptr);
    const TArray<TWeakObjectPtr<AUnitBase>>& GetSelection() const { return Selected; }
    int32 GetBattleTeam() const;
    void FocusArmy();
    void LeaveBattle();
    bool IsPresentationReady() const { return bPresentationReady; }
private:
    UPROPERTY(Transient) TObjectPtr<class USFBattleMusicComponent> BattleMusic;
    UPROPERTY(Transient) TObjectPtr<class ULoadingWidget> LoadingWidget;
    TArray<TWeakObjectPtr<AUnitBase>> Selected;
    bool bPresentationReady=false;
    bool bSentReady=false;
    bool bFocused=false;
};
