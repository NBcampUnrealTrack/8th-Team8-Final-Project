// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "SFCommanderAnim.generated.h"

class ASFCommander;
class UNiagaraSystem;

DECLARE_DYNAMIC_DELEGATE_ThreeParams(FSkillActiveDelegate, int32, SkillTier, UNiagaraSystem*, CurrentSkillEffect, UNiagaraSystem*, CurrentSkillHitEffect);

UCLASS()
class STRIKEFORGE_API USFCommanderAnim : public UAnimInstance
{
	GENERATED_BODY()
protected:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

public:
	UPROPERTY(BlueprintReadOnly)
	float Speed;
	UPROPERTY(BlueprintReadOnly)
	uint8 bMoving : 1;
	UPROPERTY(BlueprintReadOnly)
	uint8 bSkillFloating : 1;

	FSkillActiveDelegate OnSkillActiveDelegate;
	float SkillDelegateTier;
	TWeakObjectPtr<UNiagaraSystem> CurrentSkillEffect;
	TWeakObjectPtr<UNiagaraSystem> CurrentSkillHitEffect;

protected:
	TWeakObjectPtr<ASFCommander> Commander;
};
