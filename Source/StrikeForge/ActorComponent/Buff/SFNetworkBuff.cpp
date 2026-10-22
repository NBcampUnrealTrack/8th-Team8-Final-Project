#include "SFBuffComponent.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Units/Unit/UnitBase.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"

void USFBuffComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(USFBuffComponent,NetworkBuffs);
    DOREPLIFETIME(USFBuffComponent,NetworkStats);
}

void USFBuffComponent::PreReplication(IRepChangedPropertyTracker& Tracker)
{
    Super::PreReplication(Tracker);
    if(const auto* Character=Cast<ACharacter>(GetOwner()))
        NetworkStats.Speed=Character->GetCharacterMovement()->MaxWalkSpeed;
    if(const auto* Unit=Cast<AUnitBase>(GetOwner())) NetworkStats.AttackMultiplier=Unit->AttackMultiplier;
}

void USFBuffComponent::RefreshNetworkBuffs()
{
    if(!GetOwner()||!GetOwner()->HasAuthority())return;
    NetworkBuffs.Reset(BuffList.Num());
    for(const UBuffData* Buff:BuffList)if(IsValid(Buff)&&!Buff->bFinished)
    {
        auto& Entry=NetworkBuffs.AddDefaulted_GetRef();
        Entry.Key=Buff->KeyName;Entry.Name=Buff->DisplayName;Entry.Description=Buff->Description;
        Entry.Image=Buff->Image;Entry.Particle=Buff->ParticleAsset;Entry.Scale=Buff->ParticleScale;
        Entry.Duration=Buff->InitialTime;Entry.Ends=Buff->NetworkEndTime;Entry.bPositive=Buff->bIsBuff;
    }
}

void USFBuffComponent::OnRep_NetworkBuffs()
{
    bNetworkPresentationPending=true;
    if(GetOwner()&&GetOwner()->HasActorBegunPlay())ApplyNetworkPresentation();
}

void USFBuffComponent::OnRep_NetworkStats()
{
    bNetworkPresentationPending=true;
    if(GetOwner()&&GetOwner()->HasActorBegunPlay())ApplyNetworkPresentation();
}

void USFBuffComponent::ApplyNetworkPresentation()
{
    if(!GetOwner()||GetOwner()->HasAuthority())return;
    bNetworkPresentationPending=false;
    if(auto* Character=Cast<ACharacter>(GetOwner());Character&&NetworkStats.Speed>0.f)
        Character->GetCharacterMovement()->MaxWalkSpeed=NetworkStats.Speed;
    if(auto* Unit=Cast<AUnitBase>(GetOwner()))Unit->AttackMultiplier=NetworkStats.AttackMultiplier;

    for(UBuffData* Buff:BuffList)if(IsValid(Buff)&&!NetworkBuffs.ContainsByPredicate(
        [Buff](const FSFBuffNetworkEntry& E){return E.Key==Buff->KeyName;}))Buff->FinishBuff(false);
    ShrinkBuff();
    for(const auto& Entry:NetworkBuffs)
    {
        const auto* Existing=BuffList.FindByPredicate([&](const TObjectPtr<UBuffData>& B){return B&&B->KeyName==Entry.Key;});
        UBuffData* Buff=Existing?Existing->Get():NewObject<UBuffData>(this);
        const bool bRestart=!Existing||Buff->NetworkEndTime!=Entry.Ends||Buff->ParticleAsset!=Entry.Particle;
        if(!Existing)BuffList.Add(Buff);
        Buff->KeyName=Entry.Key;Buff->DisplayName=Entry.Name;Buff->Description=Entry.Description;
        Buff->Image=Entry.Image;Buff->bIsBuff=Entry.bPositive;Buff->InitialTime=Entry.Duration;
        Buff->bNetworkPresentation=true;Buff->NetworkEndTime=Entry.Ends;
        if(bRestart)
        {
            if(Buff->ParticleComp)Buff->ParticleComp->Deactivate();
            Buff->ParticleAsset=Entry.Particle;Buff->ParticleScale=Entry.Scale;
            if(Entry.Particle)
            {
                Buff->ParticleComp=UNiagaraFunctionLibrary::SpawnSystemAttached(Entry.Particle,GetOwner()->GetRootComponent(),NAME_None,
                    FVector::ZeroVector,FRotator::ZeroRotator,Entry.Scale,EAttachLocation::KeepRelativeOffset,false,ENCPoolMethod::AutoRelease,true);
                if(Buff->ParticleComp)
                {
                    Buff->ParticleComp->SetUsingAbsoluteRotation(true);
                    Buff->ParticleComp->OnSystemFinished.AddDynamic(Buff,&UBuffData::OnNiagaraFinished);
                }
            }
        }
        Buff->GetTimeRate();
    }
    OnBuffStatsChanged.Broadcast();
}
