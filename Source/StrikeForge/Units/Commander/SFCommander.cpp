// Fill out your copyright notice in the Description page of Project Settings.


#include "Units/Commander/SFCommander.h"
#include "InGame/Controller/SFInGamePlayerController.h"
#include "Units/Unit/UnitAIController.h"
#include "Components/CapsuleComponent.h"
#include "NavigationSystem.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Logging/SFLog.h"

// Sets default values
ASFCommander::ASFCommander()
	: CommandAction(ECommandAction::NONE)
{
	PrimaryActorTick.bCanEverTick = true;
	GetCharacterMovement()->bUseControllerDesiredRotation = true;
	GetCharacterMovement()->RotationRate.Yaw = 720.f;
}

// Called when the game starts or when spawned
void ASFCommander::BeginPlay()
{
	Super::BeginPlay();
	GetWorldTimerManager().SetTimer(
		LoadPCTimer,
		this,
		&ASFCommander::LoadPlayerController,
		1.f,
		true,
		0.f);
}

void ASFCommander::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(LoadPCTimer);
	Super::EndPlay(EndPlayReason);
}

void ASFCommander::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (CommandAction == ECommandAction::BUILD)
	{
		// 건설이 가능하면 캐릭터를 멈추고 건설시작.
		if (CheckCanBuild())
		{
			StopMoving();
			BeginBuild();
		}
	}
}

void ASFCommander::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	if (IsValid(GetController<AUnitAIController>()))
	{
		GetController<AUnitAIController>()->OnCompleteMoving.AddUniqueDynamic(
			this,
			&ASFCommander::OnFailMoving
		);
	}
}

void ASFCommander::LoadPlayerController()
{
	SF_LOG(Log, "ASFCommander: LoadPlayerController()");
	if (GetWorld() && GetWorld()->IsInitialized())
	{
		if (ASFInGamePlayerController* SFPC = GetWorld()->GetFirstPlayerController<ASFInGamePlayerController>())
		{
			SFPC->OnMoveInput.Clear();
			SFPC->OnMoveInput.AddUniqueDynamic(
				this,
				&ASFCommander::MoveToDest
			);
			GetWorldTimerManager().ClearTimer(LoadPCTimer);
			SF_LOG(Log, "ASFCommander: LoadPlayerController(): Success");
		}
	}
}

void ASFCommander::MoveToDest(FVector Dest)
{
	//MoveToAction(Dest, ECommandAction::NONE);
	if (TestMesh != nullptr)
	{
		FBoxSphereBounds MeshBounds = TestMesh->GetBounds();
		MeshBounds.BoxExtent *= 5.f;
		MeshBounds.SphereRadius *= 5.f;
		MoveToBuild(Dest, MeshBounds, FMath::RandRange(0.f,360.f));
	}
}

