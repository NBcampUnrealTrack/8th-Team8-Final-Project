#include "SFPVPSessionSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "Online/OnlineSessionNames.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "Engine/PendingNetGame.h"
#include "Containers/Ticker.h"
#include "GameFramework/PlayerController.h"
#include "Game/GameInstance/SFGameInstance.h"
#include "Interfaces/OnlineExternalUIInterface.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerState.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CommandLine.h"
#include "UI/SFTitlePresentation.h"
#include "SFPVPLobby.h"

bool USFPVPSessionSubsystem::IsInternetConfigured() const
{
    bool bEnabled=false;int32 AppId=0;
    return GConfig->GetBool(TEXT("OnlineSubsystemSteam"),TEXT("bEnabled"),bEnabled,GEngineIni)&&bEnabled
        &&GConfig->GetInt(TEXT("OnlineSubsystemSteam"),TEXT("SteamDevAppId"),AppId,GEngineIni)&&AppId>0;
}
bool USFPVPSessionSubsystem::IsInternetPlaySupported() const
{
    // UE disables OnlineSubsystemSteam inside the editor's PIE process, even in a separate preview window.
    return GetWorld() && GetWorld()->WorldType != EWorldType::PIE && (!GIsEditor || IsRunningGame())
        && !FParse::Param(FCommandLine::Get(),TEXT("forcepassthrough"));
}
IOnlineIdentityPtr USFPVPSessionSubsystem::Identity() const
{
    if(!CachedIdentity.IsValid()&&IsInternetConfigured()&&IsInternetPlaySupported())
        CachedIdentity=Online::GetIdentityInterface(GetWorld(),FName(TEXT("STEAM")));
    return CachedIdentity;
}
bool USFPVPSessionSubsystem::IsInternetLoggedIn() const
{
    // UI polling does not initialize Steam. The player explicitly checks their client connection.
    return CachedIdentity.IsValid()&&CachedIdentity->GetLoginStatus(0)==ELoginStatus::LoggedIn;
}
FString USFPVPSessionSubsystem::BrowserPrompt() const
{
    if(!bInternet)return TEXT("같은 네트워크의 방을 검색하거나 방을 만들어 주세요.");
    if(!IsInternetPlaySupported())return TEXT("에디터 미리보기에서는 Steam 대전을 지원하지 않습니다. Play-Steam.cmd로 별도 실행해 주세요.");
    if(!IsInternetConfigured())return TEXT("Steam 연결 설정이 필요합니다. App ID 설정 후 게임을 다시 실행해 주세요.");
    return IsInternetLoggedIn()?TEXT("Steam 인터넷 방을 검색하거나 방을 만들어 주세요."):TEXT("Steam을 실행해 로그인한 뒤 ‘Steam 연결’을 눌러 주세요.");
}
FString USFPVPSessionSubsystem::GetConnectionDescription() const
{
    return bInternet?TEXT("인터넷 대전  |  Steam 로비 / P2P 연결"):TEXT("같은 네트워크 대전  |  LAN 검색 / IP 주소 참가");
}
FString USFPVPSessionSubsystem::GetIdentityLabel() const
{
    return IsInternetLoggedIn()?FString::Printf(TEXT("Steam | %s"),*CachedIdentity->GetPlayerNickname(0)):TEXT("Steam 연결");
}
void USFPVPSessionSubsystem::ApplyLocalIdentity()
{
    if(auto* LP=GetGameInstance()->GetFirstGamePlayer())
    {
        const auto Account=bInternet?CachedIdentity:Online::GetIdentityInterface(GetWorld(),FName(TEXT("NULL")));
        if(Account.IsValid())LP->SetCachedUniqueNetId(FUniqueNetIdRepl(Account->GetUniquePlayerId(0)));
    }
}
void USFPVPSessionSubsystem::SetInternetMode(bool bEnabled)
{
    if(bBusy||GetWorld()->GetNetMode()!=NM_Standalone||bInternet==bEnabled)return;
    if(CachedSessions.IsValid()&&CachedSessions->GetNamedSession(NAME_GameSession))return;
    ClearSessionDelegates();CachedSessions.Reset();Rooms.Reset();Search.Reset();
    bInternet=bEnabled;ApplyLocalIdentity();Status=BrowserPrompt();
}
void USFPVPSessionSubsystem::LoginInternet()
{
    if(bBusy||!bInternet||GetWorld()->GetNetMode()!=NM_Standalone)return;
    if(!IsInternetPlaySupported()){Status=BrowserPrompt();return;}
    if(!IsInternetConfigured()){Status=BrowserPrompt();return;}
    const auto Account=Identity();
    if(!Account.IsValid()){Status=TEXT("Steam에 연결하지 못했습니다. Steam 실행 및 로그인 후 게임을 다시 실행해 주세요.");return;}
    if(!LoginStatusHandle.IsValid())LoginStatusHandle=Account->AddOnLoginStatusChangedDelegate_Handle(0,FOnLoginStatusChangedDelegate::CreateUObject(this,&ThisClass::LoginStatusChanged));
    if(IsInternetLoggedIn()){ApplyLocalIdentity();Sessions();Status=BrowserPrompt();return;}
    LoginHandle=Account->AddOnLoginCompleteDelegate_Handle(0,FOnLoginCompleteDelegate::CreateUObject(this,&ThisClass::LoginCompleted));
    bBusy=true;Status=TEXT("Steam 계정 연결을 확인하고 있습니다.");
    if(!Account->AutoLogin(0))
    {
        Account->ClearOnLoginCompleteDelegate_Handle(0,LoginHandle);LoginHandle.Reset();bBusy=false;
        Status=TEXT("Steam이 온라인 상태인지 확인한 뒤 다시 연결해 주세요.");
    }
}
void USFPVPSessionSubsystem::LoginCompleted(int32 LocalUser,bool bSuccess,const FUniqueNetId& Id,const FString& Error)
{
    if(CachedIdentity.IsValid())CachedIdentity->ClearOnLoginCompleteDelegate_Handle(LocalUser,LoginHandle);
    LoginHandle.Reset();bBusy=false;
    if(bSuccess&&IsInternetLoggedIn()){ApplyLocalIdentity();Sessions();Status=BrowserPrompt();}
    else Status=TEXT("Steam 로그인 상태를 확인하지 못했습니다. Steam에 로그인한 뒤 다시 연결해 주세요.");
    // Never log credentials, user IDs or authentication callback payloads.
    UE_LOG(LogTemp,Display,TEXT("[PVP] Steam connection completed: success=%d"),bSuccess);
}
void USFPVPSessionSubsystem::LoginStatusChanged(int32 LocalUser,ELoginStatus::Type Previous,ELoginStatus::Type Current,const FUniqueNetId& Id)
{
    if(!bInternet||Previous!=ELoginStatus::LoggedIn||Current==ELoginStatus::LoggedIn)return;
    Status=TEXT("Steam 연결이 종료되었습니다. Steam 온라인 상태를 확인해 주세요.");
    if(GetWorld()&&GetWorld()->GetNetMode()!=NM_Standalone)
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this,[this](float){Leave(false);return false;}),.1f);
}
bool USFPVPSessionSubsystem::EnsureOnlineReady()
{
    if(!bInternet)return true;
    if(!IsInternetConfigured()||!IsInternetLoggedIn()){Status=BrowserPrompt();return false;}
    ApplyLocalIdentity();return true;
}
void USFPVPSessionSubsystem::RegisterRoomPlayer(APlayerController* Player)
{
    if(!bInternet||!IsInternetLoggedIn()||!Player||!Player->PlayerState||!Player->HasAuthority()||GetWorld()->GetNetMode()==NM_Standalone)return;
    const auto& Id=Player->PlayerState->GetUniqueId();const auto S=Sessions();
    if(S.IsValid()&&Id.IsValid()&&!S->IsPlayerInSession(NAME_GameSession,*Id))S->RegisterPlayer(NAME_GameSession,*Id,false);
}
void USFPVPSessionSubsystem::UnregisterRoomPlayer(AController* Player)
{
    if(!bInternet||!Player||!Player->PlayerState||!Player->HasAuthority())return;
    const auto& Id=Player->PlayerState->GetUniqueId();const auto S=CachedSessions;
    if(S.IsValid()&&Id.IsValid()&&S->IsPlayerInSession(NAME_GameSession,*Id))S->UnregisterPlayer(NAME_GameSession,*Id);
}
void USFPVPSessionSubsystem::ShowSteamInvite()
{
    if(!bInternet||bBusy||!IsInternetLoggedIn()||GetWorld()->GetNetMode()==NM_Standalone)return;
    const auto UI=Online::GetExternalUIInterface(GetWorld(),FName(TEXT("STEAM")));
    if(!UI.IsValid()||!UI->ShowInviteUI(0,NAME_GameSession))
        Status=TEXT("Steam 초대 화면을 열지 못했습니다. Steam 오버레이 활성화 상태를 확인해 주세요.");
}
void USFPVPSessionSubsystem::InviteAccepted(bool bSuccess,int32 ControllerId,TSharedPtr<const FUniqueNetId> UserId,const FOnlineSessionSearchResult& Result)
{
    if(!bSuccess||ControllerId!=0)return;
    if(bBusy||GetWorld()->GetNetMode()!=NM_Standalone){Status=TEXT("현재 방을 나간 뒤 친구 초대를 다시 수락해 주세요.");return;}
    FString Version;
    if(!Result.IsValid()||!Result.Session.SessionSettings.Get(FName(TEXT("SF_PROTOCOL")),Version)||Version!=Protocol)
    {Status=TEXT("이 초대는 같은 버전의 StrikeForge 방이 아닙니다.");return;}
    SetInternetMode(true);
    if(!EnsureOnlineReady())return;
    auto Room=Result;
    Room.Session.SessionSettings.bUsesPresence=true;Room.Session.SessionSettings.bUseLobbiesIfAvailable=true;
    Rooms.Reset();Rooms.Add(MoveTemp(Room));JoinRoom(0);
}

