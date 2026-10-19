#include "SFPVPLobby.h"
#include "SFPVPSessionSubsystem.h"
#include "UI/SFPVPLobbyWidget.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/GameSession.h"
#include "Game/GameInstance/SFGameInstance.h"
#include "Misc/PackageName.h"

void ASFPVPLobbyPlayerState::CopyProperties(APlayerState* PlayerState)
{
    Super::CopyProperties(PlayerState);
    if(auto* P=Cast<ASFPVPLobbyPlayerState>(PlayerState)){P->Seat=Seat;P->Race=Race;P->bReady=bReady;}
}

void ASFPVPLobbyPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ASFPVPLobbyPlayerState, Seat);
    DOREPLIFETIME(ASFPVPLobbyPlayerState, Race);
    DOREPLIFETIME(ASFPVPLobbyPlayerState, bReady);
}
void ASFPVPLobbyState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ASFPVPLobbyState, RoomName);
    DOREPLIFETIME(ASFPVPLobbyState, bInternetRoom);
    DOREPLIFETIME(ASFPVPLobbyState, bStarting);
    DOREPLIFETIME(ASFPVPLobbyState, StartError);
}
ASFPVPLobbyPlayerState* ASFPVPLobbyState::GetSeat(int32 Index) const
{
    for (APlayerState* Player : PlayerArray)
        if (auto* P = Cast<ASFPVPLobbyPlayerState>(Player); IsValid(P) && !P->IsInactive() && P->Seat == Index) return P;
    return nullptr;
}
bool ASFPVPLobbyState::AreBothReady() const
{
    return GetSeat(0) && GetSeat(1) && GetSeat(0)->bReady && GetSeat(1)->bReady;
}
void ASFPVPLobbyController::BeginPlay()
{
    Super::BeginPlay();
    if (!IsLocalController()) return;
    GetGameInstance()->GetSubsystem<USFPVPSessionSubsystem>()->LobbyArrived();
    LobbyWidget = CreateWidget<USFPVPLobbyWidget>(this);
    LobbyWidget->AddToViewport();
    bShowMouseCursor = true;
    FInputModeUIOnly Input; Input.SetWidgetToFocus(LobbyWidget->TakeWidget());
    Input.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(Input);
}
void ASFPVPLobbyController::ServerSetRace_Implementation(ESFRace Race)
{
    if(auto* S=GetWorld()->GetGameState<ASFPVPLobbyState>();S&&S->bStarting)return;
    if (Race != ESFRace::Human && Race != ESFRace::Beast) return;
    if (auto* P = GetPlayerState<ASFPVPLobbyPlayerState>())
    {
        if (P->Race != Race) { P->Race = Race; P->bReady = false; P->ForceNetUpdate(); }
    }
}
void ASFPVPLobbyController::EndPlay(const EEndPlayReason::Type Reason)
{
    if(LobbyWidget)LobbyWidget->RemoveFromParent();
    Super::EndPlay(Reason);
}
void ASFPVPLobbyController::ServerSetReady_Implementation(bool bReady)
{
    if(auto* S=GetWorld()->GetGameState<ASFPVPLobbyState>();S&&S->bStarting)return;
    if (GetNetMode() == NM_Standalone) return;
    if (auto* P = GetPlayerState<ASFPVPLobbyPlayerState>())
    {
        P->bReady = bReady; P->ForceNetUpdate();
        UE_LOG(LogTemp, Display, TEXT("[PVP] Seat %d ready=%d race=%d"), P->Seat, P->bReady, int32(P->Race));
    }
}
ASFPVPLobbyMode::ASFPVPLobbyMode()
{
    PlayerControllerClass = ASFPVPLobbyController::StaticClass();
    PlayerStateClass = ASFPVPLobbyPlayerState::StaticClass();
    GameStateClass = ASFPVPLobbyState::StaticClass();
    DefaultPawnClass = nullptr;
    bStartPlayersAsSpectators = true;
    bUseSeamlessTravel = true;
}
void ASFPVPLobbyMode::InitGameState()
{
    Super::InitGameState();
    if (auto* S = GetGameState<ASFPVPLobbyState>())
    {
        S->RoomName = GetGameInstance()->GetSubsystem<USFPVPSessionSubsystem>()->GetRoomName();
        S->bInternetRoom=GetGameInstance()->GetSubsystem<USFPVPSessionSubsystem>()->IsInternetMode();
    }
    if (GameSession) GameSession->MaxPlayers = 2;
}
void ASFPVPLobbyMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
    Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
    if(auto* S=GetGameState<ASFPVPLobbyState>();S&&S->bStarting)ErrorMessage=TEXT("PVP_MATCH_ALREADY_STARTED");
    if (!ErrorMessage.IsEmpty()) return;
    if (UGameplayStatics::ParseOption(Options,TEXT("SFProtocol"))!=USFPVPSessionSubsystem::ProtocolVersion)ErrorMessage=TEXT("PVP_VERSION_MISMATCH");
    if(const auto* S=GetGameState<ASFPVPLobbyState>())
    {
        const bool bOnline=UGameplayStatics::ParseOption(Options,TEXT("SFOnline"))==TEXT("1");
        if(bOnline!=S->bInternetRoom||(S->bInternetRoom&&(!UniqueId.IsValid()||UniqueId.GetType()!=FName(TEXT("STEAM")))))
            ErrorMessage=TEXT("PVP_CONNECTION_MODE_MISMATCH");
    }
    if (GetNumPlayers() + GetNumSpectators() >= 2) ErrorMessage = TEXT("PVP_ROOM_FULL");
}

