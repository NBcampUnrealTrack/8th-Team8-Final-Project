// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InGame/SFSelectINfo/SFSelectable.h"
#include "Units/Interfaces/Movable.h"
#include "SFCommander.generated.h"

class ASFInGamePlayerController;
class UAnimationAsset;
class USFBuffComponent;
class UHealthComponent;
class UCombatTargetComponent;
class UNiagaraSystem;
class UTexture2D;
struct FPathFollowingResult;

UENUM(BlueprintType)
enum class ERace : uint8
{
	BEAST,
	ETHERIAN
};

UENUM()
enum class ECommandAction : uint8
{
	NONE,
	BUILD
};

USTRUCT()
struct FBuffTarget
{
	GENERATED_BODY()

	TWeakObjectPtr<AActor> ActorPtr;
	TWeakObjectPtr<USFBuffComponent> BuffCompPtr;
};

USTRUCT(BlueprintType)
struct FSkillSpecificValue
{
	GENERATED_BODY()

	UPROPERTY(VisibleDefaultsOnly)
	FName Key;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float Value;
};

USTRUCT(BlueprintType)
struct FSkillAsset
{
	GENERATED_BODY()

	// 스킬명
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Skill|Asset")
	FString Name;

	// 버프 지속시간 (초 단위, 음수라면 무제한)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Skill|Asset")
	float Time;

	// 효과 발동 주기 (0이면 매 프레임 발동)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Skill|Asset")
	float Interval;

	// 스킬 반경 (100 = 1m, 음수라면 맵 전역)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Skill|Asset")
	float Radius;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, EditFixedSize, Category = "SF|Skill|Asset")
	TArray<FSkillSpecificValue> SkillValue;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Skill|Montage")
	TObjectPtr<UAnimMontage> SkillMontage;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Skill|Asset")
	TObjectPtr<USoundBase> SkillSound;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Skill|Asset")
	TObjectPtr<UNiagaraSystem> SkillEffect;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Skill|Asset")
	TObjectPtr<UNiagaraSystem> SkillHitEffect;
public:
	const float FindValue(FName TargetKey)
	{
		return SkillValue.FindByPredicate(
			[TargetKey](const FSkillSpecificValue& CurrentStruct)
			{
				return CurrentStruct.Key == TargetKey;
			})->Value;
	}
};