void USFPVPSessionSubsystem::CloseJoining()
{
    if(auto S=Sessions())if(auto* Session=S->GetNamedSession(NAME_GameSession))
    {
        auto Settings=Session->SessionSettings;Settings.bShouldAdvertise=false;Settings.bAllowJoinInProgress=false;
        S->UpdateSession(NAME_GameSession,Settings,true);
    }
}

IOnlineSessionPtr USFPVPSessionSubsystem::Sessions()
{
    if (!CachedSessions.IsValid() && GEngine && GetWorld() && GEngine->GetWorldContextFromWorld(GetWorld()))
    {
        if(bInternet&&!IsInternetConfigured())return nullptr;
        CachedSessions = Online::GetSessionInterface(GetWorld(),FName(bInternet?TEXT("STEAM"):TEXT("NULL")));
    }
    if(bInternet&&CachedSessions.IsValid()&&!InviteHandle.IsValid())
        InviteHandle=CachedSessions->AddOnSessionUserInviteAcceptedDelegate_Handle(FOnSessionUserInviteAcceptedDelegate::CreateUObject(this,&ThisClass::InviteAccepted));
    return CachedSessions;
}
void USFPVPSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    NetworkHandle = GEngine->OnNetworkFailure().AddUObject(this, &ThisClass::NetworkFailure);
    TravelHandle = GEngine->OnTravelFailure().AddUObject(this, &ThisClass::TravelFailure);
}
void USFPVPSessionSubsystem::ClearSessionDelegates()
{
    if (auto S = CachedSessions)
    {
        S->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
        S->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
        S->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
        S->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle);
        S->ClearOnSessionUserInviteAcceptedDelegate_Handle(InviteHandle);
    }
    InviteHandle.Reset();
}
void USFPVPSessionSubsystem::Deinitialize()
{
    ClearSessionDelegates();
    if (GEngine) { GEngine->OnNetworkFailure().Remove(NetworkHandle); GEngine->OnTravelFailure().Remove(TravelHandle); }
    CachedSessions.Reset();
    if(CachedIdentity.IsValid())
    {
        CachedIdentity->ClearOnLoginCompleteDelegate_Handle(0,LoginHandle);
        CachedIdentity->ClearOnLoginStatusChangedDelegate_Handle(0,LoginStatusHandle);
    }
    CachedIdentity.Reset();
    Super::Deinitialize();
}
void USFPVPSessionSubsystem::OpenBrowser()
{
    if (bBusy) return;
    if(UGameplayStatics::GetCurrentLevelName(this,true)==TEXT("Start")&&GetWorld()->GetNetMode()!=NM_Standalone)
    {
        // Editor multiplayer already has connected players. Preserve that connection instead of
        // OpenLevel disconnecting both clients and creating a separate seat 0 for each of them.
        if(GetWorld()->GetNetMode()==NM_Client)
        {if(auto* PC=Cast<ASFTitleController>(GetGameInstance()->GetFirstLocalPlayerController()))PC->ServerOpenPVPLobby();return;}
        bInternet=false;ApplyLocalIdentity();
        GetWorld()->ServerTravel(FString(LobbyMap)+TEXT("?game=/Script/StrikeForge.SFPVPLobbyMode?SFOnline=0"),true);return;
    }
    Status = BrowserPrompt();
    UGameplayStatics::OpenLevel(this, FName(LobbyMap));
}
void USFPVPSessionSubsystem::Host(const FString& Name)
{
    if (bBusy || GetWorld()->GetNetMode() != NM_Standalone) return;
    if(!EnsureOnlineReady())return;
    auto S = Sessions();
    if (!S.IsValid()) { Status = TEXT("네트워크 서비스를 사용할 수 없습니다."); return; }
    if (S->GetNamedSession(NAME_GameSession)) { Status = TEXT("이전 방을 나간 뒤 다시 만들어 주세요."); return; }
    RoomName = Name.TrimStartAndEnd().Left(32).Replace(TEXT("\n"), TEXT(" ")).Replace(TEXT("\r"), TEXT(" "));
    if (RoomName.IsEmpty()) RoomName = TEXT("StrikeForge 1대1");
    FOnlineSessionSettings Settings;
    Settings.bIsLANMatch = !bInternet;
    Settings.bUseLobbiesIfAvailable=bInternet;
    Settings.bUsesPresence=bInternet;
    Settings.bAllowJoinViaPresence=bInternet;
    Settings.bAllowInvites=bInternet;
    Settings.NumPublicConnections = 2;
    Settings.bShouldAdvertise = true;
    Settings.bAllowJoinInProgress = true;
    Settings.Set(FName(TEXT("SF_PROTOCOL")), FString(Protocol), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
    Settings.Set(FName(TEXT("SF_ROOM")), RoomName, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
    bBusy = true; Status = TEXT("방을 만들고 있습니다...");
    CreateHandle = S->AddOnCreateSessionCompleteDelegate_Handle(FOnCreateSessionCompleteDelegate::CreateUObject(this, &ThisClass::Created));
    if (!S->CreateSession(0, NAME_GameSession, Settings)) Created(NAME_GameSession, false);
}
void USFPVPSessionSubsystem::Created(FName Name, bool bSuccess)
{
    if (auto S = Sessions()) S->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
    if (!bSuccess) { bBusy = false; Status = TEXT("방 생성에 실패했습니다. 다시 시도해 주세요."); return; }
    UE_LOG(LogTemp, Display, TEXT("[PVP] Session created: %s"), *RoomName);
    UGameplayStatics::OpenLevel(this,FName(LobbyMap),true,bInternet?TEXT("listen"):TEXT("listen?bIsLanMatch=1"));
}
void USFPVPSessionSubsystem::FindRooms()
{
    if (bBusy || GetWorld()->GetNetMode() != NM_Standalone) return;
    if(!EnsureOnlineReady())return;
    auto S = Sessions(); if (!S.IsValid()) return;
    Rooms.Reset(); Search = MakeShared<FOnlineSessionSearch>();
    Search->bIsLanQuery = !bInternet; Search->MaxSearchResults = 50; Search->TimeoutInSeconds = bInternet?15:5;
    if(bInternet)
    {
        Search->QuerySettings.Set(SEARCH_LOBBIES,true,EOnlineComparisonOp::Equals);
        Search->QuerySettings.Set(FName(TEXT("SF_PROTOCOL")),FString(Protocol),EOnlineComparisonOp::Equals);
    }
    bBusy = true; Status = TEXT("방을 찾고 있습니다...");
    FindHandle = S->AddOnFindSessionsCompleteDelegate_Handle(FOnFindSessionsCompleteDelegate::CreateUObject(this, &ThisClass::Found));
    if (!S->FindSessions(0, Search.ToSharedRef())) Found(false);
}
void USFPVPSessionSubsystem::Found(bool bSuccess)
{
    if (auto S = Sessions()) S->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
    bBusy = false;
    if (bSuccess && Search)
        for (const auto& R : Search->SearchResults)
        {
            FString Version;
            if(R.Session.SessionSettings.Get(FName(TEXT("SF_PROTOCOL")),Version)&&Version==Protocol)
            {
                auto Room=R;
                // UE 5.5's lobby search deserializer does not restore this flag.
                // It must match bUsesPresence when calling Steam JoinSession.
                if(bInternet){Room.Session.SessionSettings.bUsesPresence=true;Room.Session.SessionSettings.bUseLobbiesIfAvailable=true;}
                Rooms.Add(MoveTemp(Room));
            }
        }
    Status = !bSuccess ? TEXT("검색에 실패했습니다. 네트워크를 확인해 주세요.") : Rooms.IsEmpty()
        ? (bInternet?TEXT("검색된 인터넷 방이 없습니다. 상대가 방을 만든 뒤 새로고침해 주세요."):TEXT("검색된 방이 없습니다. 같은 네트워크에서 방을 만들거나 주소로 참가하세요."))
        : FString::Printf(TEXT("%d개의 방을 찾았습니다."), Rooms.Num());
    UE_LOG(LogTemp, Display, TEXT("[PVP] Search complete: %d rooms"), Rooms.Num());
}
FString USFPVPSessionSubsystem::DescribeRoom(int32 Index) const
{
    if (!Rooms.IsValidIndex(Index)) return FString();
    const auto& R = Rooms[Index]; FString Name;
    R.Session.SessionSettings.Get(FName(TEXT("SF_ROOM")), Name);
    return FString::Printf(TEXT("%s     %d / 2"), *Name, 2 - R.Session.NumOpenPublicConnections);
}
void USFPVPSessionSubsystem::JoinRoom(int32 Index)
{
    if (bBusy || !Rooms.IsValidIndex(Index) || GetWorld()->GetNetMode() != NM_Standalone) return;
    if(!EnsureOnlineReady())return;
    if(bInternet&&CachedIdentity.IsValid())
    {
        const auto Id=CachedIdentity->GetUniquePlayerId(0);
        if(Id.IsValid()&&Rooms[Index].Session.OwningUserId.IsValid()&&*Id==*Rooms[Index].Session.OwningUserId)
        {Status=TEXT("같은 Steam 계정으로 자신의 방에 참가할 수 없습니다. 두 PC에서 서로 다른 계정을 사용해 주세요.");return;}
    }
    if (Rooms[Index].Session.NumOpenPublicConnections <= 0) { Status = TEXT("이미 가득 찬 방입니다."); return; }
    auto S = Sessions(); if (!S.IsValid()) return;
    bBusy = true; Status = TEXT("방에 접속하고 있습니다...");
    JoinHandle = S->AddOnJoinSessionCompleteDelegate_Handle(FOnJoinSessionCompleteDelegate::CreateUObject(this, &ThisClass::Joined));
    if (!S->JoinSession(0, NAME_GameSession, Rooms[Index])) Joined(NAME_GameSession, EOnJoinSessionCompleteResult::UnknownError);
}
void USFPVPSessionSubsystem::Joined(FName Name, EOnJoinSessionCompleteResult::Type Result)
{
    auto S = Sessions();
    if (S) S->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
    FString URL;
    if (Result == EOnJoinSessionCompleteResult::Success && S && S->GetResolvedConnectString(Name, URL))
    {
        if (auto* PC = GetGameInstance()->GetFirstLocalPlayerController())
        {
            // Internet lobbies must resolve to a Steam P2P address.
            if(bInternet&&!URL.StartsWith(TEXT("steam."),ESearchCase::IgnoreCase))
            {Status=TEXT("인터넷 방의 P2P 주소를 확인하지 못했습니다. 방장과 같은 버전인지 확인해 주세요.");bBusy=false;Leave(false);return;}
            PC->ClientTravel(URL+FString::Printf(TEXT("?SFProtocol=%s?SFOnline=%d"),ProtocolVersion,bInternet?1:0),TRAVEL_Absolute);return;
        }
    }
    Status = TEXT("방에 참가하지 못했습니다. 방이 닫혔거나 가득 찼을 수 있습니다.");
    bBusy = false; Leave(false);
}
void USFPVPSessionSubsystem::ConnectAddress(const FString& Address)
{
    if (bBusy || GetWorld()->GetNetMode() != NM_Standalone) return;
    if(bInternet){Status=TEXT("인터넷 대전은 Steam 연결 후 방 목록이나 친구 초대로 참가해 주세요.");return;}
    const FString URL = Address.TrimStartAndEnd();
    // Only a host/IP and optional port, never a map path or URL options supplied by a user.
    if (URL.IsEmpty() || URL.Len() > 253 || !URL.Contains(TEXT(".")))
    { Status = TEXT("방장의 IPv4 주소를 입력해 주세요. 예: 192.168.0.10:7777"); return; }
    for (TCHAR C : URL) if (!FChar::IsDigit(C) && C != TEXT('.') && C != TEXT(':'))
    { Status = TEXT("IP 주소와 포트만 입력할 수 있습니다."); return; }
    if (auto* PC = GetGameInstance()->GetFirstLocalPlayerController())
    {
        bBusy = true; Status = TEXT("주소로 접속하고 있습니다...");
        PC->ClientTravel(URL+FString::Printf(TEXT("?SFProtocol=%s?SFOnline=0"),ProtocolVersion),TRAVEL_Absolute);
    }
}
void USFPVPSessionSubsystem::LobbyArrived()
{
    bBusy = false; bLeaving = false; bRecovering = false;
    if(GetWorld()->GetNetMode()!=NM_Standalone)if(const auto* State=GetWorld()->GetGameState<ASFPVPLobbyState>())
    {
        if(bInternet!=State->bInternetRoom){ClearSessionDelegates();CachedSessions.Reset();Rooms.Reset();bInternet=State->bInternetRoom;}
    }
    if (GetWorld()->GetNetMode() != NM_Standalone) Status = TEXT("종족을 선택하고 준비해 주세요.");
    else if (Status.IsEmpty() || Status == TEXT("종족을 선택하고 준비해 주세요."))Status=BrowserPrompt();
    ApplyLocalIdentity();
    if(bInternet&&GetWorld()->GetNetMode()==NM_Standalone&&IsInternetPlaySupported())
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this,[this](float){LoginInternet();return false;}),.1f);
}
void USFPVPSessionSubsystem::Leave(bool bInToTitle)
{
    if (bLeaving) return;
    bLeaving = true; bBusy = true; bToTitle = bInToTitle;
    ClearSessionDelegates();
    auto S = Sessions();
    if (S && S->GetNamedSession(NAME_GameSession))
    {
        DestroyHandle = S->AddOnDestroySessionCompleteDelegate_Handle(FOnDestroySessionCompleteDelegate::CreateUObject(this, &ThisClass::Destroyed));
        if (!S->DestroySession(NAME_GameSession)) Destroyed(NAME_GameSession, false);
    }
    else ReturnAfterLeave();
}
void USFPVPSessionSubsystem::Destroyed(FName Name, bool bSuccess)
{
    if (auto S = Sessions()) S->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle);
    ReturnAfterLeave();
}
void USFPVPSessionSubsystem::ReturnAfterLeave()
{
    bBusy = false; bLeaving = false; Rooms.Reset();
    if (bToTitle)
    {
        if (auto* GI = Cast<USFGameInstance>(GetGameInstance())) { GI->MoveToTitle(); return; }
    }
    UGameplayStatics::OpenLevel(this, FName(LobbyMap));
}
void USFPVPSessionSubsystem::NetworkFailure(UWorld* World, UNetDriver* Driver, ENetworkFailure::Type Type, const FString& Error)
{
    UWorld* LocalWorld = GetWorld();
    if (!LocalWorld || (World && World != LocalWorld) || (!LocalWorld->GetMapName().Contains(TEXT("PVP"))) || bRecovering) return;
    if (!World)
    {
        const auto* Context = GetGameInstance()->GetWorldContext();
        if (!Context || !Context->PendingNetGame || Context->PendingNetGame->NetDriver != Driver) return;
    }
    UE_LOG(LogTemp, Warning, TEXT("[PVP] Connection failed: %s"), *Error);
    Status = Error.Contains(TEXT("full")) || Error.Contains(TEXT("FULL"))
        ? TEXT("방이 가득 차서 참가하지 못했습니다. 다른 방을 선택해 주세요.")
        : TEXT("방과의 연결이 종료되었습니다. 방 목록에서 다시 참가해 주세요.");
    bRecovering = true;
    // The engine schedules its own disconnect travel during this broadcast.
    // Recover after that operation, including failures of a pending connection (World == nullptr).
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this](float)
    { Leave(false); return false; }), .1f);
}
void USFPVPSessionSubsystem::TravelFailure(UWorld* World, ETravelFailure::Type Type, const FString& Error)
{
    if (World != GetWorld() || !bBusy) return;
    Status = TEXT("로비 이동에 실패했습니다. 다시 시도해 주세요."); Leave(false);
}
