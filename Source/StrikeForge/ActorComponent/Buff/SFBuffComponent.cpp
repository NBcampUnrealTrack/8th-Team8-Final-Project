// Fill out your copyright notice in the Description page of Project Settings.

#include "ActorComponent/Buff/SFBuffComponent.h"
#include "ActorComponent/Combat/CombatTargetComponent.h"
#include "ActorComponent/Stat/HealthComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Units/Unit/UnitBase.h"

void UBuffData::ExecuteBuff()
{
	// 버프를 받은 직후 실행
	if (OnBuffBeginEvent.IsBound())
	{
		OnBuffBeginEvent.Broadcast();
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

	if (BuffList.Num() == 0)
	{
		return;
	}

	/* 버프 컴포넌트에서 관리중인 값 초기화 */
	Current_Status = Origin_Status;

	/* 각 버프의 이벤트 실행 */
	for (TWeakObjectPtr<UBuffData> Buff : BuffList)
	{
		if (!Buff.IsValid())
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
			if (Buff->OnBuffEndEvent.IsBound())
			{
				Buff->OnBuffEndEvent.Broadcast();
			}

			Buff->ConditionalBeginDestroy();
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

	Flag_Apply = EBuffApply::None;
}

void USFBuffComponent::AddBuff(const FString& _KeyName, const FText& _DisplayName, TFunction<void()> _TickEvent, TFunction<void()> _BeginEvent, TFunction<void()> _EndEvent, float _InitialTime, float _TickInterval)
{
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

	BuffList.Last()->ExecuteBuff();
	ShrinkBuff();
}

void USFBuffComponent::ShrinkBuff()
{
	BuffList.Remove(nullptr);
	BuffList.Shrink();
}

void USFBuffComponent::ClearBuff()
{
	for (UBuffData* BD : BuffList)
	{
		BD->FinishBuff(true);
	}
	BuffList.Empty();
}

const TArray<TObjectPtr<UBuffData>> USFBuffComponent::GetBuffList()
{
	ShrinkBuff();
	return BuffList;
}