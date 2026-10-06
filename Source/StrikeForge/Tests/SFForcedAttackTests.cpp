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

// Fixtures skip asset-dependent building construction and UI initialization.
// All targeting, damage, queue validation and pulse updates use production methods.
struct FSFForcedAttackTestAccess
{
	static void PrepareUnit(AUnitBase* Unit) { Unit->bRequireLineOfSight = false; }
	static bool NextCommand(AUnitBase* Unit) { return Unit->ExecuteNextQueuedCommand(); }
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

#endif
