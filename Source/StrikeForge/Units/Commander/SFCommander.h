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

UCLASS()
class STRIKEFORGE_API ASFCommander 
	: public ACharacter
	, public ISFSelectable
	, public IMovable
{
	GENERATED_BODY()

public:
	ASFCommander();
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
	FText GetSkillEffectDescription(int32 SkillTier) const;
	ERace GetCommanderRace() const { return Race; }

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Skill|Asset")
	TArray<TSoftObjectPtr<UTexture2D>> SkillIcons;

	UFUNCTION()
	void ActiveCommanderSkill_Beast(int32 SkillTier);

	UFUNCTION()
	void ActiveCommanderSkill_Ether(int32 SkillTier);

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

	void LoadPlayerController();

	UFUNCTION()
	void CheckFailMoving(bool IsSuccessed);

	void MoveToAction(FVector Dest, ECommandAction Action);

	bool CheckCanBuild();

	void BeginBuild();

	void PrintMessage(FString Msg);

	void StopMoving();
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Component")
	TObjectPtr<USFBuffComponent> BuffComponent;
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Component")
	TObjectPtr<UHealthComponent> HealthComponent;
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Component")
	TObjectPtr<UCombatTargetComponent> CombatTargetComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Skill|Montage")
	TObjectPtr<UAnimMontage> SkillMontage;

	// TODO : 이후 구조체로 통합해 수정할 예정입니다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Skill|Asset")
	TObjectPtr<USoundBase> SkillSound1;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Skill|Asset")
	TObjectPtr<UNiagaraSystem> SkillEffect1;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Skill|Asset")
	TObjectPtr<UNiagaraSystem> SkillHitEffect1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Skill|Asset")
	TObjectPtr<USoundBase> SkillSound2;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Skill|Asset")
	TObjectPtr<UNiagaraSystem> SkillEffect2;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Skill|Asset")
	TObjectPtr<UNiagaraSystem> SkillHitEffect2;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Skill|Asset")
	TObjectPtr<USoundBase> SkillSound3;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Skill|Asset")
	TObjectPtr<UNiagaraSystem> SkillEffect3;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Skill|Asset")
	TObjectPtr<UNiagaraSystem> SkillHitEffect3;

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