void ASFCommander::MoveToBuild(FVector Center, FBoxSphereBounds Bounds, float Rotation)
{
	float CapsuleHalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	float CapsuleRadius = GetCapsuleComponent()->GetScaledCapsuleRadius();

	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
	if (!NavSys || !NavSys->GetDefaultNavDataInstance()) return;
	FVector QueryExt(0.1f, 0.1f, 1000.f);
	FNavLocation OutLoc;

	const FBox BuildBox = Bounds.GetBox();
	FVector Dest;
	if (BuildBox.IsValid)
	{
		Bounds.Origin = FVector::UpVector * Bounds.BoxExtent.Z;
		DrawDebugBox(GetWorld(), Center + FVector::UpVector * Bounds.BoxExtent.Z, Bounds.BoxExtent, FQuat::MakeFromRotator(FRotator(0.f, Rotation, 0.f)), FColor::Red, false, 2.f);
		FVector WorldLoc = GetActorLocation() - FVector::UpVector * CapsuleHalfHeight;	// 지휘관의 지면 월드 좌표

		// 1. 지휘관의 월드좌표를 건설 구역 기준 상대좌표로 변환
		FVector LocalLoc = (WorldLoc - Center).RotateAngleAxis(-Rotation, FVector::UpVector);
		SF_LOG(Log, "MoveToBuild: CommanderLocalLoc = %s", *LocalLoc.ToString());

		// 2. 지휘관의 박스형 캡슐 생성
		DrawDebugBox(GetWorld(), GetActorLocation(), FVector(CapsuleRadius, CapsuleRadius, CapsuleHalfHeight), FColor::Green, false, 5.f);
		FBox CommanderBox = FBox::BuildAABB(LocalLoc + FVector::UpVector * CapsuleHalfHeight, FVector(CapsuleRadius, CapsuleRadius, CapsuleHalfHeight));

		SF_LOG(Log, "MoveToBuild: BuildBox={Center:%s, Extents:%s}", *BuildBox.GetCenter().ToString(), *BuildBox.GetExtent().ToString());
		SF_LOG(Log, "MoveToBuild: CommanderBox={Center:%s, Extents:%s}", *CommanderBox.GetCenter().ToString(), *CommanderBox.GetExtent().ToString());
		// 3. 지휘관과 건설구역과의 충돌여부 판별
		if (BuildBox.Intersect(CommanderBox))
		{
			if (BuildBox.IsInsideOrOn(LocalLoc + FVector::UpVector * BuildBox.GetExtent().Z))
			{
				// 3-1. 지휘관이 건설구역 안으로 캡슐 중심까지 들어옴
				SF_LOG(Log, "MoveToBuild: Commander is Inside The Box.");

				// 안쪽에서 경계와 가장 가까운 방향 구하기
				TArray<FVector> Dists;
				Dists.Add(FVector::ForwardVector * (Bounds.BoxExtent.X - LocalLoc.X));	// X-Positive
				Dists.Add(FVector::ForwardVector * (-Bounds.BoxExtent.X - LocalLoc.X));	// X-Negative
				Dists.Add(FVector::RightVector * (Bounds.BoxExtent.Y - LocalLoc.Y));	// Y-Positive
				Dists.Add(FVector::RightVector * (-Bounds.BoxExtent.Y - LocalLoc.Y));	// Y-Negative
				Dists.Sort([](FVector a, FVector b) {return FMath::Abs(a.Length()) < FMath::Abs(b.Length()); });	// 가장 거리가 짧은 방향으로

				// 도착점 계산
				bool bValidDest = false;
				for (int32 i = 0; i < Dists.Num(); i++)
				{
					FVector WorldDir = ((LocalLoc + Dists[i]).RotateAngleAxis(Rotation, FVector::UpVector)).GetSafeNormal2D();	// 월드방향 기준 건설구역 경계와 가장 가까운 방향
					FVector TempDest = WorldLoc + WorldDir * (Dists[i].Length() + CapsuleRadius * 2.f + MaxDistToBuild + MoveRqAcceptRadius * 2.f);

					// 도착점이 유효한 지점인가?
					if (NavSys->ProjectPointToNavigation(TempDest, OutLoc, QueryExt))
					{
						bValidDest = true;

						// 도착점 확정
						Dest = OutLoc;
						GEngine->AddOnScreenDebugMessage(3, 2.f, FColor::Cyan, FString::Printf(TEXT("i: %d"), i));
						break;
					}
				}
				if (!bValidDest)
				{
					PrintMessage(TEXT("Build Error!"));
				}
			}
			else
			{
				// 3-2. 지휘관이 건설 구역과 겹쳐있음
				SF_LOG(Log, "MoveToBuild: Commander has Overlapped The Box.");

				// 바깥쪽에서 경계와 가장 가까운 방향 구하기
				FVector ClosetLocalLoc = BuildBox.GetClosestPointTo(LocalLoc);
				FVector WorldDir = (LocalLoc - ClosetLocalLoc).RotateAngleAxis(Rotation, FVector::UpVector).GetSafeNormal2D();
				float Dist = FVector::Dist(ClosetLocalLoc, LocalLoc);
				FVector TempDest = WorldLoc + WorldDir * (CapsuleRadius * 2.f - Dist + MaxDistToBuild + MoveRqAcceptRadius * 2.f);

				// 도착점이 유효한 지점인가?
				if (NavSys->ProjectPointToNavigation(TempDest, OutLoc, QueryExt))
				{
					// 도착점 확정
					Dest = OutLoc;
				}
				else
				{
					PrintMessage(TEXT("Build Error!"));
				}
			}
		}
		else
		{
			SF_LOG(Log, "MoveToBuild: Commander is Outside The Box.");
			Dest = Center;
		}
	}
	else
	{
		Bounds.Origin = FVector::ZeroVector;
		DrawDebugSphere(GetWorld(), Center,Bounds.SphereRadius,16, FColor::Red, false, 2.f);
		FVector WorldLoc = GetActorLocation() - FVector::UpVector * CapsuleHalfHeight;	// 지휘관의 지면 월드 좌표
		if (Bounds.GetSphere().IsInside(Dest))
		{
			SF_LOG(Log, "MoveToBuild: Commander is Outside The Sphere.");
			FVector Dir = (WorldLoc - Center).GetUnsafeNormal2D();
			float Dist = Bounds.SphereRadius - FVector::Dist2D(WorldLoc, Center);
			Dest = WorldLoc + Dir * (Dist + MaxDistToBuild);
		}
		else
		{
			SF_LOG(Log, "MoveToBuild: Commander is Outside The Sphere.");
			Dest = Center;
		}
	}

	SF_LOG(Log, "MoveToBuild: Dest=%s", *Dest.ToString());
	BuildLocation = Center;
	BuildRotation = Rotation;
	BuildBoundary = Bounds;
	DrawDebugPoint(GetWorld(), Dest, 10.f, FColor::Red, false, 20.f);
	if (CheckCanBuild())
	{
		BeginBuild();
	}
	else
	{
		MoveToAction(Dest, ECommandAction::BUILD);
	}
}

