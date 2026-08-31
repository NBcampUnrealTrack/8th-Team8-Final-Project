// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SFBuffComponent.generated.h"

DECLARE_MULTICAST_DELEGATE(FOnBuffEvent);

UCLASS()
class STRIKEFORGE_API UBuffData : public UObject
{
    GENERATED_BODY()
public:
    UBuffData()
        : KeyName(FString()), DisplayName(FText()), OnBuffTickEvent(FOnBuffEvent()), OnBuffBeginEvent(FOnBuffEvent()), OnBuffEndEvent(FOnBuffEvent()), InitialTime(-1.f), TickInterval(0.f) {
    }

    void ExecuteBuff(); // 버프 작동. 액터컴포넌트에서 AddBuff 시 함께 실행됨. 만지지 말 것

    void FinishBuff();  // 버프 종료. 해당 버프의 종료이벤트를 실행하고 삭제함.
    
    const float GetTimeRate(); // 버프 남은시간 비율(0~1)로 가져오기.

protected:
    virtual void BeginDestroy() override;

public:
    UPROPERTY(BlueprintReadWrite, Category = "Buff|Info")
    FString KeyName;   // 버프 Key명 (중복방지)
    UPROPERTY(BlueprintReadWrite, Category = "Buff|Info")
    FText DisplayName; // 버프 명칭 (UI 디스플레이용)
    UPROPERTY(BlueprintReadWrite, Category = "Buff|Value")
    float InitialTime;  // 지속시간 초기값. (음수라면 무한)
    UPROPERTY(BlueprintReadWrite, Category = "Buff|Value")
    float TickInterval; // 효과발생 주기. (0이면 매 프레임 지속)

    FTimerHandle BuffTimerHandle_End;
    FTSTicker::FDelegateHandle BuffTickerHandle;

    FOnBuffEvent OnBuffTickEvent;
    FOnBuffEvent OnBuffBeginEvent;
    FOnBuffEvent OnBuffEndEvent;
};

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class STRIKEFORGE_API USFBuffComponent : public UActorComponent
{
	GENERATED_BODY()

public:	

    // C++전용
    void AddBuff(const FString& _KeyName, const FText& _DisplayName, TFunction<void()> _TickEvent, TFunction<void()> _BeginEvent, TFunction<void()> _EndEvent, float _InitialTime = -1.f, float _TickInterval = 0.f);
    void ShrinkBuff();  // 만료된 버프 정리
    void ClearBuff();   // 모든 버프 제거

    FORCEINLINE const TArray<TObjectPtr<UBuffData>> GetBuffList() { return BuffList; }; // 해당 액터가 보유중인 모든 버프 가져오기.

protected:
	USFBuffComponent();

    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
    /** 버프 리스트*/
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State", meta = (AllowPrivateAccess = true))
    TArray<TObjectPtr<UBuffData>> BuffList;
};
