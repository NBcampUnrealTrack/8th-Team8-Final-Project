// Fill out your copyright notice in the Description page of Project Settings.

#include "ActorComponent/Buff/SFBuffComponent.h"
#include "ActorComponent/Combat/CombatTargetComponent.h"
#include "ActorComponent/Stat/HealthComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "Units/Unit/UnitBase.h"
#include "GameFramework/GameStateBase.h"

void UBuffData::ExecuteBuff()
{
	// 버프를 받은 직후 실행
	if (OnBuffBeginEvent.IsBound())
	{
		OnBuffBeginEvent.Broadcast();
	}

	if (IsValid(ParticleComp))
	{
		ParticleComp->Activate(true);
	}

	// 버프 종료까지 남은 시간
	RemainTime = InitialTime;

	// 버프효과가 적용되는 주기
	RemainInterval = TickInterval;
}

void UBuffData::FinishBuff(bool bCallEndEvent)
{
    if (bFinished) return;
    bFinished = true;
	// 버프종료 이벤트 실행
	if (bCallEndEvent == true && OnBuffEndEvent.IsBound())
	{
		OnBuffEndEvent.Broadcast();
	}

	if (IsValid(ParticleComp))
	{
		ParticleComp->Deactivate();
	}
	
	OnBuffTickEvent.Clear();
	OnBuffBeginEvent.Clear();
	OnBuffEndEvent.Clear();
}

const float UBuffData::GetTimeRate()
{
	TWeakObjectPtr<UWorld> World = GetWorld();
	if (!World.IsValid())
		return 0.f;

    if (bFinished) return 0.f;
    if (bNetworkPresentation && InitialTime >= 0.f)
    {
        const AGameStateBase* State = World->GetGameState();
        RemainTime = FMath::Max(0.f, float(NetworkEndTime - (State ? State->GetServerWorldTimeSeconds() : World->GetTimeSeconds())));
    }
    if (InitialTime < 0.f) return 1.f;
	return InitialTime > 0.f ? FMath::Clamp(RemainTime / InitialTime, 0.f, 1.f) : 0.f;
}

void UBuffData::OnNiagaraFinished(UNiagaraComponent* PSystem)
{
    if (ParticleComp == PSystem) ParticleComp = nullptr;
}

void UBuffData::BeginDestroy()
{

	Super::BeginDestroy();
}

//------------------------

USFBuffComponent::USFBuffComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
    SetIsReplicatedByDefault(true);
}

void USFBuffComponent::BeginPlay()
{
	Super::BeginPlay();
	Flag_Apply = EBuffApply::None;

	TWeakObjectPtr<ACharacter> SFChar = GetOwner<ACharacter>();
	if (SFChar.IsValid())
	{
		TWeakObjectPtr<UCharacterMovementComponent> SFMovement = SFChar->GetCharacterMovement();
		if (SFMovement.IsValid())
		{
			Origin_Status.Speed = SFMovement->MaxWalkSpeed;
		}
	}
}

void USFBuffComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearBuff();
	Super::EndPlay(EndPlayReason);
}

void USFBuffComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (GetOwner() && !GetOwner()->HasAuthority())
    {
        if (bNetworkPresentationPending) ApplyNetworkPresentation();
        for (UBuffData* Buff : BuffList) if (IsValid(Buff)) Buff->GetTimeRate();
        return;
    }
	RefreshBuffStats(DeltaTime);
}

