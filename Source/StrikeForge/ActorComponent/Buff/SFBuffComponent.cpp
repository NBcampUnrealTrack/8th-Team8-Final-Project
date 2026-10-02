// Fill out your copyright notice in the Description page of Project Settings.

#include "ActorComponent/Buff/SFBuffComponent.h"
#include "ActorComponent/Combat/CombatTargetComponent.h"
#include "ActorComponent/Stat/HealthComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "Units/Unit/UnitBase.h"

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
	// 버프종료 이벤트 실행
	if (bCallEndEvent == true && OnBuffEndEvent.IsBound())
	{
		OnBuffEndEvent.Broadcast();
	}

	if (IsValid(ParticleComp))
	{
		ParticleComp->Deactivate();
	}

	this->ConditionalBeginDestroy();
}

const float UBuffData::GetTimeRate()
{
	TWeakObjectPtr<UWorld> World = GetWorld();
	if (!World.IsValid())
		return 0.f;

	return RemainTime / InitialTime;
}

void UBuffData::BeginDestroy()
{
	OnBuffTickEvent.Clear();
	OnBuffBeginEvent.Clear();
	OnBuffEndEvent.Clear();

	Super::BeginDestroy();
}

//------------------------

USFBuffComponent::USFBuffComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
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
	if (bStatsApplied) OnBuffStatsChanged.Broadcast();
}

void USFBuffComponent::AddBuff(const FString& _KeyName, const FText& _DisplayName, TFunction<void()> _TickEvent, TFunction<void()> _BeginEvent, TFunction<void()> _EndEvent, float _InitialTime, float _TickInterval, UNiagaraSystem* _Particle)
{
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
		if (BD->KeyName == _KeyName)
		{
			BD->FinishBuff(false);
			BuffList.Remove(BD);
			break;
		}
	}
	BuffList.Add(NewObject<UBuffData>(this));
	BuffList.Last()->KeyName = _KeyName;
	BuffList.Last()->DisplayName = _DisplayName;
	BuffList.Last()->InitialTime = _InitialTime;
	BuffList.Last()->TickInterval = _TickInterval;
	BuffList.Last()->OnBuffTickEvent.AddLambda([_TickEvent]() {_TickEvent(); });
	BuffList.Last()->OnBuffBeginEvent.AddLambda([_BeginEvent]() {_BeginEvent(); });
	BuffList.Last()->OnBuffEndEvent.AddLambda([_EndEvent]() {_EndEvent(); });

	if (IsValid(_Particle))
	{
		BuffList.Last()->ParticleComp = UNiagaraFunctionLibrary::SpawnSystemAttached(_Particle, GetOwner()->GetRootComponent(), NAME_None, FVector::ZeroVector, FRotator::ZeroRotator, FVector::OneVector, EAttachLocation::KeepRelativeOffset, false, ENCPoolMethod::AutoRelease, false);
		BuffList.Last()->ParticleComp->SetUsingAbsoluteRotation(true);
	}

	BuffList.Last()->ExecuteBuff();
	ShrinkBuff();
	// Zero-interval stat modifiers apply before AddBuff returns, not one tick later.
	RefreshBuffStats(0.f);
}

void USFBuffComponent::ShrinkBuff()
{
	// FinishBuff starts destruction immediately; such objects can still be non-null
	// and pass IsValid until GC. Never repeat their end callback on subsequent ticks.
	BuffList.RemoveAll([](const TObjectPtr<UBuffData>& Buff)
	{
		return !IsValid(Buff) || Buff->HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed);
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
}

const TArray<TObjectPtr<UBuffData>> USFBuffComponent::GetBuffList()
{
	ShrinkBuff();
	return BuffList;
}