UCLASS()
class STRIKEFORGE_API ASFCommander 
	: public ACharacter
	, public ISFSelectable
	, public IMovable
{
	GENERATED_BODY()

public:
	ASFCommander();
	/** Set ownership before FinishSpawning so BeginPlay enables player selection. */
	void InitializePlayerCommander(int32 TeamIndex);
	bool IsPlayerCommander() const { return bMyCommander; }
	UFUNCTION(BlueprintPure, Category = "SF|Respawn")
	bool IsAwaitingRespawn() const { return bAwaitingRespawn; }
	UFUNCTION(BlueprintPure, Category = "SF|Respawn")
	float GetRespawnTimeRemaining() const;
	static constexpr float RespawnDelaySeconds = 30.f;
	friend struct FSFPlaytestMovementAccess;
	
	virtual FSFSelectableInfo GetSelectableInfo_Implementation() override;
	virtual void MoveToLocation_Implementation(const FVector& InDestination) override;
	virtual void StopMove_Implementation() override;
	
	// 건설명령 함수. Center=건물좌표, Bounds=건물바운드, Rotation=회전값.
	// 스태틱메시의 GetBounds()를 가져와 Bounds에 등록해주세요.
	UFUNCTION(BlueprintCallable, Category = "Command")
	void MoveToBuild(FVector Center, FBoxSphereBounds Bounds, float Rotation = 0.f);

	// 지휘관 초상화 애니메이션 설정
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Portrait")
	TObjectPtr<UAnimationAsset> PortraitIdleAnimation = nullptr;

	UFUNCTION()
	void ActiveCommanderSkill(int32 SkillTier);

	/** Player-facing tiers are 1..3; the Nexus stores zero-based upgrade tiers. */
	UFUNCTION(BlueprintPure, Category = "SF|Skill")
	int32 GetUnlockedSkillTier() const;

	bool IsSkillTierUnlocked(int32 SkillTier) const;
	static constexpr float SkillCooldownSeconds = 30.f;
	UFUNCTION(BlueprintPure, Category = "SF|Skill")
	float GetSkillCooldownRemaining(int32 SkillTier) const;
	FText GetSkillName(int32 SkillTier) const;
	FText ReadSkillEffectDescription(int32 SkillTier);
	ERace GetCommanderRace() const { return Race; }

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Skill|Asset")
	TArray<TSoftObjectPtr<UTexture2D>> SkillIcons;

	UFUNCTION()
	void ActiveCommanderSkill_Beast(int32 SkillTier, UNiagaraSystem* CurrentSkillEffect, UNiagaraSystem* CurrentSkillHitEffect);

	UFUNCTION()
	void ActiveCommanderSkill_Ether(int32 SkillTier, UNiagaraSystem* CurrentSkillEffect, UNiagaraSystem* CurrentSkillHitEffect);

	TArray<FBuffTarget> GetSkillActors(TWeakObjectPtr<UWorld> World, float Radius);
	TArray<FBuffTarget> GetSkillActors(TWeakObjectPtr<UWorld> World, TSubclassOf<AActor> ActorClass);

protected:
	// Game time stops with pause; each skill keeps its own timer across HUD selection changes.
	double SkillCooldownEnds[3] = {0.0, 0.0, 0.0};
	friend struct FSFCommanderCooldownTestAccess;

	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void Tick(float DeltaSeconds) override;

	virtual void PossessedBy(AController* NewController) override;

#if WITH_EDITOR
	// PostEditChangeProperty 오버라이드 선언
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	void InitSkillAsset();
#endif

	void LoadPlayerController();

	void CheckFailMoving(const FPathFollowingResult& Result);

	void MoveToAction(FVector Dest, ECommandAction Action);

	bool CheckCanBuild();

	void BeginBuild();

	void PrintMessage(FString Msg);

	void StopMoving();
	UFUNCTION()
	void HandleCommanderDeath();
	void RespawnAtNexus();
	bool FindRespawnTransform(FTransform& OutTransform) const;
	bool bAwaitingRespawn = false;
	double RespawnEndTime = 0.0;
	FTimerHandle RespawnTimer;
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Component")
	TObjectPtr<USFBuffComponent> BuffComponent;
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Component")
	TObjectPtr<UHealthComponent> HealthComponent;
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Component")
	TObjectPtr<UCombatTargetComponent> CombatTargetComponent;

	UPROPERTY(EditDefaultsOnly, EditFixedSize, Category = "SF|Skill|Asset")
	TArray<FSkillAsset> SkillAsset;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Meta = (ExposeOnSpawn = "true"), Category = "SF|SpawnProperty")
	uint8 bMyCommander : 1;	// true일 시 로드 즉시 플레이어의 제어를 받음.

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Race")
	ERace Race;

	const float MaxDistToBuild = 50.f;	// 건설이 가능한 최대거리

	// 만약 중간에 액션을 중단해야하는 일이 있다면 해당 변수를 초기화.
	ECommandAction CommandAction = ECommandAction::NONE;

	// MoveRequest 수락 근사값
	const float MoveRqAcceptRadius = 10.f;

	// 건설할 부지의 위치와 경계
	FVector BuildLocation;	// 건설할 건물의 중심
	float BuildRotation;	// 건물 회전값
	FBoxSphereBounds BuildBoundary;	// 건물 바운드

	// 플레이어 컨트롤러 불러오기 타이머
	FTimerHandle LoadPCTimer;
};