void ASFPVPLobbyController::ServerStartBattle_Implementation()
{
    auto* S=GetWorld()->GetGameState<ASFPVPLobbyState>();
    const auto* P=GetPlayerState<ASFPVPLobbyPlayerState>();
    if(!P||P->Seat!=0||!S||!S->AreBothReady()||S->bStarting)return;
    TArray<FString> Maps;
    // Explicit PVP derivatives of the seven numbered campaign battlefields. Never the title/start map.
    for(int32 I=1;I<=7;++I)
    {
        const FString Map=FString::Printf(TEXT("/Game/PVP/Maps/PVP_Stage%02d"),I);
        if(FPackageName::DoesPackageExist(Map))Maps.Add(Map);
    }
    if(Maps.IsEmpty()){S->StartError=TEXT("사용 가능한 PVP 전장이 없습니다.");return;}
    const FString Map=Maps[FMath::RandRange(0,Maps.Num()-1)];
    S->bStarting=true;S->StartError.Empty();S->ForceNetUpdate();
    GetGameInstance()->GetSubsystem<USFPVPSessionSubsystem>()->CloseJoining();
    UE_LOG(LogTemp,Display,TEXT("[PVP] Starting random battlefield: %s"),*Map);
    if(!GetWorld()->ServerTravel(Map+TEXT("?game=/Script/StrikeForge.SFPVPBattleMode"),true))
    {S->bStarting=false;S->StartError=TEXT("전장 이동에 실패했습니다. 다시 시도해 주세요.");}
}
void ASFPVPLobbyMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);
    auto* S = GetGameState<ASFPVPLobbyState>();
    auto* P = NewPlayer->GetPlayerState<ASFPVPLobbyPlayerState>();
    if (!S || !P) return;
    const int32 Seat = !S->GetSeat(0) ? 0 : (!S->GetSeat(1) ? 1 : INDEX_NONE);
    // Also enforce capacity here: simultaneous logins may both pass PreLogin.
    if (Seat == INDEX_NONE) { GameSession->KickPlayer(NewPlayer, FText::FromString(TEXT("PVP_ROOM_FULL"))); return; }
    P->Seat = Seat; P->SetPlayerName(Seat == 0 ? TEXT("방장") : TEXT("도전자"));
    P->ForceNetUpdate();
    GetGameInstance()->GetSubsystem<USFPVPSessionSubsystem>()->RegisterRoomPlayer(NewPlayer);
    UE_LOG(LogTemp, Display, TEXT("[PVP] Joined seat=%d netmode=%d"), Seat, int32(GetNetMode()));
}
void ASFPVPLobbyMode::Logout(AController* Exiting)
{
    GetGameInstance()->GetSubsystem<USFPVPSessionSubsystem>()->UnregisterRoomPlayer(Exiting);
    if (auto* P = Exiting->GetPlayerState<ASFPVPLobbyPlayerState>()) P->Seat = INDEX_NONE;
    Super::Logout(Exiting);
    if (auto* S = GetGameState<ASFPVPLobbyState>())
        for (APlayerState* Player : S->PlayerArray)
            if (auto* P = Cast<ASFPVPLobbyPlayerState>(Player)) { P->bReady = false; P->ForceNetUpdate(); }
    UE_LOG(LogTemp, Display, TEXT("[PVP] Player left; readiness reset"));
}
