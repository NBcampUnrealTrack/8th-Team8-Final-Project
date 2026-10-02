// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "SFCommanderAnim.generated.h"

DECLARE_DYNAMIC_DELEGATE_OneParam(FSkillActiveDelegate, int32, SkillTier);

class ASFCommander;
class AUnitAIController;

UCLASS()
class STRIKEFORGE_API USFCommanderAnim : public UAnimInstance
{
	GENERATED_BODY()
protected:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	UFUNCTION()
	void AnimNotify_SkillSoundNotify();

public:
	UPROPERTY(BlueprintReadOnly)
	float Speed;
	UPROPERTY(BlueprintReadOnly)
	uint8 bMoving : 1;
	UPROPERTY(BlueprintReadOnly)
	uint8 bSkillFloating : 1;

	FSkillActiveDelegate OnSkillActiveDelegate;
	float SkillDelegateTier;
	TWeakObjectPtr<USoundBase> CurrentSkillSound;

protected:
	TWeakObjectPtr<ASFCommander> Commander;
	TWeakObjectPtr<AUnitAIController> UnitAIController;
};
