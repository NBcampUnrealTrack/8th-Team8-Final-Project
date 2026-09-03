// Fill out your copyright notice in the Description page of Project Settings.


#include "Units/Commander/SFCommander.h"
#include "InGame/Controller/SFInGamePlayerController.h"
#include "Units/Unit/UnitAIController.h"
#include "Components/CapsuleComponent.h"
#include "NavigationSystem.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/OverlapResult.h"
#include "Units/Unit/UnitBase.h"
#include "ActorComponent/Buff/SFBuffComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Buildings/Building/SFBaseBuilding.h"
#include "Buildings/Building/ProductBuilding/SFProductionBuilding.h"
#include "Logging/SFLog.h"

// Sets default values
ASFCommander::ASFCommander()
	: CommandAction(ECommandAction::NONE)
	, bMyCommander(false)
{
	PrimaryActorTick.bCanEverTick = true;
	GetCharacterMovement()->bUseControllerDesiredRotation = true;
	GetCharacterMovement()->RotationRate.Yaw = 720.f;
	AIControllerClass = AUnitAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	BuffComponent = CreateDefaultSubobject<USFBuffComponent>(TEXT("BuffComponent"));
}

FSFSelectableInfo ASFCommander::GetSelectableInfo_Implementation()
{
	FSFSelectableInfo Info;
	Info.SelectableType = ESFSelectableType::Commander;
	Info.DataRowName    = "";      // 행 이름 필드 추가 필요
	Info.PortraitAnimation = PortraitIdleAnimation;
	return Info;
}

void ASFCommander::MoveToLocation_Implementation(const FVector& InDestination)
{
	MoveToAction(InDestination, ECommandAction::NONE);
}

void ASFCommander::StopMove_Implementation()
{
	StopMoving();
}

// Called when the game starts or when spawned
void ASFCommander::BeginPlay()
{
	Super::BeginPlay();
	if (bMyCommander)
	{
		GetWorldTimerManager().SetTimer(
			LoadPCTimer,
			this,
			&ASFCommander::LoadPlayerController,
			1.f,
			true,
			0.f);
	}
}

void ASFCommander::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(LoadPCTimer);
	
	if (CommandAction == ECommandAction::BUILD &&
	EndPlayReason == EEndPlayReason::Destroyed)
	{
		if (ASFInGamePlayerController* SFPC =
			GetWorld()->GetFirstPlayerController<
				ASFInGamePlayerController>())
		{
			SFPC->ClearPendingBuildingRef();
		}
	}
	
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
			&ASFCommander::CheckFailMoving
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
			// add : cached commander
			// IMovable called when clicked commander and clicked floor
			SFPC->RegisterCommander(this);

			GetWorldTimerManager().ClearTimer(LoadPCTimer);
			SF_LOG(Log, "ASFCommander: LoadPlayerController(): Success");
		}
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
	FVector Dest = Center;
	if (BuildBox.IsValid)
	{
		Bounds.Origin = FVector::UpVector * Bounds.BoxExtent.Z;
		//DrawDebugBox(GetWorld(), Center + FVector::UpVector * Bounds.BoxExtent.Z, Bounds.BoxExtent, FQuat::MakeFromRotator(FRotator(0.f, Rotation, 0.f)), FColor::Red, false, 2.f);
		FVector WorldLoc = GetActorLocation() - FVector::UpVector * CapsuleHalfHeight;	// 지휘관의 지면 월드 좌표

		// 1. 지휘관의 월드좌표를 건설 구역 기준 상대좌표로 변환
		FVector LocalLoc = (WorldLoc - Center).RotateAngleAxis(-Rotation, FVector::UpVector);
		SF_LOG(Log, "MoveToBuild: CommanderLocalLoc = %s", *LocalLoc.ToString());

		// 2. 지휘관의 박스형 캡슐 생성
		//DrawDebugBox(GetWorld(), GetActorLocation(), FVector(CapsuleRadius, CapsuleRadius, CapsuleHalfHeight), FColor::Green, false, 5.f);
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
						//GEngine->AddOnScreenDebugMessage(3, 2.f, FColor::Cyan, FString::Printf(TEXT("i: %d"), i));
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
		//DrawDebugSphere(GetWorld(), Center,Bounds.SphereRadius,16, FColor::Red, false, 2.f);
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
	//DrawDebugPoint(GetWorld(), Dest, 20.f, FColor::Red, false, 2.f);
	if (CheckCanBuild())
	{
		BeginBuild();
	}
	else
	{
		MoveToAction(Dest, ECommandAction::BUILD);
	}
}