void ASFCommander::OnFailMoving(bool IsSuccessed)
{
	if (CommandAction == ECommandAction::BUILD)
	{
		PrintMessage(TEXT("Build Error!"));
		StopMoving();
	}
}

void ASFCommander::MoveToAction(FVector Dest, ECommandAction OutAction)
{
	CommandAction = OutAction;
	if (IsValid(GetCharacterMovement()))
	{
		GetCharacterMovement()->bOrientRotationToMovement = true;
	}

	AUnitAIController* OwnerController = GetController<AUnitAIController>();
	if (!IsValid(OwnerController))
	{
		return;
	}

	OwnerController->RequestMoveToLocation(Dest, MoveRqAcceptRadius);
}

bool ASFCommander::CheckCanBuild()
{
	float Dist = FVector::Dist(GetActorLocation(), BuildLocation);
	float CapsuleHalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	float CapsuleRadius = GetCapsuleComponent()->GetScaledCapsuleRadius();
	GEngine->AddOnScreenDebugMessage(1, 2.f, FColor::Magenta, FString::Printf(TEXT("BuildDistance: %f"), Dist));
	const FBox BuildBox = BuildBoundary.GetBox();
	if (BuildBox.IsValid)
	{
		FVector WorldLoc = GetActorLocation() - FVector::UpVector * CapsuleHalfHeight;	// 지휘관의 지면 월드 좌표
		FVector LocalLoc = (WorldLoc - BuildLocation).RotateAngleAxis(-BuildRotation, FVector::UpVector);	// 지휘관의 건설구역 기준 상대위치

		DrawDebugBox(GetWorld(), LocalLoc + FVector::UpVector * CapsuleHalfHeight, FVector(CapsuleRadius, CapsuleRadius, CapsuleHalfHeight), FColor::Magenta, false, 0.f);
		DrawDebugBox(GetWorld(), FVector::ZeroVector + FVector::UpVector * BuildBox.GetExtent().Z, BuildBox.GetExtent(), FColor::Emerald, false, 2.f);
		float EdgeDistFromBound;
		if (BuildBox.IsInsideOrOn(LocalLoc + FVector::UpVector * BuildBox.GetExtent().Z))
		{
			EdgeDistFromBound = 0.f;
		}
		else
		{
			EdgeDistFromBound = FVector::Dist(LocalLoc, BuildBox.GetClosestPointTo(LocalLoc));
		}
		GEngine->AddOnScreenDebugMessage(2, 5.f, FColor::Cyan, FString::Printf(TEXT("EdgeDistFromBound: %f"), EdgeDistFromBound));
		if (EdgeDistFromBound > CapsuleRadius && EdgeDistFromBound <= CapsuleRadius + MaxDistToBuild)
		{
			DrawDebugBox(GetWorld(), BuildLocation + FVector::UpVector * BuildBoundary.BoxExtent.Z, BuildBoundary.BoxExtent, FQuat::MakeFromRotator(FRotator(0.f, BuildRotation, 0.f)), FColor::Blue, false, 2.f, 0U, 20.f);
			return true;
		}
	}
	else
	{
		if (Dist > BuildBoundary.SphereRadius && Dist <= BuildBoundary.SphereRadius + MaxDistToBuild)
		{
			DrawDebugSphere(GetWorld(), BuildLocation, BuildBoundary.SphereRadius, 16, FColor::Blue, false, 2.f, 0U, 20.f);
			return true;
		}
	}
	return false;
}

void ASFCommander::BeginBuild()
{
	// 건설부지를 향해 제자리 회전
	if (IsValid(GetController()) && IsValid(GetCharacterMovement()))
	{
		FVector Dir = BuildLocation - GetActorLocation();
		
		GetCharacterMovement()->bOrientRotationToMovement = false;
		GetController()->SetControlRotation(Dir.Rotation());
	}

	// TODO : 빌딩관련 함수를 적어주세요.
}

void ASFCommander::PrintMessage(FString Msg)
{
	// TODO : Msg 출력값을 UI와 연동해주세요. 현재 이곳에는 건설시도 실패 메세지가 주로 들어갑니다.
	GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Red, Msg);
}

void ASFCommander::StopMoving()
{
	CommandAction = ECommandAction::NONE;

	if (IsValid(GetCharacterMovement()))
	{
		GetCharacterMovement()->bOrientRotationToMovement = false;
	}

	AUnitAIController* OwnerController = GetController<AUnitAIController>();
	if (!IsValid(OwnerController))
	{
		return;
	}

	OwnerController->ClearMoveRequest();
}
