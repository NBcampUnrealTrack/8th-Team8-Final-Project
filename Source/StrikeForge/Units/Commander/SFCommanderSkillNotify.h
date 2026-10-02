// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "SFCommanderSkillNotify.generated.h"

class USFCommanderAnim;

UCLASS()
class STRIKEFORGE_API USFCommanderSkillNotify : public UAnimNotifyState
{
	GENERATED_BODY()
protected:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

protected:
	TWeakObjectPtr<USFCommanderAnim> Anim;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Notify Settings")
	uint8 Floating : 1;
};