void ASFCommander::CheckFailMoving(bool IsSuccessed)
{
	// ★CHANGED: 건설 명령이 아니면 관심 없음 (조기 반환으로 중첩 제거)
	if (CommandAction != ECommandAction::BUILD)
	{
		return;
	}

	// ★ADD: 성공/중단은 실패가 아니다.
	//   - 도착 성공  → Tick 의 CheckCanBuild() 가 건설을 시작한다
	//   - 중단(Aborted) → 새 이동 요청이 이어받은 것이므로 건드리면 안 된다
	AUnitAIController* AI = GetController<AUnitAIController>();
	const bool bStillMoving = IsValid(AI) && AI->IsMoving();

	if (IsSuccessed || bStillMoving)
	{
		return;
	}

	// 여기부터가 진짜 도달 실패 (경로 막힘 / NavMesh 밖)
	PrintMessage(TEXT("Build Error!"));

	if (ASFInGamePlayerController* SFPC = GetWorld()->GetFirstPlayerController<ASFInGamePlayerController>())
	{
		SFPC->ClearPendingBuildingRef();
	}

	StopMoving();
}

void ASFCommander::MoveToAction(FVector Dest, ECommandAction OutAction)
{
	if (CommandAction == ECommandAction::BUILD && OutAction != ECommandAction::BUILD)
	{
		if (ASFInGamePlayerController* SFPC = GetWorld()->GetFirstPlayerController<ASFInGamePlayerController>())
		{
			SFPC->ClearPendingBuildingRef();
		}
	}
	
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
	//GEngine->AddOnScreenDebugMessage(1, 2.f, FColor::Magenta, FString::Printf(TEXT("BuildDistance: %f"), Dist));
	const FBox BuildBox = BuildBoundary.GetBox();
	if (BuildBox.IsValid)
	{
		FVector WorldLoc = GetActorLocation() - FVector::UpVector * CapsuleHalfHeight;	// 지휘관의 지면 월드 좌표
		FVector LocalLoc = (WorldLoc - BuildLocation).RotateAngleAxis(-BuildRotation, FVector::UpVector);	// 지휘관의 건설구역 기준 상대위치

		//DrawDebugBox(GetWorld(), LocalLoc + FVector::UpVector * CapsuleHalfHeight, FVector(CapsuleRadius, CapsuleRadius, CapsuleHalfHeight), FColor::Magenta, false, 0.f);
		//DrawDebugBox(GetWorld(), FVector::ZeroVector + FVector::UpVector * BuildBox.GetExtent().Z, BuildBox.GetExtent(), FColor::Emerald, false, 2.f);
		float EdgeDistFromBound;
		if (BuildBox.IsInsideOrOn(LocalLoc + FVector::UpVector * BuildBox.GetExtent().Z))
		{
			EdgeDistFromBound = 0.f;
		}
		else
		{
			EdgeDistFromBound = FVector::Dist(LocalLoc, BuildBox.GetClosestPointTo(LocalLoc));
		}
		//GEngine->AddOnScreenDebugMessage(2, 5.f, FColor::Cyan, FString::Printf(TEXT("EdgeDistFromBound: %f"), EdgeDistFromBound));
		if (EdgeDistFromBound > CapsuleRadius && EdgeDistFromBound <= CapsuleRadius + MaxDistToBuild)
		{
			//DrawDebugBox(GetWorld(), BuildLocation + FVector::UpVector * BuildBoundary.BoxExtent.Z, BuildBoundary.BoxExtent, FQuat::MakeFromRotator(FRotator(0.f, BuildRotation, 0.f)), FColor::Blue, false, 2.f, 0U, 20.f);
			return true;
		}
	}
	else
	{
		if (Dist > BuildBoundary.SphereRadius && Dist <= BuildBoundary.SphereRadius + MaxDistToBuild)
		{
			//DrawDebugSphere(GetWorld(), BuildLocation, BuildBoundary.SphereRadius, 16, FColor::Blue, false, 2.f, 0U, 20.f);
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

	// Notify To Controller That Commander Arrived Location
	if (ASFInGamePlayerController* SFPC = GetWorld()->GetFirstPlayerController<ASFInGamePlayerController>())
	{
		// Begin Constructing Building in this Function
		SFPC->NotifyBuilderArrived();
	}
	
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

void ASFCommander::ActiveCommanderSkill(int32 SkillTier)
{
	TWeakObjectPtr<UWorld> World = GetWorld();
	if (!World.IsValid())
	{
		SF_LOG(Error, "ASFCommander: World Is Invalid.");
		return;
	}

	switch (Race)
	{
	case ERace::BEAST:
		ActiveCommanderSkill_Beast(SkillTier, World);
		break;
	case ERace::ETHERIAN:
		ActiveCommanderSkill_Ether(SkillTier, World);
		break;
	}
}

void ASFCommander::ActiveCommanderSkill_Beast(int32 SkillTier, TWeakObjectPtr<UWorld> World)
{
	switch (SkillTier)
	{
	case 1:
		//1티어 스킬- 범위 내 아군 이속증가
		do
		{
			/** 버프 정보 */

			// TODO : 현재 수치를 임의로 기재한 상태. 필요에 맞게 변경해주세요.
			const FString BuffKey = TEXT("SpeedUp"); // 버프 키 (중복방지)
			const FText BuffText = FText::FromString(TEXT("돌격명령"));	// 버프 UI표시명
			const float BuffTime = 5.f;	// 버프 지속시간(초)
			const float BuffInterval = 0.f;	// 효과 발동 주기 (0이면 매 프레임 발동)

			const float SkillRadius = 500.f; // 버프가 적용되는 스킬 반경
			//-------------------

			// 스킬 영향권에 들어온 액터 가져오기 (USFBuffComponent를 가지고 있는 Pawn 콜리전인 액터에 한함)
			TArray<FBuffTarget> Targets = GetSkillActors(World, SkillRadius);

			for (FBuffTarget TG : Targets)
			{
				// TODO : 현재 커맨더에게만 버프를 부여하고 있는 상태. [커맨더를 포함한 '아군' 유닛], 혹은 [특정 컴포넌트 기능을 가진 액터] 만을 필터링하도록 코드를 변경해주세요.
				TWeakObjectPtr<ASFCommander> Target = Cast<ASFCommander>(TG.ActorPtr);

				if (Target.IsValid())
				{
					// 버프 부여
					TG.BuffCompPtr->AddBuff(BuffKey, BuffText,
						[Target]()	// BuffInterval 주기로 반복 실행(Tick)
						{
							// TODO: 틱 단위 함수 작성
							GEngine->AddOnScreenDebugMessage(1, 0.f, FColor::White, FString::Printf(TEXT("[%s] SpeedUp Buff Tick..."), *Target->GetName()));
						},
						[Target]()	// 버프 시작 시 실행(Begin)
						{
							// TODO: 버프 시작 시 함수 작성
							GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan, FString::Printf(TEXT("[%s] SpeedUp Buff Begin"), *Target->GetName()));
						},
						[Target]()	// 버프 종료 시 실행(End)
						{
							// TODO: 버프 종료 시 함수 작성
							GEngine->AddOnScreenDebugMessage(1, 5.f, FColor::Emerald, FString::Printf(TEXT("[%s] SpeedUp Buff End"), *Target->GetName()));
						},
						BuffTime,
						BuffInterval);
				}
			}
		} while (false);
		break;
	case 2:
		//2티어 스킬- 공방증가
		do
		{
			/** 버프 정보 */

			// TODO : 현재 수치를 임의로 기재한 상태. 필요에 맞게 변경해주세요.
			const FString BuffKey = TEXT("AtkDefUp"); // 버프 키 (중복방지)
			const FText BuffText = FText::FromString(TEXT("전장의 포효"));	// 버프 UI표시명
			const float BuffTime = 10.f;	// 버프 지속시간(초)
			const float BuffInterval = 0.f;	// 효과 발동 주기 (0이면 매 프레임 발동)

			const float SkillRadius = 500.f; // 버프가 적용되는 스킬 반경
			//-------------------

			// 스킬 영향권에 들어온 액터 가져오기 (USFBuffComponent를 가지고 있는 Pawn 콜리전인 액터에 한함)
			TArray<FBuffTarget> Targets = GetSkillActors(World, SkillRadius);

			for (FBuffTarget TG : Targets)
			{
				// TODO : 현재 커맨더에게만 버프를 부여하고 있는 상태. [커맨더를 포함한 '아군' 유닛], 혹은 [특정 컴포넌트 기능을 가진 액터] 만을 필터링하도록 코드를 변경해주세요.
				TWeakObjectPtr<ASFCommander> Target = Cast<ASFCommander>(TG.ActorPtr);

				if (Target.IsValid())
				{
					// 버프 부여
					TG.BuffCompPtr->AddBuff(BuffKey, BuffText,
						[Target]()	// BuffInterval 주기로 반복 실행(Tick)
						{
							// TODO: 틱 단위 함수 작성
							GEngine->AddOnScreenDebugMessage(2, 0.f, FColor::White, FString::Printf(TEXT("[%s] AtkDefUp Buff Tick..."), *Target->GetName()));
						},
						[Target]()	// 버프 시작 시 실행(Begin)
						{
							// TODO: 버프 시작 시 함수 작성
							GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan, FString::Printf(TEXT("[%s] AtkDefUp Buff Begin"), *Target->GetName()));
						},
						[Target]()	// 버프 종료 시 실행(End)
						{
							// TODO: 버프 종료 시 함수 작성
							GEngine->AddOnScreenDebugMessage(2, 5.f, FColor::Emerald, FString::Printf(TEXT("[%s] AtkDefUp Buff End"), *Target->GetName()));
						},
						BuffTime,
						BuffInterval);
				}
			}
		} while (false);
		break;
	case 3:
		//3티어 스킬- 적 받는피해 증가
		do
		{
			/** 버프 정보 */

			// TODO : 현재 수치를 임의로 기재한 상태. 필요에 맞게 변경해주세요.
			const FString BuffKey = TEXT("ReceiveDmgUp"); // 버프 키 (중복방지)
			const FText BuffText = FText::FromString(TEXT("위압"));	// 버프 UI표시명
			const float BuffTime = 10.f;	// 버프 지속시간(초)
			const float BuffInterval = 0.f;	// 효과 발동 주기 (0이면 매 프레임 발동)

			const float SkillRadius = 750.f; // 버프가 적용되는 스킬 반경
			//-------------------

			// 스킬 영향권에 들어온 액터 가져오기 (USFBuffComponent를 가지고 있는 Pawn 콜리전인 액터에 한함)
			TArray<FBuffTarget> Targets = GetSkillActors(World, SkillRadius);

			for (FBuffTarget TG : Targets)
			{
				// TODO : 현재 커맨더에게만 버프를 부여하고 있는 상태. [커맨더를 포함한 '적군' 유닛], [적군 건물], 혹은 [특정 컴포넌트 기능을 가진 액터] 만을 필터링하도록 코드를 변경해주세요.
				TWeakObjectPtr<ASFCommander> Target = Cast<ASFCommander>(TG.ActorPtr);

				if (Target.IsValid())
				{
					// 버프 부여
					TG.BuffCompPtr->AddBuff(BuffKey, BuffText,
						[Target]()	// BuffInterval 주기로 반복 실행(Tick)
						{
							// TODO: 틱 단위 함수 작성
							GEngine->AddOnScreenDebugMessage(3, 0.f, FColor::White, FString::Printf(TEXT("[%s] ReceiveDmgUp Buff Tick..."), *Target->GetName()));
						},
						[Target]()	// 버프 시작 시 실행(Begin)
						{
							// TODO: 버프 시작 시 함수 작성
							GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan, FString::Printf(TEXT("[%s] ReceiveDmgUp Buff Begin"), *Target->GetName()));
						},
						[Target]()	// 버프 종료 시 실행(End)
						{
							// TODO: 버프 종료 시 함수 작성
							GEngine->AddOnScreenDebugMessage(3, 5.f, FColor::Emerald, FString::Printf(TEXT("[%s] ReceiveDmgUp Buff End"), *Target->GetName()));
						},
						BuffTime,
						BuffInterval);
				}
			}
		} while (false);
		break;
	}
}

