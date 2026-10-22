#if WITH_DEV_AUTOMATION_TESTS
#include "SFPVPPresentationQA.h"
#include "Game/Audio/SFBattleMusicComponent.h"
#include "Sound/SoundCue.h"
#include "Sound/SoundNodeWavePlayer.h"
#include "Sound/SoundClass.h"
#include "EngineUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/App.h"
#include "ActorComponent/Buff/SFBuffComponent.h"
#include "ActorComponent/Combat/CombatTargetComponent.h"
#include "Units/Commander/SFCommander.h"
#include "Units/Commander/SFCommanderAnim.h"
#include "Units/Unit/UnitBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/AudioComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NiagaraComponent.h"
#include "Game/Audio/SFUnitVoiceSubsystem.h"
#include "Game/Subsystem/SFBuildingSynergySubsystem.h"
#include "Game/Multiplayer/SFPVPBattle.h"
#include "Game/GameState/SFTeamState.h"
#include "Buildings/Building/HouseBuilding/Nexus/SFNexusBuilding.h"
#include "Datas/Building/SFBuildingDefinition.h"
#include "InGame/SFInGameHUDWidget.h"
#include "InGame/SFSelectINfo/SFSelectionPanelWidget.h"
#include "InGame/HUD/Minimap/SFMinimapPanelWidget.h"
#include "InGame/Controller/SFRTSCameraPawn.h"
#include "Game/Subsystem/SFCombatSubsystem.h"
#include "Game/Subsystem/SFFogOfWarSubsystem.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "ImageUtils.h"
#include "Serialization/BufferArchive.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanelSlot.h"
#include "NavigationSystem.h"

