#if WITH_EDITOR
#include "HAL/IConsoleManager.h"
#include "Engine/World.h"
#include "Units/Commander/SFCommander.h"
#include "ActorComponent/Combat/CombatTargetComponent.h"
#include "InGame/Controller/SFInGamePlayerController.h"
#include "UObject/UnrealType.h"

// The Python PIE test cannot call Blueprint-internal deferred spawn functions.
static FAutoConsoleCommandWithWorld RespawnEnemyFixture(
    TEXT("SF.Commander.SpawnRespawnFixture"), TEXT("PIE QA: spawn the opposing race commander for respawn verification."),
    FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
    {
        if (!World || World->WorldType != EWorldType::PIE) return;
        auto* PC = World->GetFirstPlayerController<ASFInGamePlayerController>();
        auto* Ally = PC ? PC->GetMyCommander() : nullptr;
        if (!Ally) return;
        const TCHAR* Path = Ally->GetCommanderRace() == ERace::ETHERIAN
            ? TEXT("/Game/Game/Race_Beast/Character/Unit_Commander/BP_Beast_Commander.BP_Beast_Commander_C")
            : TEXT("/Game/Game/Race_Etherial/Character/Unit_Commander/BP_Ether_Commander.BP_Ether_Commander_C");
        auto* Class = LoadClass<ASFCommander>(nullptr, Path);
        if (!Class) return;
        const FTransform Transform(FRotator::ZeroRotator, Ally->GetActorLocation() + FVector(500, 0, 0));
        auto* Enemy = World->SpawnActorDeferred<ASFCommander>(Class, Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if (!Enemy) return;
        // Imported production blueprints can default to player ownership; this fixture is an NPC.
        if (auto* Ownership = FindFProperty<FBoolProperty>(ASFCommander::StaticClass(), TEXT("bMyCommander")))
            Ownership->SetPropertyValue_InContainer(Enemy, false);
        Enemy->FindComponentByClass<UCombatTargetComponent>()->SetTeamIndex(PC->GetMyTeamIndex() == 0 ? 1 : 0);
        Enemy->Tags.Add(TEXT("RespawnFixture"));
        Enemy->FinishSpawning(Transform);
    }));
#endif
