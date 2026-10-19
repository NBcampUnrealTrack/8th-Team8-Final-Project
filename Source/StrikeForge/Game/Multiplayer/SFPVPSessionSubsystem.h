#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "SFPVPSessionSubsystem.generated.h"

/** Steam Internet lobbies and Null LAN discovery, authentication and listen-server lifecycle. */
UCLASS()
class STRIKEFORGE_API USFPVPSessionSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    static constexpr const TCHAR* LobbyMap = TEXT("/Game/Core/Levels/PVPLobby");
    static constexpr const TCHAR* Protocol = TEXT("StrikeForge.Steam.3");
    static constexpr const TCHAR* ProtocolVersion = TEXT("3");
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    void OpenBrowser();
    void Host(const FString& Name);
    void FindRooms();
    void JoinRoom(int32 Index);
    void ConnectAddress(const FString& Address);
    void Leave(bool bToTitle = false);
    bool IsBusy() const { return bBusy; }
    const FString& GetStatus() const { return Status; }
    const FString& GetRoomName() const { return RoomName; }
    const TArray<FOnlineSessionSearchResult>& GetRooms() const { return Rooms; }
    FString DescribeRoom(int32 Index) const;
    void LobbyArrived();
    void CloseJoining();
    void SetInternetMode(bool bEnabled);
    bool IsInternetMode() const { return bInternet; }
    bool IsInternetConfigured() const;
    bool IsInternetPlaySupported() const;
    bool IsInternetLoggedIn() const;
    void LoginInternet();
    FString GetConnectionDescription() const;
    FString GetIdentityLabel() const;
    void ShowSteamInvite();
    void RegisterRoomPlayer(APlayerController* Player);
    void UnregisterRoomPlayer(AController* Player);
private:
    IOnlineIdentityPtr Identity() const;
    bool EnsureOnlineReady();
    void LoginCompleted(int32 LocalUser, bool bSuccess, const FUniqueNetId& Id, const FString& Error);
    void LoginStatusChanged(int32 LocalUser, ELoginStatus::Type Previous, ELoginStatus::Type Current, const FUniqueNetId& Id);
    void ApplyLocalIdentity();
    void InviteAccepted(bool bSuccess,int32 ControllerId,TSharedPtr<const FUniqueNetId> UserId,const FOnlineSessionSearchResult& Result);
    FString BrowserPrompt() const;
    IOnlineSessionPtr Sessions();
    void Created(FName Name, bool bSuccess);
    void Found(bool bSuccess);
    void Joined(FName Name, EOnJoinSessionCompleteResult::Type Result);
    void Destroyed(FName Name, bool bSuccess);
    void NetworkFailure(UWorld* World, UNetDriver* Driver, ENetworkFailure::Type Type, const FString& Error);
    void TravelFailure(UWorld* World, ETravelFailure::Type Type, const FString& Error);
    void ClearSessionDelegates();
    void ReturnAfterLeave();
    IOnlineSessionPtr CachedSessions;
    mutable IOnlineIdentityPtr CachedIdentity;
    FDelegateHandle CreateHandle, FindHandle, JoinHandle, DestroyHandle, NetworkHandle, TravelHandle;
    FDelegateHandle LoginHandle, LoginStatusHandle;
    FDelegateHandle InviteHandle;
    TSharedPtr<FOnlineSessionSearch> Search;
    TArray<FOnlineSessionSearchResult> Rooms;
    bool bBusy = false;
    bool bToTitle = false;
    bool bLeaving = false;
    bool bRecovering = false;
    bool bInternet = true;
    FString Status;
    FString RoomName;
};
