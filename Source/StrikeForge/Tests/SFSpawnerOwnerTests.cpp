#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Components/SceneComponent.h"
#include "ActorComponent/Stat/HealthComponent.h"
#include "ActorComponent/Combat/CombatTargetComponent.h"
#include "Units/Spawner/UnitSpawner.h"
#include "UObject/Script.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFSpawnerOwnerLifecycleTest,
	"StrikeForge.Spawner.OwnerBuildingStopsProduction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFSpawnerOwnerLifecycleTest::RunTest(const FString& Parameters)
{
	FEditorScriptExecutionGuard ScriptGuard;
	UWorld::InitializationValues Values;
	Values.AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
	if (!TestNotNull(TEXT("Test world"), World)) return false;

	auto MakeBuilding = [World](float X)
	{
		AActor* Building = World->SpawnActor<AActor>();
		USceneComponent* Root = NewObject<USceneComponent>(Building);
		Building->SetRootComponent(Root);
		Root->RegisterComponent();
		Building->SetActorLocation(FVector(X, 0.f, 0.f));
		UHealthComponent* Health = NewObject<UHealthComponent>(Building);
		Health->RegisterComponent();
		Health->InitializeHealth(100.f);
		UCombatTargetComponent* Target = NewObject<UCombatTargetComponent>(Building);
		Target->SetTeamIndex(1);
		Target->SetTargetType(ETargetType::ProductionBuilding);
		Target->RegisterComponent();
		Building->DispatchBeginPlay();
		return TPair<AActor*, UHealthComponent*>(Building, Health);
	};

	auto MakeSpawner = [World]()
	{
		AUnitSpawner* Spawner = World->SpawnActor<AUnitSpawner>();
		FindFProperty<FBoolProperty>(AUnitSpawner::StaticClass(), TEXT("bStartActive"))
			->SetPropertyValue_InContainer(Spawner, false);
		FindFProperty<FBoolProperty>(AUnitSpawner::StaticClass(), TEXT("bRequireOwnerBuilding"))
			->SetPropertyValue_InContainer(Spawner, true);
		Spawner->DispatchBeginPlay();
		return Spawner;
	};

	AUnitSpawner* DamageSpawner = MakeSpawner();
	auto DamageBuilding = MakeBuilding(3000.f);
	TestTrue(TEXT("Late building is explicitly linked"),
		DamageSpawner->AssignOwnerBuilding(DamageBuilding.Key));
	TestEqual(TEXT("Linked building is the owner"),
		DamageSpawner->GetOwnerBuilding(), DamageBuilding.Key);
	DamageBuilding.Value->ApplyDamage(100.f);
	TestTrue(TEXT("Combat destruction shuts down the spawner"),
		DamageSpawner->IsShutDownByOwnerBuilding());
	TestNull(TEXT("Destroyed building cannot spawn units"), DamageSpawner->SpawnOne());

	AUnitSpawner* RemovedSpawner = MakeSpawner();
	auto RemovedBuilding = MakeBuilding(5000.f);
	TestTrue(TEXT("Second building is explicitly linked"),
		RemovedSpawner->AssignOwnerBuilding(RemovedBuilding.Key));
	RemovedBuilding.Key->Destroy();
	TestTrue(TEXT("Direct actor destruction also shuts down the spawner"),
		RemovedSpawner->IsShutDownByOwnerBuilding());
	World->DestroyWorld(false);
	return true;
}
