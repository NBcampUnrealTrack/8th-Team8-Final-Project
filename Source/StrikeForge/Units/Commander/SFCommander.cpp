// Fill out your copyright notice in the Description page of Project Settings.


#include "Units/Commander/SFCommander.h"
#include "Units/Commander/SFCommanderAnim.h"
#include "Animation/AnimSequence.h"
#include "InGame/Controller/SFInGamePlayerController.h"
#include "Units/Unit/UnitAIController.h"
#include "Components/CapsuleComponent.h"
#include "NavigationSystem.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/OverlapResult.h"
#include "Units/Unit/UnitBase.h"
#include "ActorComponent/Buff/SFBuffComponent.h"
#include "ActorComponent/Combat/CombatTargetComponent.h"
#include "ActorComponent/Stat/HealthComponent.h"
#include "ActorComponent/UI/SFUnitHealthBarComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Buildings/Building/SFBaseBuilding.h"
#include "Buildings/Building/ProductBuilding/SFProductionBuilding.h"
#include "ActorComponent/Combat/CombatTargetComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "Logging/SFLog.h"
#include "Game/GameState/SFGameState.h"
#include "Buildings/Building/HouseBuilding/Nexus/SFNexusBuilding.h"
#include "TimerManager.h"
#include "Game/Audio/SFUnitVoiceSubsystem.h"
#include "Sound/SoundBase.h"

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
	HealthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));
	CombatTargetComponent = CreateDefaultSubobject<UCombatTargetComponent>(TEXT("CombatTargetComponent"));
	CombatTargetComponent->SightRadius = 1400.f;
	CreateDefaultSubobject<USFUnitHealthBarComponent>(TEXT("DamageHealthBar"))->SetupAttachment(RootComponent);
}

void ASFCommander::InitializePlayerCommander(int32 TeamIndex)
{
	bMyCommander = true;
	CombatTargetComponent->SetTeamIndex(TeamIndex);
}

FSFSelectableInfo ASFCommander::GetSelectableInfo_Implementation()
{
	FSFSelectableInfo Info;
	Info.SelectableType = ESFSelectableType::Commander;
	Info.DataRowName    = TEXT("Commander");
	
	if (IsValid(HealthComponent))
	{
		Info.CurrentHealth = HealthComponent->GetCurrentHealth();
		Info.MaximumHealth = HealthComponent->GetMaxHealth();
	}
	if (IsValid(BuffComponent))
	{
		Info.BuffCompPtr = BuffComponent;
	}

	Info.PortraitAnimation = PortraitIdleAnimation;
	// 이동속도의 유일한 출처는 이동 컴포넌트다. UnitBase.cpp:488 과 같은 방식.
	if (const UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Info.MoveSpeed = Move->MaxWalkSpeed;
	}
	
	Info.bShowAttackPower = true;
	Info.bShowHealth = true;
	Info.bShowMoveSpeed = true;
	Info.bShowSkillIcons = true;

	return Info;
}

