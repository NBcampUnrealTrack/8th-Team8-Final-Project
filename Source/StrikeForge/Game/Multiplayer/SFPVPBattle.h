#pragma once
#include "CoreMinimal.h"
#include "SFPVPLobby.h"
#include "InGame/Controller/SFInGamePlayerController.h"
#include "Units/Unit/UnitBase.h"
#include "GameFramework/HUD.h"
#include "SFPVPBattle.generated.h"

UCLASS()
class STRIKEFORGE_API ASFPVPBattleState : public ASFPVPLobbyState
{
    GENERATED_BODY()
public:
    UPROPERTY(Replicated) bool bBattleReady = false;
    UPROPERTY(Replicated) FString BattleStatus = TEXT("두 플레이어의 전장 입장을 기다리고 있습니다.");
    UPROPERTY(Replicated) FVector Base0 = FVector::ZeroVector;
    UPROPERTY(Replicated) FVector Base1 = FVector::ZeroVector;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};

UCLASS()
class STRIKEFORGE_API ASFPVPBattleMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    ASFPVPBattleMode();
    virtual void BeginPlay() override;
    virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
    virtual void PreLogin(const FString& Options,const FString& Address,const FUniqueNetIdRepl& UniqueId,FString& Error) override;
    virtual void Logout(AController* Exiting) override;
private:
    void TrySpawnArmies();
    bool FindBattleGround(FVector& A,FVector& B) const;
    UPROPERTY() TArray<TSubclassOf<AUnitBase>> StartingUnits;
    FTimerHandle SpawnTimer;
    double WaitStarted = 0;
};

UCLASS()
class STRIKEFORGE_API ASFPVPBattleController : public ASFInGamePlayerController
{
    GENERATED_BODY()
public:
    ASFPVPBattleController();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void SetupInputComponent() override;
    virtual void PlayerTick(float DeltaTime) override;
    UFUNCTION(Server, Reliable) void ServerCommand(const TArray<AUnitBase*>& Units, ESFUnitCommand Command, FVector_NetQuantize Location, AUnitBase* Target);
    void SelectAll();
    void CommandSelected(ESFUnitCommand Command,FVector Location,AUnitBase* Target=nullptr);
    const TArray<TWeakObjectPtr<AUnitBase>>& GetSelection() const { return Selected; }
    bool IsDragging() const { return bDragging; }
    FVector2D GetDragStart() const { return DragStart; }
    int32 GetBattleTeam() const;
    void FocusArmy();
    void LeaveBattle();
private:
    void SelectPressed();
    void SelectReleased();
    void RightClick();
    void ArmAttack();
    void Stop();
    void ZoomIn();
    void ZoomOut();
    UPROPERTY(Transient) TObjectPtr<class ACameraActor> BattleCamera;
    TArray<TWeakObjectPtr<AUnitBase>> Selected;
    FVector CameraFocus=FVector::ZeroVector;
    float CameraHeight=1700.f;
    bool bFocused=false,bDragging=false,bAttackPending=false;
    FVector2D DragStart;
    double LastCommandTime=-1;
};

UCLASS()
class STRIKEFORGE_API ASFPVPBattleHUD : public AHUD
{
    GENERATED_BODY()
public:
    ASFPVPBattleHUD();
    virtual void DrawHUD() override;
private:
    UPROPERTY() TObjectPtr<class UFont> UIFont;
};