struct FSFPVPTeamAudit
{
    static void Write(UWorld* W,const FString& Instance,bool bPixels=false)
    {
        auto* Combat=USFCombatSubsystem::Get(W);auto* PC=Combat?Combat->GetLocalPlayerController():nullptr;
        auto* State=W->GetGameState<ASFPVPBattleState>();if(!PC||!State)return;
        const int32 LocalTeam=Combat->GetLocalPlayerTeam();auto* HUD=PC->GetInGameHUDWidget();
        FString Text=FString::Printf(TEXT("local_team=%d first_is_local=%d winner=%d result=%d\n"),LocalTeam,W->GetFirstPlayerController()->IsLocalController(),State->WinnerTeam,int32(State->GetStageResult()));
        if(auto* Camera=PC->GetPawn())if(auto* Arm=Camera->FindComponentByClass<USpringArmComponent>())
            Text+=FString::Printf(TEXT("camera_arm=%.2f\n"),Arm->TargetArmLength);
        if(auto* Camera=PC->GetPawn())if(auto* View=Camera->FindComponentByClass<UCameraComponent>())
            Text+=FString::Printf(TEXT("camera_blendables=%d\n"),View->PostProcessSettings.WeightedBlendables.Array.Num());
        if(HUD)
        {
            auto* P=HUD->SelectionPanel.Get();
            Text+=FString::Printf(TEXT("selection=%s type=%d panel_has=%d panel_type=%d panel_name=%s\n"),*GetNameSafe(HUD->SelectedActor.Get()),int32(HUD->SelectedInfo.SelectableType),P&&P->bHasSelection,P?int32(P->CurrentSelectableInfo.SelectableType):-1,P?*P->CurrentSelectableInfo.DisplayName.ToString():TEXT("none"));
            if(auto* Mini=Cast<USFMinimapPanelWidget>(HUD->MinimapPanel))
            {
                Text+=FString::Printf(TEXT("minimap_capture=%d markers=%d\n"),Mini->RuntimeMinimapCaptureActor!=nullptr,Mini->MinimapMarkerEntries.Num());
                for(const auto& E:Mini->MinimapMarkerEntries)if(E.TargetActor.IsValid()&&IsValid(E.MarkerWidget))
                {
                    auto* Target=USFCombatSubsystem::FindTargetComponent(E.TargetActor.Get());
                    const auto C=E.MarkerWidget->GetBrushColor();
                    Text+=FString::Printf(TEXT("marker,%s,%d,%d,%.3f,%.3f,%.3f,%d\n"),*E.TargetActor->GetName(),Target?Target->GetTeamIndex():-1,int32(E.MarkerType),C.R,C.G,C.B,int32(E.MarkerWidget->GetVisibility()));
                }
                if(bPixels&&Mini->MinimapRenderTarget)
                {
                    TArray<UWidget*> Widgets;Mini->WidgetTree->GetAllWidgets(Widgets);
                    for(auto* Widget:Widgets)
                    {
                        const auto* Slot=Cast<UCanvasPanelSlot>(Widget->Slot);
                        const auto* Image=Cast<UImage>(Widget);
                        Text+=FString::Printf(TEXT("widget,%s,parent=%s,z=%d,visible=%d,resource=%s\n"),*Widget->GetName(),*GetNameSafe(Widget->GetParent()),Slot?Slot->GetZOrder():-1,int32(Widget->GetVisibility()),Image?*GetNameSafe(Image->GetBrush().GetResourceObject()):TEXT("none"));
                    }
                    TArray<FColor> Pixels;Mini->MinimapRenderTarget->GameThread_GetRenderTargetResource()->ReadPixels(Pixels);
                    int32 Lit=0;for(const auto& C:Pixels)Lit+=C.R>8||C.G>8||C.B>8;
                    FFileHelper::SaveStringToFile(FString::Printf(TEXT("lit=%d total=%d"),Lit,Pixels.Num()),*(FPaths::ProjectSavedDir()/TEXT("PVPQA")/(Instance+TEXT(".minimap_pixels"))));
                    FBufferArchive Buffer;FImageUtils::ExportRenderTarget2DAsPNG(Mini->MinimapRenderTarget,Buffer);
                    FFileHelper::SaveArrayToFile(Buffer,*(FPaths::ProjectSavedDir()/TEXT("PVPQA")/(Instance+TEXT(".minimap.png"))));
                }
            }
        }
        if(auto* Fog=USFFogOfWarSubsystem::Get(W))for(int32 Team=0;Team<State->Nexuses.Num();++Team)if(auto* N=State->Nexuses[Team].Get())
            Text+=FString::Printf(TEXT("nexus,%d,visible=%d,team_vision=%d,type=%d\n"),Team,Fog->IsActorVisibleToLocalPlayer(N),Fog->IsLocationVisible(LocalTeam,N->GetActorLocation()),int32(ISFSelectable::Execute_GetSelectableInfo(N).SelectableType));
        FFileHelper::SaveStringToFile(Text,*(FPaths::ProjectSavedDir()/TEXT("PVPQA")/(Instance+(bPixels?TEXT(".view_audit"):TEXT(".view")))),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    }
};

// Only reachable from the explicitly enabled development mailbox, never from gameplay keys.
bool SFPresentationQACommand(UWorld* W,const FString& Op,const FString& Value)
{
    if(Op==TEXT("music_audit"))
    {
        FString Report;
        for(FConstPlayerControllerIterator It=W->GetPlayerControllerIterator();It;++It)
            if(auto* PC=Cast<ASFPVPBattleController>(It->Get()))
            {
                auto* Music=PC->FindComponentByClass<USFBattleMusicComponent>();
                if(!Music)Music=FindObject<USFBattleMusicComponent>(PC,TEXT("BattleMusic"));
                if(!Music){Report+=FString::Printf(TEXT("local=%d team=%d playing=0 component=none\n"),PC->IsLocalController(),PC->GetBattleTeam());continue;}
                auto* Cue=Cast<USoundCue>(Music->Sound);
                const auto* Node=Cue?Cast<USoundNodeWavePlayer>(Cue->FirstNode):nullptr;
                Report+=FString::Printf(TEXT("local=%d team=%d playing=%d track=%s class=%s loop=%d ui=%d spatial=%d duration=%.2f\n"),
                    PC->IsLocalController(),PC->GetBattleTeam(),Music->IsPlaying(),*GetPathNameSafe(Node?Node->GetSoundWave():nullptr),
                    *GetPathNameSafe(Cue?Cue->GetSoundClass():nullptr),Cue&&Cue->IsLooping(),Music->bIsUISound,Music->bAllowSpatialization,Cue?Cue->GetDuration():0);
            }
        FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectSavedDir()/TEXT("PVPQA")/(FPaths::GetCleanFilename(Value)+TEXT(".music"))),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
        return true;
    }
    if(Op==TEXT("music_next"))
    {
        if(auto* PC=USFCombatSubsystem::Get(W)->GetLocalPlayerController())
            if(auto* Music=PC->FindComponentByClass<USFBattleMusicComponent>();Music&&Music->IsPlaying()&&Music->Sound)
                Music->Play(FMath::Max(0.f,Music->Sound->GetDuration()-.25f));
        return true;
    }
    if(Op==TEXT("audio_focus")){FApp::SetUnfocusedVolumeMultiplier(1.f);return true;}
    if(Op==TEXT("viewaudit")){FSFPVPTeamAudit::Write(W,Value,true);return true;}
    auto* State=W->GetGameState<ASFPVPBattleState>();
    if(Op==TEXT("remote_first")&&W->GetAuthGameMode<ASFPVPBattleMode>())
    {
        if(auto* Local=USFCombatSubsystem::Get(W)->GetLocalPlayerController()) { W->RemoveController(Local);W->AddController(Local); }
        return true;
    }
    if(Op==TEXT("select_actor")&&State)
    {
        FString Kind,TeamText;Value.Split(TEXT(" "),&Kind,&TeamText);const int32 Team=FCString::Atoi(*TeamText);
        auto* PC=USFCombatSubsystem::Get(W)->GetLocalPlayerController();
        AActor* Actor=Kind==TEXT("nexus")&&State->Nexuses.IsValidIndex(Team)?static_cast<AActor*>(State->Nexuses[Team].Get()):
            Kind==TEXT("commander")&&State->Commanders.IsValidIndex(Team)?static_cast<AActor*>(State->Commanders[Team].Get()):nullptr;
        if(PC&&Actor&&PC->GetInGameHUDWidget())PC->GetInGameHUDWidget()->SelectActor(Actor);
        return true;
    }
    if(!State||!W->GetAuthGameMode<ASFPVPBattleMode>())return false;
    if(Op==TEXT("reveal_test")&&State->Commanders.IsValidIndex(0)&&State->Nexuses.IsValidIndex(1))
    {
        if(auto* Commander=State->Commanders[0].Get())if(auto* Nexus=State->Nexuses[1].Get())
        {
            FNavLocation Ground;auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(W);
            if(Nav&&Nav->GetRandomReachablePointInRadius(Nexus->GetActorLocation(),1200,Ground))
                Commander->TeleportTo(Ground.Location+FVector(0,0,90),Commander->GetActorRotation());
        }
        return true;
    }
    if(Op==TEXT("resources"))
    {
        TArray<FString> P;Value.ParseIntoArrayWS(P);
        if(P.Num()==3)if(auto* Team=State->GetTeamState(FCString::Atoi(*P[0])))
        {Team->AddResource(EResourceType::Mineral,FCString::Atoi(*P[1]));Team->AddResource(EResourceType::Ether,FCString::Atoi(*P[2]));}
        return true;
    }
    if(Op==TEXT("upgrade_nexus"))
    {
        const int32 Team=FCString::Atoi(*Value);
        if(State->Nexuses.IsValidIndex(Team))if(auto* N=State->Nexuses[Team].Get())N->UpgradeBuilding();
        return true;
    }
    return false;
}

