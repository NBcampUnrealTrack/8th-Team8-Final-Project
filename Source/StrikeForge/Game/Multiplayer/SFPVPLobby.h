#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/PlayerController.h"
#include "Datas/Race/SFRaceType.h"
#include "SFPVPLobby.generated.h"

UCLASS()
class STRIKEFORGE_API ASFPVPLobbyPlayerState : public APlayerState
{
    GENERATED_BODY()
public:
    UPROPERTY(Replicated) int32 Seat = INDEX_NONE;
    UPROPERTY(Replicated) ESFRace Race = ESFRace::Human;
    UPROPERTY(Replicated) bool bReady = false;
    virtual void CopyProperties(APlayerState* PlayerState) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};

UCLASS()
class STRIKEFORGE_API ASFPVPLobbyState : public AGameStateBase
{
    GENERATED_BODY()
public:
    UPROPERTY(Replicated) FString RoomName;
    UPROPERTY(Replicated) bool bInternetRoom = false;
    UPROPERTY(Replicated) bool bStarting = false;
    UPROPERTY(Replicated) FString StartError;
    ASFPVPLobbyPlayerState* GetSeat(int32 Index) const;
    bool AreBothReady() const;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};

UCLASS()
class STRIKEFORGE_API ASFPVPLobbyController : public APlayerController
{
    GENERATED_BODY()
public:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    UFUNCTION(Server, Reliable) void ServerSetRace(ESFRace Race);
    UFUNCTION(Server, Reliable) void ServerSetReady(bool bReady);
    UFUNCTION(Server, Reliable) void ServerStartBattle();
private:
    UPROPERTY(Transient) TObjectPtr<class USFPVPLobbyWidget> LobbyWidget;
};

UCLASS()
class STRIKEFORGE_API ASFPVPLobbyMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    ASFPVPLobbyMode();
    virtual void InitGameState() override;
    virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void Logout(AController* Exiting) override;
};