USoundBase* ASFCommander::GetSelectionSound_Implementation()
{
	return SelectionSound;
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
	// Initialize every commander before components and Blueprint BeginPlay observe
	// health. Player-controller registration may be delayed or never happen for NPCs.
	// CombatTargetComponent still applies any explicit OverrideMaxHealth afterward.
	HealthComponent->InitializeHealth(300.f);
	Super::BeginPlay();
	HealthComponent->OnHealthIsZero.AddUniqueDynamic(this, &ASFCommander::HandleCommanderDeath);

	// BP에서 붙인 무기 메시까지 배치 격자 데칼이 묻지 않도록 모든 렌더링 컴포넌트에 적용합니다.
	TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents(this);
	for (UPrimitiveComponent* Primitive : PrimitiveComponents)
	{
		Primitive->SetReceivesDecals(false);
	}

	if (bMyCommander)
	{
		// Selection traces use Visibility. Do not depend on an imported skeletal
		// mesh's physics bodies being present or covering the visible character.
		GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
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
	GetWorldTimerManager().ClearTimer(RespawnTimer);
	GetWorldTimerManager().ClearTimer(DeathAnimationTimer);
	HealthComponent->OnHealthIsZero.RemoveDynamic(this, &ASFCommander::HandleCommanderDeath);
	
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

float ASFCommander::GetRespawnTimeRemaining() const
{
	return bAwaitingRespawn && GetWorld()
		? FMath::Max(0.f, static_cast<float>(RespawnEndTime - GetWorld()->GetTimeSeconds())) : 0.f;
}

void ASFCommander::HandleCommanderDeath()
{
	if (bAwaitingRespawn) return;
	bAwaitingRespawn = true;
	RespawnEndTime = GetWorld()->GetTimeSeconds() + RespawnDelaySeconds;
	if (bMyCommander)
	{
		if (auto* PC = GetWorld()->GetFirstPlayerController<ASFInGamePlayerController>())
			PC->ClearPendingBuildingRef();
	}
	StopMoving();
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	if (auto* Anim = Cast<USFCommanderAnim>(GetMesh()->GetAnimInstance()))
	{
		Anim->OnSkillActiveDelegate.Unbind();
		Anim->bSkillFloating = false;
		Anim->Montage_Stop(0.f);
	}
	SetActorEnableCollision(false);
	if (DeathAnimation && DeathAnimation->GetPlayLength() > 0.f)
	{
		GetMesh()->PlayAnimation(DeathAnimation, false);
		GetWorldTimerManager().SetTimer(DeathAnimationTimer, this,
			&ASFCommander::HideAfterDeathAnimation, DeathAnimation->GetPlayLength(), false);
	}
	else
	{
		HideAfterDeathAnimation();
	}
	GetWorldTimerManager().SetTimer(RespawnTimer, this, &ASFCommander::RespawnAtNexus, RespawnDelaySeconds, false);
}

void ASFCommander::HideAfterDeathAnimation()
{
	if (bAwaitingRespawn)
	{
		SetActorHiddenInGame(true);
	}
}

bool ASFCommander::FindRespawnTransform(FTransform& OutTransform) const
{
	const auto* GS = GetWorld()->GetGameState<ASFGameState>();
	const int32 Team = CombatTargetComponent->GetTeamIndex();
	const auto* Nexus = GS ? GS->GetNexus(Team) : nullptr;
	auto* Nav = UNavigationSystemV1::GetCurrent(GetWorld());
	if (!IsValid(Nexus) || Nexus->IsActorBeingDestroyed() || !Nav) return false;
	const auto* NexusHealth = Nexus->FindComponentByClass<UHealthComponent>();
	if (NexusHealth && NexusHealth->GetIsDead()) return false;
	const auto* OtherNexus = GS->GetNexus(Team == 0 ? 1 : 0);
	const FVector Base = Nexus->GetActorLocation();
	FVector Forward = ((IsValid(OtherNexus) ? OtherNexus->GetActorLocation() : FVector::ZeroVector) - Base).GetSafeNormal2D();
	if (Forward.IsNearlyZero()) Forward = Nexus->GetActorForwardVector().GetSafeNormal2D();
	const FVector Side(-Forward.Y, Forward.X, 0.f);
	const FVector Extent = Nexus->GetBuildBounds().TransformBy(Nexus->GetActorTransform()).GetBox().GetExtent();
	const float Radius = GetCapsuleComponent()->GetScaledCapsuleRadius();
	const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const float Clearance = FMath::Abs(Forward.X) * Extent.X + FMath::Abs(Forward.Y) * Extent.Y + Radius + 200.f;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(CommanderRespawn), false, this);
	for (float Distance : {0.f, 200.f, 400.f, 600.f})
	{
		for (float Offset : {0.f, -250.f, 250.f, -500.f, 500.f})
		{
			FNavLocation Ground;
			const FVector Candidate = Base + Forward * (Clearance + Distance) + Side * Offset;
			if (!Nav->ProjectPointToNavigation(Candidate, Ground, FVector(150.f, 150.f, 1000.f))) continue;
			const FVector Location = Ground.Location + FVector(0.f, 0.f, HalfHeight + 5.f);
			if (GetWorld()->OverlapBlockingTestByChannel(Location, FQuat::Identity, ECC_Pawn,
				FCollisionShape::MakeCapsule(Radius, HalfHeight), Query)) continue;
			OutTransform = FTransform(Forward.Rotation(), Location);
			return true;
		}
	}
	return false;
}

void ASFCommander::RespawnAtNexus()
{
	if (!bAwaitingRespawn) return;
	FTransform Transform;
	if (!FindRespawnTransform(Transform))
	{
		// A blocked entrance or a navmesh rebuild must not strand the commander forever.
		GetWorldTimerManager().SetTimer(RespawnTimer, this, &ASFCommander::RespawnAtNexus, .5f, false);
		return;
	}
	GetWorldTimerManager().ClearTimer(DeathAnimationTimer);
	// PlayAnimation uses single-node mode; restore the locomotion/skill AnimBP on revival.
	GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	SetActorLocationAndRotation(Transform.GetLocation(), Transform.Rotator(), false, nullptr, ETeleportType::TeleportPhysics);
	HealthComponent->SetHealth(HealthComponent->GetMaxHealth(), HealthComponent->GetMaxHealth());
	// A health buff may have expired while HP was zero; recompute its current multiplier.
	HealthComponent->BoostHealth(BuffComponent->Current_Status.HPMul);
	bAwaitingRespawn = false;
	RespawnEndTime = 0.0;
	SetActorEnableCollision(true);
	SetActorHiddenInGame(false);
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	CombatTargetComponent->RestoreAfterRespawn();
}

void ASFCommander::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	if (IsValid(GetController<AUnitAIController>()))
	{
		GetController<AUnitAIController>()->OnCompleteMoving.AddUObject(
			this,
			&ASFCommander::CheckFailMoving
		);
	}
}

#if WITH_EDITOR
void ASFCommander::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// 변경된 프로퍼티의 이름을 가져옵니다.
	FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	// 우리가 원하는 특정 변수(MyTargetValue)가 변경되었는지 확인합니다.
	if (PropertyName == GET_MEMBER_NAME_CHECKED(ASFCommander, Race))
	{
		// 특정 변수가 변경되었을 때 실행할 함수나 로직을 여기에 작성합니다.
		SkillAsset.Empty();
		InitSkillAsset();
	}
}

void ASFCommander::InitSkillAsset()
{
	if (!SkillAsset.IsEmpty())
	{
		return;
	}
	SkillAsset.SetNum(3);

	switch (Race)
	{
	case ERace::BEAST:
		do {
			SkillAsset[0].Name = TEXT("돌격명령");
			SkillAsset[0].Time = 5.f;
			SkillAsset[0].Interval = 0.f;
			SkillAsset[0].Radius = 750.f;
			SkillAsset[0].SkillValue.Emplace(TEXT("SpeedMul"), 1.5f);
			SkillAsset[0].bIsBuff = true;
		} while (false);
		do {
			SkillAsset[1].Name = TEXT("전장의 포효");
			SkillAsset[1].Time = 10.f;
			SkillAsset[1].Interval = 0.f;
			SkillAsset[1].Radius = 750.f;
			SkillAsset[1].SkillValue.Emplace(TEXT("AtkMul"), 1.3f);
			SkillAsset[1].SkillValue.Emplace(TEXT("HPMul"), 1.5f);
			SkillAsset[0].bIsBuff = true;
		} while (false);
		do {
			SkillAsset[2].Name = TEXT("위압");
			SkillAsset[2].Time = 10.f;
			SkillAsset[2].Interval = 0.f;
			SkillAsset[2].Radius = 1000.f;
			SkillAsset[2].SkillValue.Emplace(TEXT("DmgMul"), 1.5f);
			SkillAsset[0].bIsBuff = false;
		} while (false);
		break;
	case ERace::ETHERIAN:
		do {
			SkillAsset[0].Name = TEXT("성역의 축복");
			SkillAsset[0].Time = 30.f;
			SkillAsset[0].Interval = 1.f;
			SkillAsset[0].Radius = 2000.f;
			SkillAsset[0].SkillValue.Emplace(TEXT("HealAmount"), 5.f);
			SkillAsset[0].bIsBuff = true;
		} while (false);
		do {
			SkillAsset[1].Name = TEXT("빨리빨리");
			SkillAsset[1].Time = 10.f;
			SkillAsset[1].Interval = 0.f;
			SkillAsset[1].Radius = -1.f;
			SkillAsset[1].SkillValue.Emplace(TEXT("ProdSpeedMul"), 2.f);
			SkillAsset[0].bIsBuff = true;
		} while (false);
		do {
			SkillAsset[2].Name = TEXT("천상의 수호");
			SkillAsset[2].Time = 5.f;
			SkillAsset[2].Interval = 0.f;
			SkillAsset[2].Radius = 600.f;
			SkillAsset[2].SkillValue.Emplace(TEXT("DmgMul"), 0.1f);
			SkillAsset[0].bIsBuff = true;
		} while (false);
		break;
	}
}
#endif

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
	if (bAwaitingRespawn)
	{
		if (bMyCommander)
			if (auto* PC = GetWorld()->GetFirstPlayerController<ASFInGamePlayerController>()) PC->ClearPendingBuildingRef();
		return;
	}
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
		StopMoving();
		BeginBuild();
	}
	else
	{
		MoveToAction(Dest, ECommandAction::BUILD);
	}
}

