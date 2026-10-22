// Fill out your copyright notice in the Description page of Project Settings.


#include "Units/Commander/SFCommanderAnim.h"
#include "Units/Commander/SFCommander.h"

void USFCommanderAnim::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	
	Commander = Cast<ASFCommander>(GetOwningActor());
	if (!Commander.IsValid())
	{
		return;
	}
}

void USFCommanderAnim::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	if (!Commander.IsValid())
	{
		return;
	}
    // AI controllers exist on the server; replicated velocity exists on both peers.
    const float GroundSpeed=Commander->GetVelocity().Size2D();
    Speed=GroundSpeed/FMath::Max(KINDA_SMALL_NUMBER,FMath::Abs(Commander->GetActorScale3D().X));
    bMoving=GroundSpeed>3.f;
}
