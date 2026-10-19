#include "SFPVPBattle.h"
#include "SFPVPSessionSubsystem.h"
#include "Game/Subsystem/SFFogOfWarSubsystem.h"
#include "ActorComponent/Combat/CombatTargetComponent.h"
#include "ActorComponent/Stat/HealthComponent.h"
#include "Net/UnrealNetwork.h"
#include "EngineUtils.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "NavigationSystem.h"
#include "NavigationData.h"
#include "NavigationPath.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/GameSession.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"
#include "TimerManager.h"
#include "Engine/GameViewportClient.h"
#include "UnrealClient.h"
#include "Misc/CommandLine.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"

void ASFPVPBattleState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ASFPVPBattleState,bBattleReady); DOREPLIFETIME(ASFPVPBattleState,BattleStatus);
    DOREPLIFETIME(ASFPVPBattleState,Base0); DOREPLIFETIME(ASFPVPBattleState,Base1);
}
ASFPVPBattleMode::ASFPVPBattleMode()
{
    PlayerControllerClass=ASFPVPBattleController::StaticClass();
    PlayerStateClass=ASFPVPLobbyPlayerState::StaticClass();
    GameStateClass=ASFPVPBattleState::StaticClass();
    HUDClass=ASFPVPBattleHUD::StaticClass(); DefaultPawnClass=nullptr;
    bUseSeamlessTravel=true;
    // Do not load four combat Blueprint dependency trees while opening the title or lobby.
    StartingUnits={TSoftClassPtr<AUnitBase>(FSoftObjectPath(TEXT("/Game/Core/Units/Etherial_Human/BP_Human_Fighter.BP_Human_Fighter_C"))),
        TSoftClassPtr<AUnitBase>(FSoftObjectPath(TEXT("/Game/Core/Units/Etherial_Human/BP_Human_Shooter.BP_Human_Shooter_C"))),
        TSoftClassPtr<AUnitBase>(FSoftObjectPath(TEXT("/Game/Core/Units/Beast_Fox/BP_Fox_Brawler.BP_Fox_Brawler_C"))),
        TSoftClassPtr<AUnitBase>(FSoftObjectPath(TEXT("/Game/Core/Units/Beast_Fox/BP_Fox_Hunter.BP_Fox_Hunter_C")))};
}
void ASFPVPBattleMode::BeginPlay()
{
    Super::BeginPlay(); WaitStarted=GetWorld()->GetTimeSeconds();
    GetWorldTimerManager().SetTimer(SpawnTimer,this,&ThisClass::TrySpawnArmies,.5f,true);
}
void ASFPVPBattleMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
    // Seamless travel preserves the server-assigned player state and creates the battle controller.
    Super::HandleStartingNewPlayer_Implementation(NewPlayer);
}
void ASFPVPBattleMode::PreLogin(const FString& Options,const FString& Address,const FUniqueNetIdRepl& UniqueId,FString& Error)
{ Error=TEXT("PVP_MATCH_ALREADY_STARTED"); }
bool ASFPVPBattleMode::FindBattleGround(FVector& A,FVector& B) const
{
    auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    if(!Nav || Nav->IsNavigationBuildInProgress())return false;
    FBox Bounds(ForceInit);for(TActorIterator<ANavMeshBoundsVolume> It(GetWorld());It;++It)Bounds+=It->GetComponentsBoundingBox(true);
    if(!Bounds.IsValid)return false;
    // The geometric center can sit on a small, isolated navigation island.
    // Try other islands as well, and require room for all six starting units.
    auto ClearFormation=[Nav](const FVector& Center)
    {
        for(int32 I=0;I<6;++I)
        {
            const FVector Desired=Center+FVector((I/3)*180.f,(I%3-1)*180.f,0);
            FNavLocation Slot;
            if(!Nav->ProjectPointToNavigation(Desired,Slot,FVector(80,80,200))||FVector::Dist2D(Desired,Slot.Location)>80)return false;
        }
        return true;
    };
    for(int32 Attempt=0;Attempt<24;++Attempt)
    {
        FNavLocation First;
        const bool bFound=Attempt==0?Nav->ProjectPointToNavigation(Bounds.GetCenter(),First,Bounds.GetExtent()):Nav->GetRandomPoint(First);
        if(!bFound||!ClearFormation(First.Location))continue;
        A=First.Location;
        for(int32 I=0;I<80;++I)
        {
            FNavLocation Candidate;
            if(!Nav->GetRandomReachablePointInRadius(A,5000.f,Candidate))continue;
            const float Distance=FVector::Dist2D(A,Candidate.Location);
            if(Distance<2500.f||Distance>4500.f||!ClearFormation(Candidate.Location))continue;
            auto* Path=UNavigationSystemV1::FindPathToLocationSynchronously(GetWorld(),A,Candidate.Location);
            if(!Path||!Path->IsValid()||Path->IsPartial()||Path->GetPathLength()>6500.f)continue;
            B=Candidate.Location;return true;
        }
    }
    return false;
}
void ASFPVPBattleMode::TrySpawnArmies()
{
    auto* S=GetGameState<ASFPVPBattleState>();if(!S||S->bBattleReady)return;
    int32 Connected=0;for(FConstPlayerControllerIterator It=GetWorld()->GetPlayerControllerIterator();It;++It)
        if(auto* PC=Cast<ASFPVPBattleController>(It->Get());PC && PC->HasClientLoadedCurrentWorld())++Connected;
    if(Connected!=2 || !S->GetSeat(0)||!S->GetSeat(1))return;
    if(!StartingUnitLoad)
    {
        TArray<FSoftObjectPath> Required;
        for(int32 Team=0;Team<2;++Team)
        {const int32 Offset=S->GetSeat(Team)->Race==ESFRace::Beast?2:0;Required.AddUnique(StartingUnits[Offset].ToSoftObjectPath());Required.AddUnique(StartingUnits[Offset+1].ToSoftObjectPath());}
        StartingUnitLoad=UAssetManager::GetStreamableManager().RequestAsyncLoad(Required);
        S->BattleStatus=TEXT("선택한 종족의 병력을 불러오고 있습니다.");S->ForceNetUpdate();
        if(!StartingUnitLoad){S->BattleStatus=TEXT("병력 데이터를 불러오지 못했습니다. Esc로 로비에 돌아가 주세요.");GetWorldTimerManager().ClearTimer(SpawnTimer);}
        return;
    }
    if(!StartingUnitLoad->HasLoadCompleted())return;
    FVector A,B;if(!FindBattleGround(A,B))
    {
        S->BattleStatus=TEXT("전장의 이동 경로를 준비하고 있습니다.");
        if(GetWorld()->GetTimeSeconds()-WaitStarted>60){S->BattleStatus=TEXT("시작 위치를 준비하지 못했습니다. Esc로 로비에 돌아가 주세요.");GetWorldTimerManager().ClearTimer(SpawnTimer);}
        return;
    }
    auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    TArray<AUnitBase*> Spawned;
    for(int32 Team=0;Team<2;++Team)
    {
        const auto* P=S->GetSeat(Team);const int32 Offset=P->Race==ESFRace::Beast?2:0;
        for(int32 I=0;I<6;++I)
        {
            const FVector Center=Team==0?A:B;FNavLocation Position;
            FVector Desired=Center+FVector((I/3)*180.f,(I%3-1)*180.f,0);
            if(!Nav->ProjectPointToNavigation(Desired,Position,FVector(200,200,300)))Position.Location=Center;
            FTransform Transform((Team==0?B-A:A-B).Rotation(),Position.Location+FVector(0,0,100));
            auto* Unit=GetWorld()->SpawnActorDeferred<AUnitBase>(StartingUnits[Offset+(I>=3?1:0)].Get(),Transform,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
            if(!Unit)continue;
            Unit->SetReplicates(true);Unit->SetReplicateMovement(true);Unit->bAlwaysRelevant=true;Unit->SetNetUpdateFrequency(30.f);
            Unit->PVPUnitId=Team*100+I;
            Unit->FindComponentByClass<UCombatTargetComponent>()->SetTeamIndex(Team);
            Unit->SetAdvanceMode(EUnitAdvanceMode::HoldPosition);
            Unit->FinishSpawning(Transform);Spawned.Add(Unit);
        }
    }
    if(Spawned.Num()!=12)
    {
        for(auto* Unit:Spawned)Unit->Destroy();
        S->BattleStatus=TEXT("시작 병력을 불러오지 못했습니다. Esc로 돌아가 주세요.");
    }
    else {S->Base0=A;S->Base1=B;S->bBattleReady=true;S->BattleStatus=TEXT("병력을 선택하고 상대 진영과 교전하세요.");}
    S->ForceNetUpdate();GetWorldTimerManager().ClearTimer(SpawnTimer);
    UE_LOG(LogTemp,Display,TEXT("[PVPBattle] Armies=%d bases=%s / %s"),Spawned.Num(),*A.ToString(),*B.ToString());
}
void ASFPVPBattleMode::Logout(AController* Exiting)
{
    GetGameInstance()->GetSubsystem<USFPVPSessionSubsystem>()->UnregisterRoomPlayer(Exiting);
    Super::Logout(Exiting);
    if(auto* S=GetGameState<ASFPVPBattleState>())
    {
        S->bBattleReady=false;S->BattleStatus=TEXT("상대가 전장을 떠났습니다. Esc로 로비에 돌아가세요.");S->ForceNetUpdate();
        GetWorldTimerManager().ClearTimer(SpawnTimer);
        for(TActorIterator<AUnitBase> It(GetWorld());It;++It)if(It->PVPUnitId!=INDEX_NONE){It->ExecuteCommand(ESFUnitCommand::Stop,It->GetActorLocation(),nullptr);It->SetActorTickEnabled(false);}
    }
}
int32 ASFPVPBattleController::GetBattleTeam() const
{const auto* P=GetPlayerState<ASFPVPLobbyPlayerState>();return P?P->Seat:INDEX_NONE;}
ASFPVPBattleController::ASFPVPBattleController()
{
    bAutoManageActiveCameraTarget=false;
}
void ASFPVPBattleController::BeginPlay()
{
    APlayerController::BeginPlay();
    if(!IsLocalController())return;
    bShowMouseCursor=true;SetInputMode(FInputModeGameAndUI().SetHideCursorDuringCapture(false).SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock));
}
void ASFPVPBattleController::EndPlay(const EEndPlayReason::Type Reason)
{if(BattleCamera)BattleCamera->Destroy();Selected.Empty();APlayerController::EndPlay(Reason);}
void ASFPVPBattleController::SetupInputComponent()
{
    APlayerController::SetupInputComponent();
    InputComponent->BindKey(EKeys::LeftMouseButton,IE_Pressed,this,&ThisClass::SelectPressed);
    InputComponent->BindKey(EKeys::LeftMouseButton,IE_Released,this,&ThisClass::SelectReleased);
    InputComponent->BindKey(EKeys::RightMouseButton,IE_Pressed,this,&ThisClass::RightClick);
    InputComponent->BindKey(EKeys::A,IE_Pressed,this,&ThisClass::ArmAttack);
    InputComponent->BindKey(EKeys::S,IE_Pressed,this,&ThisClass::Stop);
    InputComponent->BindKey(EKeys::Tab,IE_Pressed,this,&ThisClass::SelectAll);
    InputComponent->BindKey(EKeys::SpaceBar,IE_Pressed,this,&ThisClass::FocusArmy);
    InputComponent->BindKey(EKeys::Escape,IE_Pressed,this,&ThisClass::LeaveBattle);
    InputComponent->BindKey(EKeys::MouseScrollUp,IE_Pressed,this,&ThisClass::ZoomIn);
    InputComponent->BindKey(EKeys::MouseScrollDown,IE_Pressed,this,&ThisClass::ZoomOut);
}
void ASFPVPBattleController::PlayerTick(float DeltaTime)
{
    APlayerController::PlayerTick(DeltaTime);if(!IsLocalController())return;
    if(!IsValid(BattleCamera)||BattleCamera->GetWorld()!=GetWorld())
    {
        BattleCamera=GetWorld()->SpawnActor<ACameraActor>();
        if(!BattleCamera)return;
        BattleCamera->GetCameraComponent()->SetFieldOfView(55);
        Selected.Empty();bFocused=false;
    }
    SetMyTeamIndex(GetBattleTeam());
    const auto* S=GetWorld()->GetGameState<ASFPVPBattleState>();
    if(!bFocused&&S&&S->bBattleReady&&GetBattleTeam()>=0){FocusArmy();bFocused=true;}
    FVector Pan=FVector::ZeroVector;
    if(IsInputKeyDown(EKeys::Up))Pan.X+=1;if(IsInputKeyDown(EKeys::Down))Pan.X-=1;
    if(IsInputKeyDown(EKeys::Right))Pan.Y+=1;if(IsInputKeyDown(EKeys::Left))Pan.Y-=1;
    float X,Y;int32 W,H;GetViewportSize(W,H);
    const auto* Viewport=GetWorld()->GetGameViewport();
    const bool bCanEdgePan=Viewport&&Viewport->Viewport&&Viewport->Viewport->HasFocus()
        &&!FParse::Param(FCommandLine::Get(),TEXT("RenderOffscreen"));
    if(bCanEdgePan&&GetMousePosition(X,Y)&&X>=0&&Y>=0&&X<W&&Y<H)
    {if(X<12)Pan.Y-=1;if(X>W-12)Pan.Y+=1;if(Y<12)Pan.X+=1;if(Y>H-12)Pan.X-=1;}
    CameraFocus+=Pan.GetClampedToMaxSize(1.f)*2200.f*DeltaTime;
    BattleCamera->SetActorLocation(CameraFocus+FVector(-CameraHeight*.7f,0,CameraHeight));
    BattleCamera->SetActorRotation((CameraFocus-BattleCamera->GetActorLocation()).Rotation());
    if(GetViewTarget()!=BattleCamera)SetViewTarget(BattleCamera);
    Selected.RemoveAll([](auto U){return !U.IsValid()||U->GetHealthComponent()->GetIsDead();});
}
void ASFPVPBattleController::FocusArmy()
{
    if(const auto* S=GetWorld()->GetGameState<ASFPVPBattleState>())CameraFocus=GetBattleTeam()==0?S->Base0:S->Base1;
    if(!Selected.IsEmpty()&&Selected[0].IsValid())CameraFocus=Selected[0]->GetActorLocation();
}
void ASFPVPBattleController::ZoomIn(){CameraHeight=FMath::Max(900.f,CameraHeight-150.f);}
void ASFPVPBattleController::ZoomOut(){CameraHeight=FMath::Min(3500.f,CameraHeight+150.f);}
void ASFPVPBattleController::SelectAll()
{
    Selected.Empty();for(TActorIterator<AUnitBase> It(GetWorld());It;++It)
        if(It->PVPUnitId!=INDEX_NONE&&It->GetTeamIndex()==GetBattleTeam()&&!It->GetHealthComponent()->GetIsDead())Selected.Add(*It);
}
void ASFPVPBattleController::SelectPressed()
{
    if(bAttackPending)
    {
        FHitResult Hit;if(GetHitResultUnderCursor(ECC_Visibility,false,Hit))
            CommandSelected(ESFUnitCommand::Attack,Hit.Location,Cast<AUnitBase>(Hit.GetActor()));
        bAttackPending=false;return;
    }
    bDragging=GetMousePosition(DragStart.X,DragStart.Y);
}
void ASFPVPBattleController::SelectReleased()
{
    if(!bDragging)return;bDragging=false;FVector2D End;GetMousePosition(End.X,End.Y);
    if(!IsInputKeyDown(EKeys::LeftShift))Selected.Empty();
    if(FVector2D::Distance(End,DragStart)<8.f)
    {
        FHitResult Hit;if(GetHitResultUnderCursor(ECC_Visibility,false,Hit))
            if(auto* U=Cast<AUnitBase>(Hit.GetActor());U&&U->GetTeamIndex()==GetBattleTeam()&&!U->GetHealthComponent()->GetIsDead())Selected.AddUnique(U);
    }
    else for(TActorIterator<AUnitBase> It(GetWorld());It;++It)
    {
        FVector2D P;if(It->PVPUnitId==INDEX_NONE||It->GetTeamIndex()!=GetBattleTeam()||It->GetHealthComponent()->GetIsDead()||!ProjectWorldLocationToScreen(It->GetActorLocation(),P))continue;
        if(P.X>=FMath::Min(End.X,DragStart.X)&&P.X<=FMath::Max(End.X,DragStart.X)&&P.Y>=FMath::Min(End.Y,DragStart.Y)&&P.Y<=FMath::Max(End.Y,DragStart.Y))Selected.AddUnique(*It);
    }
    if(!Selected.IsEmpty())PlaySelectionSound(Selected[0].Get());
}
void ASFPVPBattleController::RightClick()
{
    bAttackPending=false;FHitResult Hit;if(!GetHitResultUnderCursor(ECC_Visibility,false,Hit))return;
    auto* Target=Cast<AUnitBase>(Hit.GetActor());
    CommandSelected(Target&&Target->GetTeamIndex()!=GetBattleTeam()?ESFUnitCommand::Attack:ESFUnitCommand::Move,Hit.Location,Target);
}
void ASFPVPBattleController::ArmAttack(){bAttackPending=true;}
void ASFPVPBattleController::Stop(){CommandSelected(ESFUnitCommand::Stop,FVector::ZeroVector);}
void ASFPVPBattleController::LeaveBattle(){GetGameInstance()->GetSubsystem<USFPVPSessionSubsystem>()->Leave(false);}
void ASFPVPBattleController::CommandSelected(ESFUnitCommand Command,FVector Location,AUnitBase* Target)
{
    TArray<AUnitBase*> Units;for(auto U:Selected)if(U.IsValid())Units.Add(U.Get());
    ServerCommand(Units,Command,Location,Target);
}
void ASFPVPBattleController::ServerCommand_Implementation(const TArray<AUnitBase*>& Units,ESFUnitCommand Command,FVector_NetQuantize Location,AUnitBase* Target)
{
    const auto* S=GetWorld()->GetGameState<ASFPVPBattleState>();
    if(!S||!S->bBattleReady||Units.Num()>100||Location.ContainsNaN()||Location.SizeSquared()>FMath::Square(1000000.f))return;
    if(Command!=ESFUnitCommand::Move&&Command!=ESFUnitCommand::Attack&&Command!=ESFUnitCommand::AttackMove&&Command!=ESFUnitCommand::Stop)return;
    const double Now=GetWorld()->GetTimeSeconds();if(Now-LastCommandTime<.04)return;LastCommandTime=Now;
    if(Target&&(!IsValid(Target)||Target->PVPUnitId==INDEX_NONE||Target->GetTeamIndex()==GetBattleTeam()||Target->GetHealthComponent()->GetIsDead()))return;
    if(Target)if(const auto* Fog=USFFogOfWarSubsystem::Get(this);Fog&&!Fog->IsLocationVisible(GetBattleTeam(),Target->GetActorLocation()))return;
    auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());FNavLocation Projected;
    if(Command!=ESFUnitCommand::Stop&&!Target&&(!Nav||!Nav->ProjectPointToNavigation(Location,Projected,FVector(250,250,500))))return;
    int32 Index=0;TSet<AUnitBase*> Seen;
    for(auto* Unit:Units)
    {
        if(!IsValid(Unit)||Seen.Contains(Unit)||Unit->PVPUnitId==INDEX_NONE||Unit->GetTeamIndex()!=GetBattleTeam()||Unit->GetHealthComponent()->GetIsDead())continue;
        Seen.Add(Unit);FVector Destination=Target?Target->GetActorLocation():Projected.Location;
        if(!Target&&Command!=ESFUnitCommand::Stop)
        {
            FNavLocation Slot;const FVector Formation=Destination+FVector((Index/3)*130,(Index%3-1)*130,0);
            if(Nav->ProjectPointToNavigation(Formation,Slot,FVector(150,150,300)))Destination=Slot.Location;
        }
        Unit->ExecuteCommand(Command,Destination,Target);++Index;
    }
    UE_LOG(LogTemp,Display,TEXT("[PVPBattle] Command seat=%d accepted=%d type=%d"),GetBattleTeam(),Index,int32(Command));
}
ASFPVPBattleHUD::ASFPVPBattleHUD()
{
    static ConstructorHelpers::FObjectFinder<UFont> F(TEXT("/Game/Game/IN_UI/Font/Jua-Regular_Font.Jua-Regular_Font"));UIFont=F.Object;
}
void ASFPVPBattleHUD::DrawHUD()
{
    Super::DrawHUD();auto* PC=Cast<ASFPVPBattleController>(PlayerOwner);if(!PC||!Canvas)return;
    const float Scale=Canvas->SizeX/1600.f;const auto* S=GetWorld()->GetGameState<ASFPVPBattleState>();
    DrawRect(FLinearColor(.015,.03,.03,.92),0,Canvas->SizeY-100*Scale,Canvas->SizeX,100*Scale);
    DrawRect(FLinearColor(.64,.44,.22),0,Canvas->SizeY-102*Scale,Canvas->SizeX,2*Scale);
    const FString Heading=FString::Printf(TEXT("PVP  |  %s  |  선택 %d"),PC->GetBattleTeam()==0?TEXT("방장"):TEXT("도전자"),PC->GetSelection().Num());
    DrawText(Heading,FLinearColor(.3,.9,.85),24*Scale,Canvas->SizeY-86*Scale,UIFont,2.f*Scale);
    DrawText(TEXT("클릭/드래그 선택   우클릭 이동/공격   A 공격 이동   S 정지   Tab 전체 선택   Space 병력 보기   방향키/화면 가장자리 카메라   Esc 나가기"),FLinearColor(.94,.87,.70),24*Scale,Canvas->SizeY-45*Scale,UIFont,1.3f*Scale);
    if(S)DrawText(S->BattleStatus,FLinearColor::White,24*Scale,20*Scale,UIFont,1.4f*Scale);
    for(TActorIterator<AUnitBase> It(GetWorld());It;++It)
    {
        if(It->PVPUnitId==INDEX_NONE||It->GetHealthComponent()->GetIsDead())continue;
        if(auto* Fog=USFFogOfWarSubsystem::Get(this);Fog&&!Fog->IsActorVisibleToLocalPlayer(*It))continue;
        FVector2D P;if(!PC->ProjectWorldLocationToScreen(It->GetActorLocation()+FVector(0,0,160),P))continue;
        const auto Color=It->GetTeamIndex()==PC->GetBattleTeam()?FLinearColor(.1,.7,1):FLinearColor(1,.18,.12);
        DrawRect(FLinearColor::Black,P.X-23,P.Y,46,6);DrawRect(Color,P.X-22,P.Y+1,44*It->GetHealthComponent()->GetHealthPercent(),4);
        if(PC->GetSelection().Contains(*It))
        {
            FVector2D Prev=FVector2D::ZeroVector;for(int32 I=0;I<=32;++I)
            {const float Angle=I*2*PI/32;FVector2D Q;PC->ProjectWorldLocationToScreen(It->GetActorLocation()+FVector(FMath::Cos(Angle)*65,FMath::Sin(Angle)*65,-85),Q);if(I)DrawLine(Prev.X,Prev.Y,Q.X,Q.Y,Color,2);Prev=Q;}
        }
    }
    if(PC->IsDragging())
    {FVector2D A=PC->GetDragStart(),B;PC->GetMousePosition(B.X,B.Y);const FLinearColor C(.2,.8,1);DrawLine(A.X,A.Y,B.X,A.Y,C);DrawLine(B.X,A.Y,B.X,B.Y,C);DrawLine(B.X,B.Y,A.X,B.Y,C);DrawLine(A.X,B.Y,A.X,A.Y,C);}
}
