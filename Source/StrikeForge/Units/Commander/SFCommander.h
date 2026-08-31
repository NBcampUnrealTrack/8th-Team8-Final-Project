// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InGame/SFSelectINfo/SFSelectable.h"
#include "Units/Interfaces/Movable.h"
#include "SFCommander.generated.h"

class ASFInGamePlayerController;
class USFBuffComponent;

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
	
	virtual FSFSelectableInfo GetSelectableInfo_Implementation() override;
	virtual void MoveToLocation_Implementation(const FVector& InDestination) override;
	virtual void StopMove_Implementation() override;
	
	// 건설명령 함수. Center=건물좌표, Bounds=건물바운드, Rotation=회전값.
	// 스태틱메시의 GetBounds()를 가져와 Bounds에 등록해주세요.
	UFUNCTION(BlueprintCallable, Category = "Command")
	void MoveToBuild(FVector Center, FBoxSphereBounds Bounds, float Rotation = 0.f);

	UFUNCTION()
	void ActiveCommanderSkill(int32 SkillTier);

	void ActiveCommanderSkill_Beast(int32 SkillTier, TWeakObjectPtr<UWorld> World);
	void ActiveCommanderSkill_Ether(int32 SkillTier, TWeakObjectPtr<UWorld> World);

	TArray<FBuffTarget> GetSkillActors(TWeakObjectPtr<UWorld> World, float Radius);
	TArray<FBuffTarget> GetSkillActors(TWeakObjectPtr<UWorld> World, TSubclassOf<AActor> ActorClass);

protected:
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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Meta = (ExposeOnSpawn = "true"), Category = "SpawnProperty")
	uint8 bMyCommander : 1;	// true일 시 로드 즉시 플레이어의 제어를 받음.

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Race")
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

	FTimerHandle CooldownTimer_Skill1;
	FTimerHandle CooldownTimer_Skill2;
	FTimerHandle CooldownTimer_Skill3;
};
