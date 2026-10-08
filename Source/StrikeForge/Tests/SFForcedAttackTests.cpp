#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Units/Unit/UnitBase.h"
#include "Buildings/Building/ProductBuilding/SFProductionBuilding.h"
#include "ActorComponent/Combat/CombatTargetComponent.h"
#include "ActorComponent/Stat/HealthComponent.h"
#include "Game/Subsystem/SFCombatSubsystem.h"
#include "Game/Subsystem/SFFogOfWarSubsystem.h"
#include "InGame/Controller/SFInGamePlayerController.h"
#include "InGame/SFInGameHUDWidget.h"
#include "InGame/HUD/SFCommandFeedbackNiagara.h"
#include "Components/BoxComponent.h"
#include "Components/DecalComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "UObject/UnrealType.h"
#include "UObject/Script.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimTypes.h"
#include "Units/Commander/SFCommander.h"
#include "Units/Unit/UnitAIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "ActorComponent/UI/SFUnitHealthBarComponent.h"
#include "HAL/IConsoleManager.h"
#include "EngineUtils.h"
#include "Components/CapsuleComponent.h"
#include "Units/Projectile/SFUnitProjectile.h"

struct FSFPlaytestMovementAccess
{
	static void Bind(AUnitAIController* AI, ASFCommander* Commander)
	{ AI->OnCompleteMoving.AddUObject(Commander, &ASFCommander::CheckFailMoving); }
	static void Arm(ASFCommander* Commander) { Commander->CommandAction = ECommandAction::BUILD; }
	static bool IsBuilding(ASFCommander* Commander) { return Commander->CommandAction == ECommandAction::BUILD; }
	static void Complete(AUnitAIController* AI, EPathFollowingResult::Type Result)
	{ AI->OnMoveCompleted(FAIRequestID(1), FPathFollowingResult(Result, 0)); }
};