void USFBuffComponent::RefreshBuffStats(float DeltaTime)
{
	if (BuffList.Num() == 0 && Flag_Apply == EBuffApply::None)
	{
		return;
	}

	/* 버프 컴포넌트에서 관리중인 값 초기화 */
	Current_Status = Origin_Status;

	/* 각 버프의 이벤트 실행 */
	for (TWeakObjectPtr<UBuffData> Buff : BuffList)
	{
		if (!Buff.IsValid() || Buff->HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed))
		{
			continue;
		}
		if (Buff->InitialTime >= 0.f)
		{
			Buff->RemainTime -= DeltaTime;
		}
		Buff->RemainInterval -= DeltaTime;

		// 버프가 끝나면 실행 (InitialTime이 음수라면 버프가 자연적으로 끝내지 않음)
		if (Buff->InitialTime >= 0.f && Buff->RemainTime <= 0.f)
		{
			Buff->FinishBuff(true);
			continue;
		}

		// 버프가 진행되는 동안 정해진 주기마다 실행
		if (Buff->RemainInterval <= 0.f)
		{
			if (Buff->OnBuffTickEvent.IsBound())
			{
				Buff->OnBuffTickEvent.Broadcast();
			}
			Buff->RemainInterval = Buff->TickInterval;
		}
	}

	/* 이번 프레임 내에 적용된 버프 종합 적용 */
	if (EnumHasAllFlags(Flag_Apply, EBuffApply::Speed))
	{
		TWeakObjectPtr<ACharacter> SFChar = GetOwner<ACharacter>();
		if (SFChar.IsValid())
		{
			TWeakObjectPtr<UCharacterMovementComponent> SFMovement = SFChar->GetCharacterMovement();
			if (SFMovement.IsValid())
			{
				SFMovement->MaxWalkSpeed = Current_Status.Speed;
			}
		}
	}

	if (EnumHasAllFlags(Flag_Apply, EBuffApply::Atk))
	{
		TWeakObjectPtr<AUnitBase> SFUnit = GetOwner<AUnitBase>();
		if (SFUnit.IsValid())
		{
			SFUnit->AttackMultiplier = Current_Status.AtkMul;
		}
	}

	if (EnumHasAllFlags(Flag_Apply, EBuffApply::MaxHealth))
	{
		// 최대체력을 갱신해야하는 타이밍
		TWeakObjectPtr<UHealthComponent> SFHealthComp = GetOwner()->GetComponentByClass<UHealthComponent>();
		if (SFHealthComp.IsValid())
		{
			if (SFHealthComp->GetCurrentHealth() > 0.f)
			{
				SFHealthComp->BoostHealth(Current_Status.HPMul);
			}
		}
	}

	if (EnumHasAllFlags(Flag_Apply, EBuffApply::DamageMul))
	{
		TWeakObjectPtr<UCombatTargetComponent> SFCombatComp = GetOwner()->GetComponentByClass<UCombatTargetComponent>();
		if (SFCombatComp.IsValid())
		{
			SFCombatComp->SetDamageMultiplier(Current_Status.DmgMul);
		}
	}

	const bool bStatsApplied = Flag_Apply != EBuffApply::None;
	Flag_Apply = EBuffApply::None;
	ShrinkBuff();
    RefreshNetworkBuffs();
	if (bStatsApplied) OnBuffStatsChanged.Broadcast();
}