void SFWritePresentationQA(UWorld* W,const FString& Instance)
{
    FSFPVPTeamAudit::Write(W,Instance);
    FString S;
    for(TActorIterator<ASFCommander> It(W);It;++It)
    {
        auto* A=Cast<USFCommanderAnim>(It->GetMesh()->GetAnimInstance());
        S+=FString::Printf(TEXT("commander,%d,%.2f,%.2f,%d,%d,%d\n"),It->FindComponentByClass<UCombatTargetComponent>()->GetTeamIndex(),It->GetCharacterMovement()->MaxWalkSpeed,
            A?A->Speed:0.f,A?int32(A->bMoving):0,A&&A->GetCurrentActiveMontage()?1:0,It->GetUnlockedSkillTier());
    }
    for(TActorIterator<AActor> It(W);It;++It)if(auto* B=It->FindComponentByClass<USFBuffComponent>())
    {
        const auto* U=Cast<AUnitBase>(*It);
        if(U)S+=FString::Printf(TEXT("unit,%s,%d,%.2f,%.2f\n"),*U->GetName(),U->GetTeamIndex(),U->GetCurrentAttackPower(),U->GetCurrentMoveSpeed());
        for(UBuffData* Buff:B->GetBuffList())S+=FString::Printf(TEXT("buff,%s,%s,%.2f,%.4f,%.2f,%d,%s\n"),*It->GetName(),*Buff->KeyName,Buff->InitialTime,
            Buff->GetTimeRate(),Buff->RemainTime,Buff->ParticleComp&&Buff->ParticleComp->IsActive()?1:0,*Buff->DisplayName.ToString());
    }
    if(auto* Synergy=W->GetSubsystem<USFBuildingSynergySubsystem>())
    {
        for(int32 T=0;T<2;++T)S+=FString::Printf(TEXT("synergy,%d,%d,%d\n"),T,Synergy->GetSynergyZonesForTeam(T).Num(),Synergy->GetClusterProgressInfosForTeam(T).Num());
        for(TActorIterator<ASFBaseBuilding> It(W);It;++It)
            S+=FString::Printf(TEXT("building,%s,%d,%s,%d,%d,%.2f\n"),*It->GetName(),It->GetTeamIndex(),It->GetBuildingDefinition()?*It->GetBuildingDefinition()->BuildingId.ToString():TEXT("none"),It->GetCurrentTier(),It->IsTierActivated()?1:0,Synergy->GetProductionBonusPercent(*It));
    }
    auto* Voice=W->GetSubsystem<USFUnitVoiceSubsystem>();auto* Audio=Voice?Voice->GetActiveVoice():nullptr;
    S+=FString::Printf(TEXT("voice,%d,%d\n"),Audio!=nullptr,Audio&&Audio->IsPlaying()?1:0);
    FFileHelper::SaveStringToFile(S,*(FPaths::ProjectSavedDir()/TEXT("PVPQA")/(Instance+TEXT(".presentation"))),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
#endif