void ASFCommander::ActiveCommanderSkill_Ether(int32 SkillTier, TWeakObjectPtr<UWorld> World)
{
	switch (SkillTier)
	{
	case 1:
		//1티어 스킬- 극 소량 체젠 업
		do
		{
			/** 버프 정보 */

			// TODO : 현재 수치를 임의로 기재한 상태. 필요에 맞게 변경해주세요.
			const FString BuffKey = TEXT("HealUp"); // 버프 키 (중복방지)
			const FText BuffText = FText::FromString(TEXT("성역의 축복"));	// 버프 UI표시명
			const float BuffTime = 30.f;	// 버프 지속시간(초)
			const float BuffInterval = 1.f;	// 효과 발동 주기 (0이면 매 프레임 발동)

			const float SkillRadius = 2000.f; // 버프가 적용되는 스킬 반경
			//-------------------

			// 스킬 영향권에 들어온 액터 가져오기 (USFBuffComponent를 가지고 있는 Pawn 콜리전인 액터에 한함)
			TArray<FBuffTarget> Targets = GetSkillActors(World, SkillRadius);

			for (FBuffTarget TG : Targets)
			{
				// TODO : 현재 커맨더에게만 버프를 부여하고 있는 상태. [커맨더를 포함한 '아군' 유닛], 혹은 [특정 컴포넌트 기능을 가진 액터] 만을 필터링하도록 코드를 변경해주세요.
				TWeakObjectPtr<ASFCommander> Target = Cast<ASFCommander>(TG.ActorPtr);

				if (Target.IsValid())
				{
					// 버프 부여
					TG.BuffCompPtr->AddBuff(BuffKey, BuffText,
						[Target]()	// BuffInterval 주기로 반복 실행(Tick)
						{
							// TODO: 틱 단위 함수 작성
							GEngine->AddOnScreenDebugMessage(4, 0.f, FColor::White, FString::Printf(TEXT("[%s] HealUp Buff Tick..."), *Target->GetName()));
						},
						[Target]()	// 버프 시작 시 실행(Begin)
						{
							// TODO: 버프 시작 시 함수 작성
							GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan, FString::Printf(TEXT("[%s] HealUp Buff Begin"), *Target->GetName()));
						},
						[Target]()	// 버프 종료 시 실행(End)
						{
							// TODO: 버프 종료 시 함수 작성
							GEngine->AddOnScreenDebugMessage(4, 5.f, FColor::Emerald, FString::Printf(TEXT("[%s] HealUp Buff End"), *Target->GetName()));
						},
						BuffTime,
						BuffInterval);
				}
			}
		} while (false);
		break;
	case 2:
		//2티어 스킬- 모든 건물 생산속도 증가
		do
		{
			/** 버프 정보 */

			// TODO : 현재 수치를 임의로 기재한 상태. 필요에 맞게 변경해주세요.
			const FString BuffKey = TEXT("ProdSpeedUp");
			const FText BuffText = FText::FromString(TEXT("빨리빨리"));

			const float BuffDuration = 10.f;
			const float ProductionSpeedMultiplier = 2.f;
			const float SkillRadius = 2000.f;
			const float SkillRadiusSquared = FMath::Square(SkillRadius);
			
			DrawDebugSphere(GetWorld(), GetActorLocation(), SkillRadius, -1,FColor::Red,false, 3.f,0,2);

			const ASFInGamePlayerController* SFPC =
					World->GetFirstPlayerController<ASFInGamePlayerController>();
			if (!IsValid(SFPC)) return;
			
			const int32 CasterTeamIndex = SFPC->GetMyTeamIndex();
			
			// 월드 내 모든 액터 가져오기 ()
			TArray<FBuffTarget> Targets = GetSkillActors(World, ASFProductionBuilding::StaticClass());

			for (FBuffTarget TG : Targets)
			{
				// TODO : 현재 건물에게만 버프를 부여하고 있는 상태. ['아군' 건물] 만을 필터링하도록 코드를 변경해주세요.
				TWeakObjectPtr<ASFProductionBuilding> Target = Cast<ASFProductionBuilding>(TG.ActorPtr);
				if (Target.IsValid() && Target->GetTeamIndex() == CasterTeamIndex)
				{
					// 버프 부여
					TG.BuffCompPtr->AddBuff(BuffKey, BuffText,
						[Target]()	// BuffInterval 주기로 반복 실행(Tick)
						{
							// TODO: 틱 단위 함수 작성
							GEngine->AddOnScreenDebugMessage(5, 0.f, FColor::White, FString::Printf(TEXT("[%s] ProdSpeedUp Buff Tick..."), *Target->GetName()));
						},
						[Target, ProductionSpeedMultiplier]()	// 버프 시작 시 실행(Begin)
						{
							// TODO: 버프 시작 시 함수 작성
							GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan, FString::Printf(TEXT("[%s] ProdSpeedUp Buff Begin"), *Target->GetName()));
							Target->ApplySpeedBuff(ProductionSpeedMultiplier);
						},
						[Target]()	// 버프 종료 시 실행(End)
						{
							// TODO: 버프 종료 시 함수 작성
							GEngine->AddOnScreenDebugMessage(5, 5.f, FColor::Emerald, FString::Printf(TEXT("[%s] ProdSpeedUp Buff End"), *Target->GetName()));
							Target->ResetSpeedMultiplier();
						},
						BuffDuration,
						1.f);
				}
			}
		} while (false);
		break;
	case 3:
		//3티어 스킬- 아군 받는피해 감소
		do
		{
			/** 버프 정보 */

			// TODO : 현재 수치를 임의로 기재한 상태. 필요에 맞게 변경해주세요.
			const FString BuffKey = TEXT("ReceiveDmgDown"); // 버프 키 (중복방지)
			const FText BuffText = FText::FromString(TEXT("천상의 수호"));	// 버프 UI표시명
			const float BuffTime = 5.f;	// 버프 지속시간(초)
			const float BuffInterval = 0.f;	// 효과 발동 주기 (0이면 매 프레임 발동)

			const float SkillRadius = 500.f; // 버프가 적용되는 스킬 반경
			//-------------------

			// 스킬 영향권에 들어온 액터 가져오기 (USFBuffComponent를 가지고 있는 Pawn 콜리전인 액터에 한함)
			TArray<FBuffTarget> Targets = GetSkillActors(World, SkillRadius);

			for (FBuffTarget TG : Targets)
			{
				// TODO : 현재 커맨더에게만 버프를 부여하고 있는 상태. [커맨더를 포함한 '아군' 유닛], [아군 건물], 혹은 [특정 컴포넌트 기능을 가진 액터] 만을 필터링하도록 코드를 변경해주세요.
				TWeakObjectPtr<ASFCommander> Target = Cast<ASFCommander>(TG.ActorPtr);

				if (Target.IsValid())
				{
					// 버프 부여
					TG.BuffCompPtr->AddBuff(BuffKey, BuffText,
						[Target]()	// BuffInterval 주기로 반복 실행(Tick)
						{
							// TODO: 틱 단위 함수 작성
							GEngine->AddOnScreenDebugMessage(6, 0.f, FColor::White, FString::Printf(TEXT("[%s] ReceiveDmgDown Buff Tick..."), *Target->GetName()));
						},
						[Target]()	// 버프 시작 시 실행(Begin)
						{
							// TODO: 버프 시작 시 함수 작성
							GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan, FString::Printf(TEXT("[%s] ReceiveDmgDown Buff Begin"), *Target->GetName()));
						},
						[Target]()	// 버프 종료 시 실행(End)
						{
							// TODO: 버프 종료 시 함수 작성
							GEngine->AddOnScreenDebugMessage(6, 5.f, FColor::Emerald, FString::Printf(TEXT("[%s] ReceiveDmgDown Buff End"), *Target->GetName()));
						},
						BuffTime,
						BuffInterval);
				}
			}
		} while (false);
		break;
	}
}