// Fixtures skip asset-dependent building construction and UI initialization.
// All targeting, damage, queue validation and pulse updates use production methods.
struct FSFForcedAttackTestAccess
{
	static void Diagnose(AUnitBase* U)
	{
		UE_LOG(LogTemp, Display, TEXT("COMBAT_PROBE %s target=%s state=%d range=%.1f distance=%.1f los=%d hold=%.2f cooldown=%.2f hits=%d/%d pending=%s montage=%s projectile=%s capsule=%.1f overlap=%d"),
			*U->GetName(), *GetNameSafe(U->CurrentTarget), int32(U->CombatState), U->GetBaseRange(U->CurrentTarget), U->GetDistanceToTargetEdge(U->CurrentTarget), U->HasLineOfSight(U->CurrentTarget),
			U->AttackAnimHoldRemaining, U->AttackCooldownRemaining, U->AppliedAttackHitCount, U->AttackHitCount, *GetNameSafe(U->PendingAttackTarget.Get()), *GetNameSafe(U->AttackMontage), *GetNameSafe(U->ProjectileClass.Get()), U->GetCapsuleComponent()->GetScaledCapsuleRadius(), U->GetCapsuleComponent()->GetGenerateOverlapEvents());
		if (U->AttackMontage) for (const auto& N : U->AttackMontage->Notifies)
			UE_LOG(LogTemp, Display, TEXT("COMBAT_NOTIFY %s %s"), *U->GetName(), *GetNameSafe(N.Notify));
	}
	static void SetRetreat(AUnitBase* Unit, bool Moving)
	{ Unit->SetWaitingForGroupMove(true); Unit->bHasCommandedDestination = Moving; }
	static void CheckRetreat(ASFInGamePlayerController* PC, AUnitBase* Arrived, AUnitBase* Straggler)
	{ FSFForcedMoveGroup Group; Group.Units = {Arrived, Straggler}; PC->ActiveForcedMoveGroups.Add(Group); PC->CheckForcedMoveGroups(); }
	static AActor* Forced(AUnitBase* Unit) { return Unit->ForcedTarget; }
	static float Recovery(AUnitBase* Unit) { return Unit->DirectApproachRemaining; }
	static void TickUnit(AUnitBase* Unit, float Delta) { Unit->Tick(Delta); }
	static void ArmMissingNotify(AUnitBase* U, AActor* Target)
	{ U->CurrentTarget = Target; U->PendingAttackTarget = Target; U->AppliedAttackHitCount = 0; U->AttackAnimHoldRemaining = .01f; U->AttackCooldownRemaining = 10.f; U->CombatState = EUnitCombatState::Attacking; }
	static void EnableLeash(AUnitBase* Unit) { Unit->bReturnToHoldLocation = true; Unit->bReturningToHold = true; Unit->SetHoldLocationToCurrent(); }
	static void PrepareUnit(AUnitBase* Unit) { Unit->bRequireLineOfSight = false; }
	static UAnimMontage* PrepareNotifyAttack(AUnitBase* Unit, AActor* Target)
	{
		Unit->PendingAttackTarget = Target;
		Unit->AppliedAttackHitCount = 0;
		Unit->bRequireLineOfSight = false;
		return Unit->AttackMontage;
	}
	static bool NextCommand(AUnitBase* Unit) { return Unit->ExecuteNextQueuedCommand(); }
	static bool IsAttackMove(AUnitBase* Unit) { return Unit->bCommandIsAttackMove; }
	static void SelectForCommand(USFInGameHUDWidget* HUD, AActor* Actor) { HUD->SelectedActors.Add(Actor); }
	static void SetMaterials(USFInGameHUDWidget* HUD, UMaterialInterface* Red, UMaterialInterface* Green)
	{
		HUD->EnemyHoverIndicatorMaterial = Red;
		HUD->SelectionIndicatorMaterial = Green;
	}
	static bool IsEnemy(USFInGameHUDWidget* HUD, AActor* Actor) { return HUD->IsValidEnemyHoverTarget(Actor); }
	static void Select(USFInGameHUDWidget* HUD, AActor* Actor)
	{
		HUD->SelectedActor = Actor;
		HUD->ShowSelectionIndicator(Actor);
	}
	static UDecalComponent* Selection(USFInGameHUDWidget* HUD, AActor* Actor)
	{
		const auto* Indicator = HUD->SelectionIndicatorComponents.Find(Actor);
		return Indicator ? Indicator->Get() : nullptr;
	}
	static UNiagaraComponent* Ring(USFInGameHUDWidget* HUD) { return HUD->EnemyHoverIndicator; }
	static const TArray<TObjectPtr<UMaterialInstanceDynamic>>& RingMaterials(USFInGameHUDWidget* HUD) { return HUD->EnemyHoverDynamicMaterials; }
	static void Step(USFInGameHUDWidget* HUD, float Delta) { HUD->UpdateEnemyHoverAttackFeedback(Delta); }
	static float QuarterDuration(USFInGameHUDWidget* HUD) { return HUD->EnemyHoverAttackPulseDuration * .5f; }
};

static FAutoConsoleCommandWithWorld CombatDiagnose(TEXT("SF.Combat.Diagnose"), TEXT("Read-only diagnosis of tagged PIE combat probes."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (!World || World->WorldType != EWorldType::PIE) return;
		for (TActorIterator<AUnitBase> It(World); It; ++It)
			if (It->ActorHasTag(TEXT("CombatFeedback"))) FSFForcedAttackTestAccess::Diagnose(*It);
	}));

