// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SFBuffComponent.generated.h"

class AUnitBase;
class UNiagaraComponent;
class UNiagaraSystem;

DECLARE_MULTICAST_DELEGATE(FOnBuffEvent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSFOnBuffStatsChanged);

UCLASS()
class STRIKEFORGE_API UBuffData : public UObject
{
    GENERATED_BODY()
public:
    UBuffData()
        : KeyName(FString()), DisplayName(FText()), OnBuffTickEvent(FOnBuffEvent()), OnBuffBeginEvent(FOnBuffEvent()), OnBuffEndEvent(FOnBuffEvent()), InitialTime(-1.f), TickInterval(0.f) {
    }

    void ExecuteBuff(); // 버프 작동. 액터컴포넌트에서 AddBuff 시 함께 실행됨. 만지지 말 것

    void FinishBuff(bool bCallEndEvent);  // 버프 종료. 매개변수: 해당 버프의 종료이벤트 실행여부
    
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
    UPROPERTY(BlueprintReadWrite, Category = "Buff|Value")
    TObjectPtr<UNiagaraComponent> ParticleComp; // 이펙트

    float RemainTime;       // 현재 남은 시간
    float RemainInterval;   // 현재 남은 주기

    FOnBuffEvent OnBuffTickEvent;
    FOnBuffEvent OnBuffBeginEvent;
    FOnBuffEvent OnBuffEndEvent;
};

// 버프 컴포넌트에서 관리하고 적용하는 값들을 모아놓은 구조체
USTRUCT()
struct FStatusData
{
    GENERATED_BODY()

    float Speed = 0.f;
    float AtkMul = 1.f;
    float HPMul = 1.f;
    float DmgMul = 1.f;
};

// 버프가 적용되는 시점
UENUM(meta = (Bitflags))
enum class EBuffApply : uint8
{
    None        = 0,
    Speed       = 1 << 0, // 1
    Atk         = 1 << 1, // 2
    MaxHealth   = 1 << 2, // 4
    DamageMul   = 1 << 3  // 8
};
ENUM_CLASS_FLAGS(EBuffApply);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class STRIKEFORGE_API USFBuffComponent : public UActorComponent
{
	GENERATED_BODY()

public:	

    // C++전용
    void AddBuff(const FString& _KeyName, const FText& _DisplayName, TFunction<void()> _TickEvent, TFunction<void()> _BeginEvent, TFunction<void()> _EndEvent, float _InitialTime = -1.f, float _TickInterval = 0.f, UNiagaraSystem* _Particle = nullptr);
    void ShrinkBuff();  // 만료된 버프 정리
    void ClearBuff();   // 모든 버프 제거

    const TArray<TObjectPtr<UBuffData>> GetBuffList(); // 해당 액터가 보유중인 모든 버프 가져오기.
    FORCEINLINE const FStatusData GetOriginStatus() { return Origin_Status; };

    // 해당 스탯을 이번 프레임에 갱신하겠다.
    FORCEINLINE void ApplyBuffFlag(EBuffApply Flags) { EnumAddFlags(Flag_Apply, Flags); }

    /** Fired after the effective movement/combat/health values have been applied. */
    UPROPERTY(BlueprintAssignable, Category = "Buff|Stats")
    FSFOnBuffStatsChanged OnBuffStatsChanged;

protected:
	USFBuffComponent();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    void RefreshBuffStats(float DeltaTime);

public:
    FStatusData Current_Status;

protected:
    /** 버프 리스트*/
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State", meta = (AllowPrivateAccess = true))
    TArray<TObjectPtr<UBuffData>> BuffList;

    // 해당 오너가 가지고 있는 버프상태
    FStatusData Origin_Status;

    EBuffApply Flag_Apply = EBuffApply::None;   // 이번 프레임 내 특정 스탯의 갱신여부를 저장
};
