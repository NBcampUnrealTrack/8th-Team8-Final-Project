// Fill out your copyright notice in the Description page of Project Settings.

#include "ActorComponent/Buff/SFBuffComponent.h"
#include "Containers/Ticker.h"

void UBuffData::ExecuteBuff()
{
	TWeakObjectPtr<UWorld> World = GetWorld();
	if (!World.IsValid())
		return;

	// 버프를 받은 직후 실행
	if (OnBuffBeginEvent.IsBound())
	{
		OnBuffBeginEvent.Broadcast();
	}

	// 버프가 끝나면 실행
	World->GetTimerManager().SetTimer(
		BuffTimerHandle_End,
		[this]()
		{
			if (OnBuffEndEvent.IsBound())
			{
				OnBuffEndEvent.Broadcast();
			}

			if (BuffTickerHandle.IsValid())
			{
				FTSTicker::GetCoreTicker().RemoveTicker(BuffTickerHandle);
				BuffTickerHandle.Reset();
			}
			this->ConditionalBeginDestroy();
		},
		InitialTime,
		false);

	// 버프가 진행되는 동안 정해진 주기마다 실행
	if (TickInterval <= 0.f)
	{
		BuffTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateLambda(
				[TickEvent = OnBuffTickEvent, EndHandler = BuffTimerHandle_End](float DeltaTime)
				{
					if (TickEvent.IsBound())
					{
						TickEvent.Broadcast();
						return true;
					}
					else
					{
						return false;
					}
				}),
			TickInterval);
	}
}

void UBuffData::FinishBuff()
{
	TWeakObjectPtr<UWorld> World = GetWorld();
	if (World.IsValid() && World->GetTimerManager().TimerExists(BuffTimerHandle_End))
	{
		// 버프종료 이벤트 실행 및 타이머 해제
		if (OnBuffEndEvent.IsBound())
		{
			OnBuffEndEvent.Broadcast();
		}
		World->GetTimerManager().ClearTimer(BuffTimerHandle_End);

		// 버프 틱 이벤트 해제
		if (BuffTickerHandle.IsValid())
		{
			FTSTicker::GetCoreTicker().RemoveTicker(BuffTickerHandle);
			BuffTickerHandle.Reset();
		}
	}
	this->ConditionalBeginDestroy();
}

const float UBuffData::GetTimeRate()
{
	TWeakObjectPtr<UWorld> World = GetWorld();
	if (!World.IsValid())
		return 0.f;

	return World->GetTimerManager().GetTimerRemaining(BuffTimerHandle_End) / World->GetTimerManager().GetTimerRate(BuffTimerHandle_End);
}

void UBuffData::BeginDestroy()
{
	OnBuffTickEvent.Clear();
	OnBuffBeginEvent.Clear();
	OnBuffEndEvent.Clear();

	TWeakObjectPtr<UWorld> World = GetWorld();
	if (World.IsValid())
	{
		World->GetTimerManager().ClearTimer(BuffTimerHandle_End);
	}
	if (BuffTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(BuffTickerHandle);
		BuffTickerHandle.Reset();
	}

	Super::BeginDestroy();
}

//------------------------

USFBuffComponent::USFBuffComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USFBuffComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearBuff();
	Super::EndPlay(EndPlayReason);
}

void USFBuffComponent::AddBuff(const FString& _KeyName, const FText& _DisplayName, TFunction<void()> _TickEvent, TFunction<void()> _BeginEvent, TFunction<void()> _EndEvent, float _InitialTime, float _TickInterval)
{
	for (UBuffData* BD : BuffList)
	{
		if (BD->KeyName == _KeyName)
		{
			BD->FinishBuff();
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
		BD->FinishBuff();
	}
	BuffList.Empty();
}