void ASFCommander::CheckFailMoving(const FPathFollowingResult& Result)
{
	// ★CHANGED: 건설 명령이 아니면 관심 없음 (조기 반환으로 중첩 제거)
	if (CommandAction != ECommandAction::BUILD)
	{
		return;
	}

	// ★ADD: 성공/중단은 실패가 아니다.
	//   - 도착 성공  → Tick 의 CheckCanBuild() 가 건설을 시작한다
	//   - 중단(Aborted) → 새 요청이 이어받은 것이므로 실패 판정 시 빌딩 에러가 발생하므로 이어 건설한다.
	if (Result.IsSuccess() || Result.IsInterrupted())
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
	if (bAwaitingRespawn) return;
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
	if (MoveCommandSound && GetWorld())
	{
		GetWorld()->GetSubsystem<USFUnitVoiceSubsystem>()->RequestVoice(this, MoveCommandSound, true);
	}
}

bool ASFCommander::CheckCanBuild()
{
	float Dist = FVector::Dist(GetActorLocation(), BuildLocation);
	float CapsuleHalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	float CapsuleRadius = GetCapsuleComponent()->GetScaledCapsuleRadius();
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
	if (bAwaitingRespawn) return;
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
	SF_LOG(Warning, "%s", *Msg);
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

int32 ASFCommander::GetUnlockedSkillTier() const
{
	const ASFGameState* GS = GetWorld() ? GetWorld()->GetGameState<ASFGameState>() : nullptr;
	const int32 Team = CombatTargetComponent ? CombatTargetComponent->GetTeamIndex() : INDEX_NONE;
	return GS && GS->GetNexus(Team) ? FMath::Clamp(GS->GetNexusTier(Team) + 1, 0, 3) : 0;
}

bool ASFCommander::IsSkillTierUnlocked(int32 SkillTier) const
{
	return SkillTier >= 1 && SkillTier <= 3 && SkillTier <= GetUnlockedSkillTier();
}

float ASFCommander::GetSkillCooldownRemaining(int32 SkillTier) const
{
	if (SkillTier < 1 || SkillTier > 3 || !GetWorld()) return 0.f;
	return static_cast<float>(FMath::Max(0.0, SkillCooldownEnds[SkillTier - 1] - GetWorld()->GetTimeSeconds()));
}

FText ASFCommander::GetSkillName(int32 SkillTier) const
{
	if (SkillTier < 1 || SkillTier > 3) return FText::GetEmpty();
	const FString SkillNames[] = {SkillAsset[0].Name, SkillAsset[1].Name, SkillAsset[2].Name };
	return FText::FromString(SkillNames[SkillTier - 1]);
}

FText ASFCommander::ReadSkillEffectDescription(int32 SkillTier)
{
	if (SkillTier < 1 || SkillTier > 3) return FText::GetEmpty();
	FString ToolTip;
	switch(Race)
	{
	case ERace::BEAST:
		switch (SkillTier)
		{
		case 1:
			ToolTip = FString::Printf(TEXT("반경 %.1fm 안의 아군 유닛과 지휘관에게 이동 속도 %d%% 증가 부여 (지속시간: %.1f초)"),
				SkillAsset[SkillTier - 1].Radius / 100.f,
				FMath::RoundToInt32((SkillAsset[SkillTier - 1].FindValue(TEXT("SpeedMul")) - 1.f) * 100.f),
				SkillAsset[SkillTier - 1].Time);
			break;
		case 2:
			ToolTip = FString::Printf(TEXT("반경 %.1fm 안의 아군 유닛과 지휘관에게 공격력 %d%%, 최대 체력 %d%% 증가 부여 (지속시간: %.1f초)"),
				SkillAsset[SkillTier - 1].Radius / 100.f,
				FMath::RoundToInt32((SkillAsset[SkillTier - 1].FindValue(TEXT("AtkMul")) - 1.f) * 100.f),
				FMath::RoundToInt32((SkillAsset[SkillTier - 1].FindValue(TEXT("HPMul")) - 1.f) * 100.f),
				SkillAsset[SkillTier - 1].Time);
			break;
		case 3:
			ToolTip = FString::Printf(TEXT("반경 %.1fm 안의 적에게 받는 피해 %d%% 증가 부여 (지속시간: %.1f초)"),
				SkillAsset[SkillTier - 1].Radius / 100.f,
				FMath::RoundToInt32((SkillAsset[SkillTier - 1].FindValue(TEXT("DmgMul")) - 1.f) * 100.f),
				SkillAsset[SkillTier - 1].Time);
			break;
		}
		break;
	case ERace::ETHERIAN:
		switch (SkillTier)
		{
		case 1:
			ToolTip = FString::Printf(TEXT("반경 %.1fm 안의 아군 유닛과 지휘관에게 초당 %d 회복 부여 (지속시간: %.1f초)"),
				SkillAsset[SkillTier - 1].Radius / 100.f,
				FMath::RoundToInt32(SkillAsset[SkillTier - 1].FindValue(TEXT("HealAmount"))),
				SkillAsset[SkillTier - 1].Time);
			break;
		case 2:
			ToolTip = FString::Printf(TEXT("모든 아군 생산 건물에 생산 속도 %d%% 증가 부여 (지속시간: %.1f초)"),
				FMath::RoundToInt32((SkillAsset[SkillTier - 1].FindValue(TEXT("ProdSpeedMul")) - 1.f) * 100.f),
				SkillAsset[SkillTier - 1].Time);
			break;
		case 3:
			ToolTip = FString::Printf(TEXT("반경 %.1fm 안의 아군에게 받는 피해 %d%% 감소 부여 (지속시간: %.1f초)"),
				SkillAsset[SkillTier - 1].Radius / 100.f,
				FMath::RoundToInt32((1.f - SkillAsset[SkillTier - 1].FindValue(TEXT("DmgMul"))) * 100.f),
				SkillAsset[SkillTier - 1].Time);
			break;
		}
	};
	return FText::FromString(ToolTip);
}

void ASFCommander::ActiveCommanderSkill(int32 SkillTier)
{
	if (!IsSkillTierUnlocked(SkillTier) || GetSkillCooldownRemaining(SkillTier) > 0.f
		|| (HealthComponent && HealthComponent->GetIsDead())
		|| !SkillAsset.IsValidIndex(SkillTier - 1)) return;
	TWeakObjectPtr<USFCommanderAnim> Anim = Cast<USFCommanderAnim>(GetMesh()->GetAnimInstance());
	if (!Anim.IsValid())
	{
		SF_LOG(Error, "ASFCommander: AnimInstance Is Invalid.");
		return;
	}

	// 스킬 시전중에는 실행되지 않음.
	//  - 현재 실행중인 몽타주의 스킬 Delegate가 아직 남아있으면 실행하지 않는다. 스킬 Delegate는 효과가 한번 발동되기전까지 존재한다.
	//  - 만약 스킬이 발동되기 전에 다른 스킬을 사용한다면 이전 스킬은 효과가 발동되지 않고 쿨타임 적용만 발생한다. 이를 방지하기 위한 것.
	TWeakObjectPtr<UAnimMontage> CurrentSkillMontage = (SkillAsset.IsValidIndex(SkillTier - 1) && IsValid(SkillAsset[SkillTier - 1].SkillMontage)) ? SkillAsset[SkillTier - 1].SkillMontage : nullptr;
	if (Anim->OnSkillActiveDelegate.IsBound())
	{
		return;
	}

	if (!IsValid(CurrentSkillMontage.Get()))
	{
		SF_LOG(Error, "ASFCommander: SkillMontage Is Invalid.");
		return;
	}
	Anim->bSkillFloating = false;
	if (Anim->Montage_Play(CurrentSkillMontage.Get()) <= 0.f)
	{
		return;
	}
	SkillCooldownEnds[SkillTier - 1] = GetWorld()->GetTimeSeconds() + SkillCooldownSeconds;

	Anim->CurrentSkillEffect = (SkillAsset.IsValidIndex(SkillTier - 1) && IsValid(SkillAsset[SkillTier - 1].SkillEffect)) ? SkillAsset[SkillTier - 1].SkillEffect : nullptr;
	Anim->CurrentSkillHitEffect = (SkillAsset.IsValidIndex(SkillTier - 1) && IsValid(SkillAsset[SkillTier - 1].SkillHitEffect)) ? SkillAsset[SkillTier - 1].SkillHitEffect : nullptr;

	Anim->SkillDelegateTier = SkillTier;
	switch (Race)
	{
	case ERace::BEAST:
		Anim->OnSkillActiveDelegate.BindDynamic(this, &ASFCommander::ActiveCommanderSkill_Beast);
		break;
	case ERace::ETHERIAN:
		Anim->OnSkillActiveDelegate.BindDynamic(this, &ASFCommander::ActiveCommanderSkill_Ether);
		break;
	}
}

void ASFCommander::ActiveCommanderSkill_Beast(int32 SkillTier, UNiagaraSystem* CurrentSkillEffect, UNiagaraSystem* CurrentSkillHitEffect)
{
	if (bAwaitingRespawn) return;
	TWeakObjectPtr<UNiagaraSystem> CurrentSkillEffectWPtr(CurrentSkillEffect);
	TWeakObjectPtr<UNiagaraSystem> CurrentSkillHitEffectWPtr(CurrentSkillHitEffect);

	TWeakObjectPtr<UWorld> World = GetWorld();
	if (!World.IsValid())
	{
		SF_LOG(Error, "ASFCommander: World Is Invalid.");
		return;
	}

	switch (SkillTier)
	{
	case 1:
		//1티어 스킬- 범위 내 아군 이속증가
		do
		{
			/** 버프 정보 */
			const FString BuffKey = TEXT("SpeedUp"); // 버프 키 (중복방지)
			const FText BuffText = FText::FromString(SkillAsset[SkillTier - 1].Name);	// 버프 UI표시명
			const float BuffTime = SkillAsset[SkillTier - 1].Time;	// 버프 지속시간(초)
			const float BuffInterval = SkillAsset[SkillTier - 1].Interval;	// 효과 발동 주기 (0이면 매 프레임 발동)

			const float SkillRadius = SkillAsset[SkillTier - 1].Radius; // 버프가 적용되는 스킬 반경
			const float SpeedMul = SkillAsset[SkillTier - 1].FindValue(TEXT("SpeedMul")); // 이동속도 배율
			//-------------------

			TArray<FBuffTarget> Targets;
			if (SkillRadius >= 0.f)
			{
				// 스킬 영향권에 들어온 액터 가져오기 (USFBuffComponent를 가지고 있는 Pawn 콜리전인 액터에 한함)
				Targets = GetSkillActors(World, SkillRadius);
			}
			else
			{
				// 모든 액터 가져오기 (USFBuffComponent를 가지고 있는 액터에 한함)
				Targets = GetSkillActors(World, AActor::StaticClass());
			}

			for (FBuffTarget TG : Targets)
			{
				bool IsTarget = false;	// 버프대상인가?

				TWeakObjectPtr<UCombatTargetComponent> TargetCombatComp = TG.ActorPtr->GetComponentByClass<UCombatTargetComponent>();
				if (TargetCombatComp.IsValid())
				{
					// 아군이고 커맨더 혹은 유닛이다.
					if (TargetCombatComp->GetTeamIndex() == 0 &&
						(Cast<ASFCommander>(TG.ActorPtr) || Cast<AUnitBase>(TG.ActorPtr)))
					{
						IsTarget = true;
					}
				}

				if (IsTarget)
				{
					// 버프 부여
					FInputBuffData BuffInfo;
					BuffInfo.KeyName = BuffKey;
					BuffInfo.DisplayName = BuffText;
					BuffInfo.Description = FString::Printf(TEXT("이동속도 %d%% 증가"),
						FMath::RoundToInt32((SkillAsset[SkillTier - 1].FindValue(TEXT("SpeedMul")) - 1.f) * 100.f));
					BuffInfo.bIsBuff = SkillAsset[SkillTier - 1].bIsBuff;
					BuffInfo.Image = SkillAsset[SkillTier - 1].BuffImage;
					BuffInfo.TickEvent = [TG, SpeedMul]()	// BuffInterval 주기로 반복 실행(Tick)
						{

							TG.BuffCompPtr->Current_Status.Speed *= SpeedMul;
						};
					BuffInfo.BeginEvent = [TG]()	// 버프 시작 시 실행(Begin)
						{
							TG.BuffCompPtr->ApplyBuffFlag(EBuffApply::Speed);
						};
					BuffInfo.EndEvent = [TG]()	// 버프 종료 시 실행(End)
						{
							TG.BuffCompPtr->ApplyBuffFlag(EBuffApply::Speed);
						};
					BuffInfo.InitialTime = BuffTime;
					BuffInfo.TickInterval = BuffInterval;
					BuffInfo.Particle = CurrentSkillHitEffectWPtr.Get();
					TG.BuffCompPtr->AddBuff(BuffInfo);
				}
			}

			if (CurrentSkillEffectWPtr.IsValid())
			{
				FVector ParticleScale = { SkillRadius / 100.f / GetActorScale().X,SkillRadius / 100.f / GetActorScale().X, 1.f};
				UNiagaraComponent* Particle = UNiagaraFunctionLibrary::SpawnSystemAttached(CurrentSkillEffectWPtr.Get(), RootComponent, NAME_None, FVector::ZeroVector, FRotator::ZeroRotator, ParticleScale, EAttachLocation::KeepRelativeOffset, true, ENCPoolMethod::AutoRelease);
				Particle->SetUsingAbsoluteRotation(true);
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
			const FText BuffText = FText::FromString(SkillAsset[SkillTier - 1].Name);	// 버프 UI표시명
			const float BuffTime = SkillAsset[SkillTier - 1].Time;	// 버프 지속시간(초)
			const float BuffInterval = SkillAsset[SkillTier - 1].Interval;	// 효과 발동 주기 (0이면 매 프레임 발동)

			const float SkillRadius = SkillAsset[SkillTier - 1].Radius; // 버프가 적용되는 스킬 반경
			const float AtkMul = SkillAsset[SkillTier - 1].FindValue(TEXT("AtkMul")); // 공격력 배율
			const float HPMul = SkillAsset[SkillTier - 1].FindValue(TEXT("HPMul")); // 체력 배율
			//-------------------

			TArray<FBuffTarget> Targets;
			if (SkillRadius >= 0.f)
			{
				// 스킬 영향권에 들어온 액터 가져오기 (USFBuffComponent를 가지고 있는 Pawn 콜리전인 액터에 한함)
				Targets = GetSkillActors(World, SkillRadius);
			}
			else
			{
				// 모든 액터 가져오기 (USFBuffComponent를 가지고 있는 액터에 한함)
				Targets = GetSkillActors(World, AActor::StaticClass());
			}

			for (FBuffTarget TG : Targets)
			{
				bool IsTarget = false;	// 버프대상인가?

				TWeakObjectPtr<UCombatTargetComponent> TargetCombatComp = TG.ActorPtr->GetComponentByClass<UCombatTargetComponent>();
				if (TargetCombatComp.IsValid())
				{
					// 아군이고 커맨더 혹은 유닛이다.
					if (TargetCombatComp->GetTeamIndex() == 0 &&
						(Cast<ASFCommander>(TG.ActorPtr) || Cast<AUnitBase>(TG.ActorPtr)))
					{
						IsTarget = true;
					}
				}

				if (IsTarget)
				{
					// 버프 부여
					FInputBuffData BuffInfo;
					BuffInfo.KeyName = BuffKey;
					BuffInfo.DisplayName = BuffText;
					BuffInfo.Description = FString::Printf(TEXT("공격력 %d%%, 최대 체력 %d%% 증가"),
						FMath::RoundToInt32((SkillAsset[SkillTier - 1].FindValue(TEXT("AtkMul")) - 1.f) * 100.f),
						FMath::RoundToInt32((SkillAsset[SkillTier - 1].FindValue(TEXT("HPMul")) - 1.f) * 100.f));
					BuffInfo.bIsBuff = SkillAsset[SkillTier - 1].bIsBuff;
					BuffInfo.Image = SkillAsset[SkillTier - 1].BuffImage;
					BuffInfo.TickEvent = [TG, AtkMul, HPMul]()	// BuffInterval 주기로 반복 실행(Tick)
						{

							TG.BuffCompPtr->Current_Status.AtkMul *= AtkMul;
							TG.BuffCompPtr->Current_Status.HPMul *= HPMul;
						};
					BuffInfo.BeginEvent = [TG, HPMul]()	// 버프 시작 시 실행(Begin)
						{

							TG.BuffCompPtr->ApplyBuffFlag(EBuffApply::MaxHealth);
							TG.BuffCompPtr->ApplyBuffFlag(EBuffApply::Atk);
						};
					BuffInfo.EndEvent = [TG, HPMul]()	// 버프 종료 시 실행(End)
						{

							TG.BuffCompPtr->ApplyBuffFlag(EBuffApply::MaxHealth);
							TG.BuffCompPtr->ApplyBuffFlag(EBuffApply::Atk);
						};
					BuffInfo.InitialTime = BuffTime;
					BuffInfo.TickInterval = BuffInterval;
					BuffInfo.Particle = CurrentSkillHitEffectWPtr.Get();
					TG.BuffCompPtr->AddBuff(BuffInfo);
				}
			}
			if (CurrentSkillEffectWPtr.IsValid())
			{
				FVector ParticleScale = { SkillRadius / 100.f,SkillRadius / 100.f, 1.f };
				UNiagaraFunctionLibrary::SpawnSystemAtLocation(World.Get(), CurrentSkillEffectWPtr.Get(), GetActorLocation(), FRotator::ZeroRotator, ParticleScale, true, true, ENCPoolMethod::AutoRelease);
			}
		} while (false);
		break;
	case 3:
		//3티어 스킬- 적 받는피해 증가
		do
		{
			/** 버프 정보 */
			const FString BuffKey = TEXT("ReceiveDmgUp"); // 버프 키 (중복방지)
			const FText BuffText = FText::FromString(SkillAsset[SkillTier - 1].Name);	// 버프 UI표시명
			const float BuffTime = SkillAsset[SkillTier - 1].Time;	// 버프 지속시간(초)
			const float BuffInterval = SkillAsset[SkillTier - 1].Interval;	// 효과 발동 주기 (0이면 매 프레임 발동)

			const float SkillRadius = SkillAsset[SkillTier - 1].Radius; // 버프가 적용되는 스킬 반경
			const float DmgMul = SkillAsset[SkillTier - 1].FindValue(TEXT("DmgMul"));	// 받는 피해 증가 배율
			//-------------------

			TArray<FBuffTarget> Targets;
			if (SkillRadius >= 0.f)
			{
				// 스킬 영향권에 들어온 액터 가져오기 (USFBuffComponent를 가지고 있는 Pawn 콜리전인 액터에 한함)
				Targets = GetSkillActors(World, SkillRadius);
			}
			else
			{
				// 모든 액터 가져오기 (USFBuffComponent를 가지고 있는 액터에 한함)
				Targets = GetSkillActors(World, AActor::StaticClass());
			}

			for (FBuffTarget TG : Targets)
			{
				bool IsTarget = false;	// 버프대상인가?

				TWeakObjectPtr<UCombatTargetComponent> TargetCombatComp = TG.ActorPtr->GetComponentByClass<UCombatTargetComponent>();
				if (TargetCombatComp.IsValid())
				{
					// 적이다.
					if (TargetCombatComp->GetTeamIndex() != 0)
					{
						IsTarget = true;
					}
				}

				if (IsTarget)
				{
					// 버프 부여
					FInputBuffData BuffInfo;
					BuffInfo.KeyName = BuffKey;
					BuffInfo.DisplayName = BuffText;
					BuffInfo.Description = FString::Printf(TEXT("적에게 받는 피해 %d%% 증가"),
						FMath::RoundToInt32((SkillAsset[SkillTier - 1].FindValue(TEXT("DmgMul")) - 1.f) * 100.f));
					BuffInfo.bIsBuff = SkillAsset[SkillTier - 1].bIsBuff;
					BuffInfo.Image = SkillAsset[SkillTier - 1].BuffImage;
					BuffInfo.TickEvent = [TG, DmgMul]()	// BuffInterval 주기로 반복 실행(Tick)
						{

							TG.BuffCompPtr->Current_Status.DmgMul *= DmgMul;
						};
					BuffInfo.BeginEvent = [TG]()	// 버프 시작 시 실행(Begin)
						{
							TG.BuffCompPtr->ApplyBuffFlag(EBuffApply::DamageMul);
						};
					BuffInfo.EndEvent = [TG]()	// 버프 종료 시 실행(End)
						{
							TG.BuffCompPtr->ApplyBuffFlag(EBuffApply::DamageMul);
						};
						BuffInfo.InitialTime = BuffTime;
					BuffInfo.TickInterval = BuffInterval;
					BuffInfo.Particle = CurrentSkillHitEffectWPtr.Get();
					TG.BuffCompPtr->AddBuff(BuffInfo);
				}
			}
			if (CurrentSkillEffectWPtr.IsValid())
			{
				FVector ParticleScale = { SkillRadius / 100.f / GetActorScale().X,SkillRadius / 100.f / GetActorScale().X, 1.f };
				//UNiagaraFunctionLibrary::SpawnSystemAtLocation(World.Get(), SkillEffect1, GetActorLocation(), FRotator::ZeroRotator, ParticleScale, true, true, ENCPoolMethod::AutoRelease);
				UNiagaraComponent* Particle = UNiagaraFunctionLibrary::SpawnSystemAttached(CurrentSkillEffectWPtr.Get(), RootComponent, NAME_None, FVector::ZeroVector, FRotator::ZeroRotator, ParticleScale, EAttachLocation::KeepRelativeOffset, true, ENCPoolMethod::AutoRelease);
				Particle->SetUsingAbsoluteRotation(true);
			}
		} while (false);
		break;
	}
}

void ASFCommander::ActiveCommanderSkill_Ether(int32 SkillTier, UNiagaraSystem* CurrentSkillEffect, UNiagaraSystem* CurrentSkillHitEffect)
{
	if (bAwaitingRespawn) return;
	TWeakObjectPtr<UNiagaraSystem> CurrentSkillEffectWPtr(CurrentSkillEffect);
	TWeakObjectPtr<UNiagaraSystem> CurrentSkillHitEffectWPtr(CurrentSkillHitEffect);

	TWeakObjectPtr<UWorld> World = GetWorld();
	if (!World.IsValid())
	{
		SF_LOG(Error, "ASFCommander: World Is Invalid.");
		return;
	}

	switch (SkillTier)
	{
	case 1:
		//1티어 스킬- 극 소량 체젠 업
		do
		{
			/** 버프 정보 */
			const FString BuffKey = TEXT("HealUp"); // 버프 키 (중복방지)
			const FText BuffText = FText::FromString(SkillAsset[SkillTier - 1].Name);	// 버프 UI표시명
			const float BuffTime = SkillAsset[SkillTier - 1].Time;	// 버프 지속시간(초)
			const float BuffInterval = SkillAsset[SkillTier - 1].Interval;	// 효과 발동 주기 (0이면 매 프레임 발동)

			const float SkillRadius = SkillAsset[SkillTier - 1].Radius; // 버프가 적용되는 스킬 반경
			const float HealAmount = SkillAsset[SkillTier - 1].FindValue(TEXT("HealAmount")); // 회복량
			//-------------------

			TArray<FBuffTarget> Targets;
			if (SkillRadius >= 0.f)
			{
				// 스킬 영향권에 들어온 액터 가져오기 (USFBuffComponent를 가지고 있는 Pawn 콜리전인 액터에 한함)
				Targets = GetSkillActors(World, SkillRadius);
			}
			else
			{
				// 모든 액터 가져오기 (USFBuffComponent를 가지고 있는 액터에 한함)
				Targets = GetSkillActors(World, AActor::StaticClass());
			}

			for (FBuffTarget TG : Targets)
			{
				bool IsTarget = false;	// 버프대상인가?

				TWeakObjectPtr<UCombatTargetComponent> TargetCombatComp = TG.ActorPtr->GetComponentByClass<UCombatTargetComponent>();
				if (TargetCombatComp.IsValid())
				{
					// 아군이고 커맨더 혹은 유닛이다.
					if (TargetCombatComp->GetTeamIndex() == 0 &&
						(Cast<ASFCommander>(TG.ActorPtr) || Cast<AUnitBase>(TG.ActorPtr)))
					{
						IsTarget = true;
					}
				}

				if (IsTarget)
				{
					// 버프 부여
					FInputBuffData BuffInfo;
					BuffInfo.KeyName = BuffKey;
					BuffInfo.DisplayName = BuffText;
					BuffInfo.Description = FString::Printf(TEXT("초당 %d 회복"),
						FMath::RoundToInt32(SkillAsset[SkillTier - 1].FindValue(TEXT("HealAmount"))));
					BuffInfo.bIsBuff = SkillAsset[SkillTier - 1].bIsBuff;
					BuffInfo.Image = SkillAsset[SkillTier - 1].BuffImage;
					BuffInfo.TickEvent = [TG, HealAmount]()	// BuffInterval 주기로 반복 실행(Tick)
						{
							TWeakObjectPtr<UHealthComponent> HPComp = TG.ActorPtr->GetComponentByClass<UHealthComponent>();
							if (HPComp.IsValid())
							{
								HPComp->IncreaseCurrentHealth(HealAmount);
							}
						};
					BuffInfo.BeginEvent = [TG]()	// 버프 시작 시 실행(Begin)
						{
							TG.BuffCompPtr->ApplyBuffFlag(EBuffApply::Regen);
						};
					BuffInfo.EndEvent = [TG]()	// 버프 종료 시 실행(End)
						{
							TG.BuffCompPtr->ApplyBuffFlag(EBuffApply::Regen);
						};
					BuffInfo.InitialTime = BuffTime;
					BuffInfo.TickInterval = BuffInterval;
					BuffInfo.Particle = CurrentSkillHitEffectWPtr.Get();
					TG.BuffCompPtr->AddBuff(BuffInfo);
				}
			}
			if (CurrentSkillEffectWPtr.IsValid())
			{
				FVector ParticleScale = { SkillRadius / 100.f,SkillRadius / 100.f, 1.f };
				UNiagaraFunctionLibrary::SpawnSystemAtLocation(World.Get(), CurrentSkillEffectWPtr.Get(), GetActorLocation(), FRotator::ZeroRotator, ParticleScale, true, true, ENCPoolMethod::AutoRelease);
			}
		} while (false);
		break;
	case 2:
		//2티어 스킬- 모든 건물 생산속도 증가
		do
		{
			/** 버프 정보 */
			const FString BuffKey = TEXT("ProdSpeedUp");
			const FText BuffText = FText::FromString(SkillAsset[SkillTier - 1].Name);	// 버프 UI표시명
			const float BuffTime = SkillAsset[SkillTier - 1].Time;	// 버프 지속시간(초)
			const float BuffInterval = SkillAsset[SkillTier - 1].Interval;	// 효과 발동 주기 (0이면 매 프레임 발동)

			const float SkillRadius = SkillAsset[SkillTier - 1].Radius; // 버프가 적용되는 스킬 반경
			const float ProductionSpeedMultiplier = SkillAsset[SkillTier - 1].FindValue(TEXT("ProdSpeedMul"));
			//--------------------------------------------

			const ASFInGamePlayerController* SFPC =
					World->GetFirstPlayerController<ASFInGamePlayerController>();
			if (!IsValid(SFPC)) return;
			
			const int32 CasterTeamIndex = SFPC->GetMyTeamIndex();

			TArray<FBuffTarget> Targets;
			if (SkillRadius >= 0.f)
			{
				// 스킬 영향권에 들어온 액터 가져오기 (USFBuffComponent를 가지고 있는 Pawn 콜리전인 액터에 한함)
				Targets = GetSkillActors(World, SkillRadius);
			}
			else
			{
				// 모든 액터 가져오기 (USFBuffComponent를 가지고 있는 건물에 한함)
				Targets = GetSkillActors(World, ASFProductionBuilding::StaticClass());
			}

			for (FBuffTarget TG : Targets)
			{
				bool IsTarget = false;	// 버프대상인가?

				TWeakObjectPtr<ASFProductionBuilding> TargetBuilding = Cast<ASFProductionBuilding>(TG.ActorPtr);
				if (TargetBuilding.IsValid())
				{
					// 아군 건물이다.
					TWeakObjectPtr<UCombatTargetComponent> CTComp = TargetBuilding->GetComponentByClass<UCombatTargetComponent>();
					if (CTComp.IsValid() && CTComp->GetTeamIndex() == 0)
					{
						IsTarget = true;
					}
				}

				if (IsTarget)
				{
					// 버프 부여
					FInputBuffData BuffInfo;
					BuffInfo.KeyName = BuffKey;
					BuffInfo.DisplayName = BuffText;
					BuffInfo.Description = FString::Printf(TEXT("생산 속도 %d%% 증가"),
						FMath::RoundToInt32((SkillAsset[SkillTier - 1].FindValue(TEXT("ProdSpeedMul")) - 1.f) * 100.f));
					BuffInfo.bIsBuff = SkillAsset[SkillTier - 1].bIsBuff;
					BuffInfo.Image = SkillAsset[SkillTier - 1].BuffImage;
					BuffInfo.TickEvent = [TargetBuilding]()	// BuffInterval 주기로 반복 실행(Tick)
						{
						};
					BuffInfo.BeginEvent = [TargetBuilding, ProductionSpeedMultiplier]()	// 버프 시작 시 실행(Begin)
						{
							// TODO: 버프 시작 시 함수 작성
							TargetBuilding->ApplySpeedBuff(ProductionSpeedMultiplier);
						};
					BuffInfo.EndEvent = [TargetBuilding]()	// 버프 종료 시 실행(End)
						{
							// TODO: 버프 종료 시 함수 작성
							TargetBuilding->ResetSpeedMultiplier();
						};
					BuffInfo.InitialTime = BuffTime;
					BuffInfo.TickInterval = BuffInterval;
					BuffInfo.Particle = CurrentSkillHitEffectWPtr.Get();
					TG.BuffCompPtr->AddBuff(BuffInfo);
				}
			}
			if (CurrentSkillEffectWPtr.IsValid())
			{
				FVector ParticleScale = { GetActorScale().X, GetActorScale().X, 1.f };
				UNiagaraComponent* Particle = UNiagaraFunctionLibrary::SpawnSystemAttached(CurrentSkillEffectWPtr.Get(), RootComponent, NAME_None, FVector::ZeroVector, FRotator::ZeroRotator, ParticleScale, EAttachLocation::KeepRelativeOffset, true, ENCPoolMethod::AutoRelease);
				Particle->SetUsingAbsoluteRotation(true);
			}
		} while (false);
		break;
	case 3:
		//3티어 스킬- 아군 받는피해 감소
		do
		{
			/** 버프 정보 */
			const FString BuffKey = TEXT("ReceiveDmgDown"); // 버프 키 (중복방지)
			const FText BuffText = FText::FromString(SkillAsset[SkillTier - 1].Name);	// 버프 UI표시명
			const float BuffTime = SkillAsset[SkillTier - 1].Time;	// 버프 지속시간(초)
			const float BuffInterval = SkillAsset[SkillTier - 1].Interval;	// 효과 발동 주기 (0이면 매 프레임 발동)

			const float SkillRadius = SkillAsset[SkillTier - 1].Radius; // 버프가 적용되는 스킬 반경
			const float DmgMul = SkillAsset[SkillTier - 1].FindValue(TEXT("DmgMul"));	// 받는 피해 증가 배율
			//-------------------

			TArray<FBuffTarget> Targets;
			if (SkillRadius >= 0.f)
			{
				// 스킬 영향권에 들어온 액터 가져오기 (USFBuffComponent를 가지고 있는 Pawn 콜리전인 액터에 한함)
				Targets = GetSkillActors(World, SkillRadius);
			}
			else
			{
				// 모든 액터 가져오기 (USFBuffComponent를 가지고 있는 액터에 한함)
				Targets = GetSkillActors(World, AActor::StaticClass());
			}

			for (FBuffTarget TG : Targets)
			{
				bool IsTarget = false;	// 버프대상인가?

				TWeakObjectPtr<UCombatTargetComponent> TargetCombatComp = TG.ActorPtr->GetComponentByClass<UCombatTargetComponent>();
				if (TargetCombatComp.IsValid())
				{
					// 아군이다.
					if (TargetCombatComp->GetTeamIndex() == 0)
					{
						IsTarget = true;
					}
				}

				if (IsTarget)
				{
					// 버프 부여
					FInputBuffData BuffInfo;
					BuffInfo.KeyName = BuffKey;
					BuffInfo.DisplayName = BuffText;
					BuffInfo.Description = FString::Printf(TEXT("적에게 받는 피해 %d%% 감소"),
						FMath::RoundToInt32((1.f - SkillAsset[SkillTier - 1].FindValue(TEXT("DmgMul"))) * 100.f));
					BuffInfo.bIsBuff = SkillAsset[SkillTier - 1].bIsBuff;
					BuffInfo.Image = SkillAsset[SkillTier - 1].BuffImage;
					BuffInfo.TickEvent = [TG, DmgMul]()	// BuffInterval 주기로 반복 실행(Tick)
						{
							TG.BuffCompPtr->Current_Status.DmgMul *= DmgMul;
						},
					BuffInfo.BeginEvent = [TG]()	// 버프 시작 시 실행(Begin)
						{
							TG.BuffCompPtr->ApplyBuffFlag(EBuffApply::DamageMul);
						},
					BuffInfo.EndEvent = [TG]()	// 버프 종료 시 실행(End)
						{
							TG.BuffCompPtr->ApplyBuffFlag(EBuffApply::DamageMul);
						},
					BuffInfo.InitialTime = BuffTime;
					BuffInfo.TickInterval = BuffInterval;
					BuffInfo.Particle = CurrentSkillHitEffectWPtr.Get();
					BuffInfo.ParticleScaleForCharacter = { 1.f / 25.f, 1.f / 45.f };
					BuffInfo.ParticleScaleForOther = { 1.f / 55.f, 1.f / 35.f };
					TG.BuffCompPtr->AddBuff(BuffInfo);
				}
			}
			if (CurrentSkillEffectWPtr.IsValid())
			{
				FVector ParticleScale = FVector::OneVector * SkillRadius / 100.f;
				UNiagaraFunctionLibrary::SpawnSystemAtLocation(World.Get(), CurrentSkillEffectWPtr.Get(), GetActorLocation(), FRotator::ZeroRotator, ParticleScale, true, true, ENCPoolMethod::AutoRelease);
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
	//DrawDebugSphere(World.Get(), GetActorLocation(), Radius, 16, FColor::Red, false, 5.f);

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