TArray<FBuffTarget> ASFCommander::GetSkillActors(TWeakObjectPtr<UWorld> World, float Radius)
{
	TArray<FBuffTarget> OutTargets;

	TArray<FOverlapResult> HitResults;
	FCollisionShape ColShape;
	ColShape.SetSphere(Radius);
	World->OverlapMultiByChannel(HitResults, GetActorLocation(), FQuat::Identity, ECC_Pawn, ColShape);
	DrawDebugSphere(World.Get(), GetActorLocation(), Radius, 16, FColor::Red, false, 5.f);

	for (FOverlapResult OverlapResult : HitResults)
	{
		if (IsValid(OverlapResult.GetActor()))
		{
			TWeakObjectPtr<USFBuffComponent> BuffComp = OverlapResult.GetActor()->GetComponentByClass<USFBuffComponent>();
			if (BuffComp.IsValid() && IsValid(BuffComp->GetOwner()))
			{
				OutTargets.Emplace(BuffComp->GetOwner(), BuffComp);
			}
		}
	}
	return OutTargets;
}

TArray<FBuffTarget> ASFCommander::GetSkillActors(TWeakObjectPtr<UWorld> World, TSubclassOf<AActor> ActorClass)
{
	TArray<FBuffTarget> OutTargets;

	TArray<AActor*> Actors;
	UGameplayStatics::GetAllActorsOfClass(World.Get(), ActorClass, Actors);

	for (AActor* Actor : Actors)
	{
		if (IsValid(Actor))
		{
			TWeakObjectPtr<USFBuffComponent> BuffComp = Actor->GetComponentByClass<USFBuffComponent>();
			if (BuffComp.IsValid() && IsValid(BuffComp->GetOwner()))
			{
				OutTargets.Emplace(BuffComp->GetOwner(), BuffComp);
			}
		}
	}
	return OutTargets;
}
