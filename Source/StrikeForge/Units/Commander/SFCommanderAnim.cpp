// Fill out your copyright notice in the Description page of Project Settings.


#include "Units/Commander/SFCommanderAnim.h"
#include "Units/Commander/SFCommander.h"
#include "Units/Unit/UnitAIController.h"
#include "Kismet/GameplayStatics.h"

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
	if (!UnitAIController.IsValid())
	{
		UnitAIController = Cast<AUnitAIController>(Commander->GetController());
		if (!UnitAIController.IsValid())
		{
			return;
		}
	}

	bMoving = UnitAIController->IsMoving();

	if (Commander.IsValid())
	{
		FVector Velocity = Commander->GetVelocity();
		Velocity.Z = 0.f;
		float Scale = Commander->GetActorScale().X;

		Speed = Velocity.Length() / Scale;
	}
}