// Fill out your copyright notice in the Description page of Project Settings.


#include "Units/Commander/SFCommanderSkillNotify.h"
#include "Units/Commander/SFCommanderAnim.h"
#include "Units/Commander/SFCommander.h"

void USFCommanderSkillNotify::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	if (!IsValid(MeshComp))
	{
		return;
	}

	Anim = Cast<USFCommanderAnim>(MeshComp->GetAnimInstance());
	if (!Anim.IsValid())
	{
		return;
	}

	Anim->OnSkillActiveDelegate.Execute(Anim->SkillDelegateTier);
	Anim->OnSkillActiveDelegate.Unbind();

	if (Floating)
	{
		Anim->bSkillFloating = true;
	}
}

void USFCommanderSkillNotify::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
	if (!Anim.IsValid())
	{
		return;
	}

	Anim->bSkillFloating = false;
}