namespace SFForcedAttackTests
{
	struct FWorldFixture
	{
		UWorld* World;
		FWorldFixture()
		{
			UWorld::InitializationValues Values;
			Values.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
		}
		~FWorldFixture() { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); }
		void PrepareTarget(AActor* Actor, int32 Team)
		{
			UHealthComponent* Health = Actor->FindComponentByClass<UHealthComponent>();
			Health->InitializeHealth(100.f);
			UCombatTargetComponent* Target = Actor->FindComponentByClass<UCombatTargetComponent>();
			// Bind the fixture health without starting asset loading or game mode logic.
			FindFProperty<FObjectPropertyBase>(UCombatTargetComponent::StaticClass(), TEXT("HealthComponent"))
				->SetObjectPropertyValue_InContainer(Target, Health);
			Target->SetTeamIndex(Team);
			World->GetSubsystem<USFCombatSubsystem>()->RegisterTarget(Target);
		}
		AUnitBase* Unit(int32 Team, FVector Position)
		{
			AUnitBase* Result = World->SpawnActor<AUnitBase>(Position, FRotator::ZeroRotator);
			PrepareTarget(Result, Team);
			FSFForcedAttackTestAccess::PrepareUnit(Result);
			return Result;
		}
		ASFProductionBuilding* Building(int32 Team, FVector Position)
		{
			ASFProductionBuilding* Result = World->SpawnActor<ASFProductionBuilding>(Position, FRotator::ZeroRotator);
			PrepareTarget(Result, Team);
			SetBuildingState(Result, EBuildingState::Completed);
			UBoxComponent* Footprint = NewObject<UBoxComponent>(Result);
			Footprint->SetupAttachment(Result->GetRootComponent());
			Footprint->SetBoxExtent(FVector(250.f, 300.f, 100.f));
			Footprint->RegisterComponent();
			return Result;
		}
		static void SetBuildingState(ASFBaseBuilding* Building, EBuildingState State)
		{
			FEnumProperty* Property = FindFProperty<FEnumProperty>(ASFBaseBuilding::StaticClass(), TEXT("CurrentBuildingState"));
			Property->GetUnderlyingProperty()->SetIntPropertyValue(Property->ContainerPtrToValuePtr<void>(Building), static_cast<uint64>(State));
		}
	};
	float Health(AActor* Actor) { return Actor->FindComponentByClass<UHealthComponent>()->GetCurrentHealth(); }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFForcedAttackTargetTest, "StrikeForge.ForcedAttack.TargetsAndDamage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFForcedAttackTargetTest::RunTest(const FString& Parameters)
{
	using namespace SFForcedAttackTests;
	FWorldFixture Fixture;
	AUnitBase* Attacker = Fixture.Unit(0, FVector::ZeroVector);
	AUnitBase* Ally = Fixture.Unit(0, FVector(200., 0., 0.));
	ASFProductionBuilding* AllyBuilding = Fixture.Building(0, FVector(400., 0., 0.));
	ASFProductionBuilding* EnemyBuilding = Fixture.Building(1, FVector(600., 0., 0.));
	TestFalse(TEXT("Self attack is rejected"), Attacker->CanForceAttackTarget(Attacker));
	for (AActor* Target : TArray<AActor*>{Ally, AllyBuilding, EnemyBuilding})
	{
		TestTrue(TEXT("Allied units and both teams' buildings accept explicit targets"), Attacker->CanForceAttackTarget(Target));
		Attacker->ExecuteCommand(ESFUnitCommand::Attack, Target->GetActorLocation(), Target);
		const float Before = Health(Target);
		Attacker->PerformAttack(Target);
		TestTrue(TEXT("Explicit target receives actual combat damage"), Health(Target) < Before);
	}
	Attacker->CommandStop();
	const float Before = Health(Ally);
	Attacker->PerformAttack(Ally);
	TestEqual(TEXT("Clearing the forced command disables friendly damage"), Health(Ally), Before);
	TArray<AActor*> AutomaticTargets;
	Fixture.World->GetSubsystem<USFCombatSubsystem>()->GetHostilesInRadius(FVector::ZeroVector, 0, 1200.f, AutomaticTargets);
	TestFalse(TEXT("Automatic acquisition excludes allied units"), AutomaticTargets.Contains(Ally));
	TestFalse(TEXT("Automatic acquisition excludes allied buildings"), AutomaticTargets.Contains(AllyBuilding));
	TestTrue(TEXT("Automatic acquisition retains enemy buildings"), AutomaticTargets.Contains(EnemyBuilding));
	Ally->SetActorLocation(FVector(3000., 0., 0.));
	TestFalse(TEXT("Personal sight still limits explicit attack"), Attacker->CanForceAttackTarget(Ally));
	Ally->SetActorLocation(FVector(200., 0., 0.));
	Fixture.SetBuildingState(AllyBuilding, EBuildingState::Preview);
	TestFalse(TEXT("Building preview cannot be attacked"), Attacker->CanForceAttackTarget(AllyBuilding));
	Fixture.SetBuildingState(AllyBuilding, EBuildingState::UnderConstruction);
	TestTrue(TEXT("An actual building under construction can be attacked"), Attacker->CanForceAttackTarget(AllyBuilding));
	Ally->FindComponentByClass<UHealthComponent>()->ApplyDamage(100.f);
	TestFalse(TEXT("Dead units cannot be attacked"), Attacker->CanForceAttackTarget(Ally));
	Attacker->CommandMoveTo(FVector(100., 100., 0.));
	Attacker->ExecuteCommandWithQueue(ESFUnitCommand::Attack, AllyBuilding->GetActorLocation(), AllyBuilding, true);
	TestTrue(TEXT("A queued friendly attack survives target validation"), FSFForcedAttackTestAccess::NextCommand(Attacker));
	const float BuildingBefore = Health(AllyBuilding);
	Attacker->PerformAttack(AllyBuilding);
	TestTrue(TEXT("Queued friendly attack actually deals damage"), Health(AllyBuilding) < BuildingBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFCombatResponseTest, "StrikeForge.ForcedAttack.SharedVisionAndRetreat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFCombatResponseTest::RunTest(const FString& Parameters)
{
	using namespace SFForcedAttackTests;
	FWorldFixture F;
	auto* PC = F.World->SpawnActor<ASFInGamePlayerController>(); PC->SetMyTeamIndex(0);
	auto* Unit = F.Unit(0, FVector::ZeroVector);
	auto* Scout = F.Unit(0, FVector(2800,0,0));
	auto* Enemy = F.Unit(1, FVector(3000,0,0));
	auto* Fog = F.World->GetSubsystem<USFFogOfWarSubsystem>();
	Fog->OnWorldBeginPlay(*F.World);
	Fog->RegisterSource(Unit->FindComponentByClass<UCombatTargetComponent>());
	Fog->RegisterSource(Scout->FindComponentByClass<UCombatTargetComponent>());
	Fog->Tick(0.1f);
	TestTrue(TEXT("Shared vision accepts a distant explicit enemy target"), Unit->CanForceAttackTarget(Enemy));
	TestNull(TEXT("Shared vision does not enlarge automatic aggro radius"), Unit->FindBestTarget());
	FSFForcedAttackTestAccess::SetRetreat(Unit, false);
	FSFForcedAttackTestAccess::EnableLeash(Unit);
	Unit->CommandAttackTarget(Enemy);
	TestFalse(TEXT("Explicit attack releases retreat waiting"), Unit->IsWaitingForGroupMove());
	TestTrue(TEXT("Explicit target retained"), FSFForcedAttackTestAccess::Forced(Unit) == Enemy);
	TestEqual(TEXT("Remote order alone does not damage target"), Health(Enemy), 100.f);
	Fog->UnregisterSource(Scout->FindComponentByClass<UCombatTargetComponent>()); Fog->Tick(0.1f);
	TestFalse(TEXT("Enemy hidden from the whole team cannot be targeted"), Unit->CanForceAttackTarget(Enemy));

	FSFForcedAttackTestAccess::SetRetreat(Unit, false);
	FSFForcedAttackTestAccess::SetRetreat(Scout, true);
	FSFForcedAttackTestAccess::CheckRetreat(PC, Unit, Scout);
	TestFalse(TEXT("Arrived unit resumes combat without waiting for a straggler"), Unit->IsWaitingForGroupMove());
	TestTrue(TEXT("Moving unit still obeys retreat"), Scout->IsWaitingForGroupMove());
	Unit->CommandAttackMove(FVector(1000,0,0));
	TestFalse(TEXT("Attack move is not stuck in retreat wait"), Unit->IsWaitingForGroupMove());
	Enemy->SetActorLocation(FVector(100,0,0));
	// The spatial grid is cached per engine frame; this synchronous fixture teleports within one frame.
	F.World->GetSubsystem<USFCombatSubsystem>()->UnregisterTarget(Enemy->FindComponentByClass<UCombatTargetComponent>());
	F.World->GetSubsystem<USFCombatSubsystem>()->RegisterTarget(Enemy->FindComponentByClass<UCombatTargetComponent>());
	Unit->CommandHoldGround();
	TestTrue(TEXT("Hold position acquires a target in weapon range"), Unit->FindBestTarget() == Enemy);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFMissingAttackNotifyTest, "StrikeForge.ForcedAttack.MissingNotifyFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFMissingAttackNotifyTest::RunTest(const FString& Parameters)
{
	SFForcedAttackTests::FWorldFixture F;
	auto* Unit = F.Unit(0, FVector::ZeroVector);
	auto* Enemy = F.Unit(1, FVector(260,0,0));
	FSFForcedAttackTestAccess::ArmMissingNotify(Unit, Enemy);
	FSFForcedAttackTestAccess::TickUnit(Unit, .1f);
	const float After = SFForcedAttackTests::Health(Enemy);
	TestTrue(TEXT("Finished attack without notify applies damage at actual weapon range"), After < 100.f);
	Unit->HandleAttackHit(1.f);
	TestEqual(TEXT("Late notify cannot duplicate fallback damage"), SFForcedAttackTests::Health(Enemy), After);
	FSFForcedAttackTestAccess::ArmMissingNotify(Unit, Enemy);
	Unit->CommandStop();
	FSFForcedAttackTestAccess::TickUnit(Unit, .1f);
	TestEqual(TEXT("Canceled attack does not apply a delayed fallback"), SFForcedAttackTests::Health(Enemy), After);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFUnitDamagedBarTest, "StrikeForge.HealthBar.DamageHealDeath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFUnitDamagedBarTest::RunTest(const FString& Parameters)
{
	using namespace SFForcedAttackTests;
	FWorldFixture F;
	for (int32 Team : {0, 1, 3})
	{
		auto* Unit = F.Unit(Team, FVector(Team * 500,0,100));
		Unit->DispatchBeginPlay();
		auto* Bar = Unit->FindComponentByClass<USFUnitHealthBarComponent>();
		if (!TestNotNull(TEXT("Every faction has a health bar component"), Bar)) return false;
		auto* HP = Unit->GetHealthComponent(); HP->InitializeHealth(100000.f);
		TestFalse(TEXT("Full health is hidden"), Bar->IsVisible());
		HP->ApplyDamage(1.f);
		TestTrue(TEXT("One HP damage appears immediately even on a large health pool"), Bar->IsVisible());
		HP->IncreaseCurrentHealth(1.f);
		TestFalse(TEXT("Full healing hides bar"), Bar->IsVisible());
		HP->ApplyDamage(100000.f);
		TestFalse(TEXT("Dead unit hides bar"), Bar->IsVisible());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFCommanderDamagedBarTest, "StrikeForge.HealthBar.CommanderSpawnDamageHeal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFCommanderDamagedBarTest::RunTest(const FString& Parameters)
{
	using namespace SFForcedAttackTests;
	FWorldFixture F;
	for (const TCHAR* Path : {
		TEXT("/Game/Game/Race_Etherial/Character/Unit_Commander/BP_Ether_Commander.BP_Ether_Commander_C"),
		TEXT("/Game/Game/Race_Beast/Character/Unit_Commander/BP_Beast_Commander.BP_Beast_Commander_C")})
	{
		UClass* Class = LoadClass<ASFCommander>(nullptr, Path);
		if (!TestNotNull(TEXT("Production commander class loads"), Class)) return false;
		for (bool bPlayerCommander : {false, true})
		{
			FActorSpawnParameters Spawn;
			Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			auto* Commander = F.World->SpawnActor<ASFCommander>(Class, FVector::ZeroVector, FRotator::ZeroRotator, Spawn);
			if (!TestNotNull(TEXT("Commander spawns"), Commander)) return false;
			if (bPlayerCommander) Commander->InitializePlayerCommander(0);
			Commander->DispatchBeginPlay();
			auto* HP = Commander->FindComponentByClass<UHealthComponent>();
			auto* Bar = Commander->FindComponentByClass<USFUnitHealthBarComponent>();
			TestEqual(TEXT("Commander starts at full health without controller registration"), HP->GetCurrentHealth(), HP->GetMaxHealth());
			TestEqual(TEXT("Default commander health remains 300"), HP->GetMaxHealth(), 300.f);
			TestFalse(TEXT("Undamaged commander hides overhead bar"), Bar->IsVisible());
			HP->ApplyDamage(1.f);
			TestTrue(TEXT("One HP of real damage shows commander bar"), Bar->IsVisible());
			HP->IncreaseCurrentHealth(1.f);
			TestFalse(TEXT("Fully healed commander hides bar again"), Bar->IsVisible());
			Commander->Destroy();
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFForcedAttackRingTest, "StrikeForge.ForcedAttack.RingsAndHover",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFForcedAttackRingTest::RunTest(const FString& Parameters)
{
	using namespace SFForcedAttackTests;
	FWorldFixture Fixture;
	ASFInGamePlayerController* PC = Fixture.World->SpawnActor<ASFInGamePlayerController>();
	PC->SetMyTeamIndex(0);
	AUnitBase* Ally = Fixture.Unit(0, FVector::ZeroVector);
	Ally->SetActorScale3D(FVector(.3f));
	ASFProductionBuilding* AllyBuilding = Fixture.Building(0, FVector(400., 0., 0.));
	ASFProductionBuilding* EnemyBuilding = Fixture.Building(1, FVector(600., 0., 0.));
	// Reproduce map floors above the lower half of the building collision box.
	AActor* Floor = Fixture.World->SpawnActor<AActor>();
	UBoxComponent* Ground = NewObject<UBoxComponent>(Floor);
	Floor->SetRootComponent(Ground);
	Ground->SetBoxExtent(FVector(2000.f, 2000.f, 10.f));
	Ground->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Ground->SetCollisionResponseToAllChannels(ECR_Ignore);
	Ground->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
	Ground->RegisterComponent();
	Ground->SetWorldLocation(FVector(0.f, 0.f, -10.f));
	USFFogOfWarSubsystem* Fog = Fixture.World->GetSubsystem<USFFogOfWarSubsystem>();
	Fog->OnWorldBeginPlay(*Fixture.World);
	Fog->RegisterSource(Ally->FindComponentByClass<UCombatTargetComponent>());
	Fog->Tick(1.f / 60.f);
	USFInGameHUDWidget* HUD = NewObject<USFInGameHUDWidget>(PC);
	UMaterialInterface* Red = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Game/IN_UI/WorldFeedback/Selection/Material/MI_EnemyHoverRing.MI_EnemyHoverRing"));
	UMaterialInterface* Green = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Game/IN_UI/WorldFeedback/Selection/Material/MI_SFSelectionRing_Player.MI_SFSelectionRing_Player"));
	if (!TestNotNull(TEXT("Existing red ring material"), Red) || !TestNotNull(TEXT("Existing friendly ring material"), Green)) return false;
	FSFForcedAttackTestAccess::SetMaterials(HUD, Red, Green);
	TestTrue(TEXT("Enemy building is a normal red hover target"), FSFForcedAttackTestAccess::IsEnemy(HUD, EnemyBuilding));
	TestFalse(TEXT("Friendly building is not a normal red hover target"), FSFForcedAttackTestAccess::IsEnemy(HUD, AllyBuilding));
	FSFForcedAttackTestAccess::Select(HUD, EnemyBuilding);
	UDecalComponent* Selection = FSFForcedAttackTestAccess::Selection(HUD, EnemyBuilding);
	if (!TestNotNull(TEXT("Enemy building selection ring"), Selection)) return false;
	TestTrue(TEXT("Enemy building normal selection uses red material"), Selection->GetDecalMaterial() == Red);
	float UnitRadius = 0.f;
	for (AActor* Target : TArray<AActor*>{Ally, AllyBuilding, EnemyBuilding})
	{
		HUD->PlayEnemyHoverAttackFeedback(Target);
		UNiagaraComponent* Ring = FSFForcedAttackTestAccess::Ring(HUD);
		if (!TestNotNull(TEXT("Target command creates a ring without prior hover"), Ring)) return false;
		TestEqual(TEXT("Hover and forced attack use the rounded purchased marker"), Ring->GetAsset()->GetName(), FString(TEXT("NS_SFHoverRound")));
		const auto& Materials = FSFForcedAttackTestAccess::RingMaterials(HUD);
		TestTrue(TEXT("All renderer materials receive the palette"), Materials.Num() >= 2);
		for (UMaterialInstanceDynamic* Dynamic : Materials)
		{
			FLinearColor Color;
			TestTrue(TEXT("Material exposes the feedback color"), Dynamic->GetVectorParameterValue(TEXT("FeedbackColor"), Color));
			TestTrue(TEXT("Both teams' explicit attacks retain red feedback"), Color.Equals(FLinearColor::Red));
		}
		const float Base = Ring->GetComponentScale().Y;
		TestTrue(TEXT("Actual terrain keeps Niagara above the floor, not below collision bounds"),
			FMath::IsNearlyEqual(Ring->GetComponentLocation().Z, 8.f, .1f));
		if (Target == Ally)
			TestTrue(TEXT("Small BP actor scale does not rescale the attached marker"),
				FMath::IsNearlyEqual(Base, 40.f / 140.f, .001f));
		if (Target == Ally) UnitRadius = Base;
		else TestTrue(TEXT("Building feedback follows its footprint"), Base > UnitRadius);
		const float Quarter = FSFForcedAttackTestAccess::QuarterDuration(HUD);
		FSFForcedAttackTestAccess::Step(HUD, Quarter);
		TestTrue(TEXT("First pulse grows"), Ring->GetComponentScale().Y > Base * 1.2f);
		FSFForcedAttackTestAccess::Step(HUD, Quarter);
		TestTrue(TEXT("First pulse shrinks"), FMath::IsNearlyEqual(Ring->GetComponentScale().Y, Base, .01f));
		FSFForcedAttackTestAccess::Step(HUD, Quarter);
		TestTrue(TEXT("Second pulse grows"), Ring->GetComponentScale().Y > Base * 1.2f);
		FSFForcedAttackTestAccess::Step(HUD, Quarter + .01f);
		TestNull(TEXT("Feedback ends after exactly two pulses"), FSFForcedAttackTestAccess::Ring(HUD));
	}
	TestNotNull(TEXT("Selected enemy building ring is restored after feedback"), FSFForcedAttackTestAccess::Selection(HUD, EnemyBuilding));
	HUD->PlayEnemyHoverAttackFeedback(Ally);
	Ally->FindComponentByClass<UHealthComponent>()->ApplyDamage(100.f);
	FSFForcedAttackTestAccess::Step(HUD, .01f);
	TestNull(TEXT("Death cancels the temporary ring"), FSFForcedAttackTestAccess::Ring(HUD));
	HUD->PlayEnemyHoverAttackFeedback(EnemyBuilding);
	EnemyBuilding->SetActorLocation(FVector(5000., 0., 0.));
	FSFForcedAttackTestAccess::Step(HUD, .01f);
	TestNull(TEXT("Losing sight cancels the temporary ring"), FSFForcedAttackTestAccess::Ring(HUD));
	TestNull(TEXT("Hidden enemy selection must not be restored through fog"), FSFForcedAttackTestAccess::Selection(HUD, EnemyBuilding));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFCommandClickPaletteTest, "StrikeForge.ForcedAttack.ClickPaletteAndLifetime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFCommandClickPaletteTest::RunTest(const FString& Parameters)
{
	SFForcedAttackTests::FWorldFixture Fixture;
	UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr,
		TEXT("/Game/Game/IN_UI/WorldFeedback/Niagara/NS_SFClickMovement.NS_SFClickMovement"));
	if (!TestNotNull(TEXT("Purchased click movement variant exists"), System)) return false;
	TArray<TObjectPtr<UMaterialInstanceDynamic>> Previous;
	for (const FLinearColor Color : { FLinearColor(0.f, .5f, .5f), FLinearColor::Red })
	{
		TArray<TObjectPtr<UMaterialInstanceDynamic>> Materials;
		UNiagaraComponent* Effect = SFCommandFeedback::Spawn(Fixture.World, System,
			FVector::ZeroVector, FVector(.7f), Color, false, &Materials);
		if (!TestNotNull(TEXT("Click feedback spawns"), Effect)) return false;
		TestTrue(TEXT("Arrow, circle and glow receive material overrides"), Materials.Num() >= 3);
		for (UMaterialInstanceDynamic* Material : Materials)
		{
			FLinearColor Actual;
			Material->GetVectorParameterValue(TEXT("FeedbackColor"), Actual);
			TestTrue(TEXT("Move and attack keep their own palette"), Actual.Equals(Color));
			TestFalse(TEXT("Simultaneous clicks do not share mutable materials"), Previous.Contains(Material));
		}
		Previous.Append(Materials);
		Effect->SetForceSolo(true);
		Effect->AdvanceSimulation(300, 1.f / 60.f);
		TestTrue(TEXT("Click effect is finite and eligible for auto-destruction"), Effect->IsComplete());
		Effect->DestroyComponent();
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFWarBeastNotifyTest, "StrikeForge.Playtest.WarBeastDamage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFWarBeastNotifyTest::RunTest(const FString& Parameters)
{
	FEditorScriptExecutionGuard ScriptGuard;
	using namespace SFForcedAttackTests;
	FWorldFixture Fixture;
	UClass* Class = LoadClass<AUnitBase>(nullptr, TEXT("/Game/Core/Units/Beast_Fox/BP_Fox_WarBeast.BP_Fox_WarBeast_C"));
	if (!Class) return false;
	AUnitBase* Beast = Fixture.World->SpawnActor<AUnitBase>(Class, FVector::ZeroVector, FRotator::ZeroRotator);
	Fixture.PrepareTarget(Beast, 0);
	AUnitBase* Target = Fixture.Unit(1, FVector(160.f, 0.f, 0.f));
	Target->FindComponentByClass<UHealthComponent>()->InitializeHealth(1000.f);
	UAnimMontage* Montage = FSFForcedAttackTestAccess::PrepareNotifyAttack(Beast, Target);
	if (!TestNotNull(TEXT("Real WarBeast attack montage"), Montage)) return false;
	const float Before = Health(Target);
	for (const auto& Event : Montage->Notifies)
		if (Event.Notify) Event.Notify->Notify(Beast->GetMesh(), Montage, FAnimNotifyEventReference());
	TestTrue(TEXT("Actual WarBeast montage notifies reduce enemy health"), Health(Target) < Before);
	const float After = Health(Target);
	for (const auto& Event : Montage->Notifies)
		if (Event.Notify) Event.Notify->Notify(Beast->GetMesh(), Montage, FAnimNotifyEventReference());
	TestEqual(TEXT("Duplicate notifies cannot damage twice"), Health(Target), After);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFMinimapAttackTest, "StrikeForge.Playtest.MinimapAttackMove",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFMinimapAttackTest::RunTest(const FString& Parameters)
{
	using namespace SFForcedAttackTests;
	FWorldFixture Fixture;
	AUnitBase* Unit = Fixture.Unit(0, FVector::ZeroVector);
	ASFInGamePlayerController* PC = Fixture.World->SpawnActor<ASFInGamePlayerController>();
	PC->SetMyTeamIndex(0);
	USFInGameHUDWidget* HUD = NewObject<USFInGameHUDWidget>(PC);
	FSFForcedAttackTestAccess::SelectForCommand(HUD, Unit);
	FindFProperty<FObjectPropertyBase>(PC->GetClass(), TEXT("InGameHUDWidget"))->SetObjectPropertyValue_InContainer(PC, HUD);
	PC->BeginAttackCommand();
	TestTrue(TEXT("Minimap consumes the pending A command"), PC->ExecutePendingUnitCommandAtLocation(FVector(900, 900, 0)));
	TestTrue(TEXT("Ground attack becomes attack-move"), FSFForcedAttackTestAccess::IsAttackMove(Unit));
	TestFalse(TEXT("Next minimap click is free to move the camera"), PC->ExecutePendingUnitCommandAtLocation(FVector::ZeroVector));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFBuildMoveReplacementTest, "StrikeForge.Playtest.BuildMoveReplacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFBuildMoveReplacementTest::RunTest(const FString& Parameters)
{
	FEditorScriptExecutionGuard ScriptGuard;
	SFForcedAttackTests::FWorldFixture Fixture;
	auto* Commander = Fixture.World->SpawnActor<ASFCommander>();
	auto* AI = Fixture.World->SpawnActor<AUnitAIController>();
	AI->Possess(Commander);
	FSFPlaytestMovementAccess::Bind(AI, Commander);
	TestTrue(TEXT("Move completion callback is bound"), AI->OnCompleteMoving.IsBound());
	TestTrue(TEXT("Commander remains valid"), IsValid(Commander));
	FSFPlaytestMovementAccess::Arm(Commander);
	FSFPlaytestMovementAccess::Complete(AI, EPathFollowingResult::Aborted);
	TestTrue(TEXT("Replacing a move preserves the new build request"), FSFPlaytestMovementAccess::IsBuilding(Commander));
	FSFPlaytestMovementAccess::Complete(AI, EPathFollowingResult::Blocked);
	TestFalse(TEXT("A real path failure still cancels the build request"), FSFPlaytestMovementAccess::IsBuilding(Commander));
	return true;
}
#endif