void USFBuffComponent::AddBuff(const FInputBuffData& InputBuffData)
{
    if (GetOwner() && !GetOwner()->HasAuthority()) return;
	ShrinkBuff();
	if (BuffList.IsEmpty())
	{
		// Unit data / Blueprint initialization may set movement after BeginPlay.
		if (const ACharacter* Character = GetOwner<ACharacter>())
			if (const UCharacterMovementComponent* Move = Character->GetCharacterMovement())
				Origin_Status.Speed = Move->MaxWalkSpeed;
	}
	for (UBuffData* BD : BuffList)
	{
		if (BD->KeyName == InputBuffData.KeyName)
		{
			BD->FinishBuff(false);
			BuffList.Remove(BD);
			break;
		}
	}
	BuffList.Add(NewObject<UBuffData>(this));
	BuffList.Last()->KeyName = InputBuffData.KeyName;
	BuffList.Last()->DisplayName = InputBuffData.DisplayName;
	BuffList.Last()->Description = InputBuffData.Description;
	BuffList.Last()->Image = InputBuffData.Image;
	BuffList.Last()->InitialTime = InputBuffData.InitialTime;
	BuffList.Last()->TickInterval = InputBuffData.TickInterval;
	BuffList.Last()->bIsBuff = InputBuffData.bIsBuff;
	if (InputBuffData.TickEvent)
		BuffList.Last()->OnBuffTickEvent.AddLambda([InputBuffData]() {InputBuffData.TickEvent(); });
	if (InputBuffData.BeginEvent)
		BuffList.Last()->OnBuffBeginEvent.AddLambda([InputBuffData]() {InputBuffData.BeginEvent(); });
	if (InputBuffData.EndEvent)
		BuffList.Last()->OnBuffEndEvent.AddLambda([InputBuffData]() {InputBuffData.EndEvent(); });

	// 파티클 크기 결정
	FVector ParticlePosition = FVector::ZeroVector;
	FVector ParticleScale = FVector::OneVector;
	if (ACharacter* SFCharacter = GetOwner<ACharacter>())
	{
		// 캐릭터라면 캡슐크기로 결정
		if (IsValid(SFCharacter->GetCapsuleComponent()))
		{
			ParticleScale.X = ParticleScale.Y = SFCharacter->GetCapsuleComponent()->GetUnscaledCapsuleRadius() * InputBuffData.ParticleScaleForCharacter.X;
			ParticleScale.Z = SFCharacter->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() * InputBuffData.ParticleScaleForCharacter.Y;
		}
	}
	else
	{
		// 아니라면 바운드 크기로 결정
		FVector Origin;
		GetOwner()->GetActorBounds(true, Origin, ParticleScale);
		ParticleScale.X *= InputBuffData.ParticleScaleForOther.X;
		ParticleScale.Y *= InputBuffData.ParticleScaleForOther.X;
		ParticleScale.Z *= InputBuffData.ParticleScaleForOther.Y;
	}
    BuffList.Last()->ParticleAsset = InputBuffData.Particle;
    BuffList.Last()->ParticleScale = ParticleScale;
    BuffList.Last()->NetworkEndTime = GetWorld()->GetTimeSeconds() + InputBuffData.InitialTime;
	if (IsValid(InputBuffData.Particle))
	{
		BuffList.Last()->ParticleComp = UNiagaraFunctionLibrary::SpawnSystemAttached(InputBuffData.Particle, GetOwner()->GetRootComponent(), NAME_None, ParticlePosition, FRotator::ZeroRotator, ParticleScale, EAttachLocation::KeepRelativeOffset, false, ENCPoolMethod::AutoRelease, false);
		BuffList.Last()->ParticleComp->SetUsingAbsoluteRotation(true);
		BuffList.Last()->ParticleComp->OnSystemFinished.AddDynamic(BuffList.Last(), &UBuffData::OnNiagaraFinished);
	}

	BuffList.Last()->ExecuteBuff();
	ShrinkBuff();
	// Zero-interval stat modifiers apply before AddBuff returns, not one tick later.
	RefreshBuffStats(0.f);
}

void USFBuffComponent::ShrinkBuff()
{
    // Expiration is independent of Niagara finishing (a buff can have no particle).
    // Remove finished entries before GC and never repeat their end callback.
	BuffList.RemoveAll([](const TObjectPtr<UBuffData>& Buff)
	{
		return !IsValid(Buff) || Buff->bFinished || Buff->HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed);
	});
	BuffList.Shrink();
}

void USFBuffComponent::ClearBuff()
{
	ShrinkBuff();
	for (UBuffData* BD : BuffList)
	{
		BD->FinishBuff(true);
	}
	BuffList.Empty();
	RefreshBuffStats(0.f);
    RefreshNetworkBuffs();
}

const TArray<TObjectPtr<UBuffData>> USFBuffComponent::GetBuffList()
{
	ShrinkBuff();
	return BuffList;
}
