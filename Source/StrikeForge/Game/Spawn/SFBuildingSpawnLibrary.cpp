#include "SFBuildingSpawnLibrary.h"
#include "Datas/Building/SFBuildingDefinition.h"
#include "Engine/Engine.h"
#include "Logging/SFLog.h"

ASFBaseBuilding*
USFBuildingSpawnLibrary::SpawnBuildingFromDefinition(
    UObject* WorldContextObject,
    USFBuildingDefinition* InDefinition,
    int32 InTeamIndex,
    const FTransform& InSpawnTransform,
    ESFBuildingSpawnMode InSpawnMode)
{
    if (!IsValid(WorldContextObject) ||
        !IsValid(InDefinition))
    {
        return nullptr;
    }

    UWorld* World = GEngine
        ? GEngine->GetWorldFromContextObject(
            WorldContextObject,
            EGetWorldErrorMode::ReturnNull)
        : nullptr;

    if (!IsValid(World))
    {
        return nullptr;
    }

    UClass* SpawnClass =
    InDefinition->BuildingClass.Get();

    if (!IsValid(SpawnClass))
    {
        SF_LOG(
            Error,
            "[BuildingSpawn] BuildingClass가 비동기 준비되지 않았습니다. "
            "Definition=%s",
            *GetNameSafe(InDefinition));

        return nullptr;
    }

    ASFBaseBuilding* SpawnedBuilding =
        World->SpawnActorDeferred<ASFBaseBuilding>(
            SpawnClass,
            InSpawnTransform,
            nullptr,
            nullptr,
            ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
    

    if (!IsValid(SpawnedBuilding))
    {
        SF_LOG(
            Error,
            "[BuildingSpawn] Deferred Spawn 실패. "
            "Definition=%s",
            *GetNameSafe(InDefinition));

        return nullptr;
    }
    
    const ASFBaseBuilding* BuildingCDO =
        SpawnClass->GetDefaultObject<ASFBaseBuilding>();

    const UStaticMeshComponent* DefaultMeshComponent =
        IsValid(BuildingCDO)
            ? BuildingCDO->FindComponentByClass<UStaticMeshComponent>()
            : nullptr;

    UStaticMeshComponent* SpawnedMeshComponent =
        SpawnedBuilding->FindComponentByClass<UStaticMeshComponent>();

    if (!IsValid(DefaultMeshComponent) ||
        !IsValid(SpawnedMeshComponent))
    {
        SF_LOG(
            Error,
            "[BuildingSpawn] StaticMeshComponent가 없습니다. Class=%s",
            *GetNameSafe(SpawnClass));

        SpawnedBuilding->Destroy();
        return nullptr;
    }
    
    SpawnedMeshComponent->SetRelativeScale3D(
    DefaultMeshComponent->GetRelativeScale3D());

    if (!SpawnedBuilding->SetBuildingDefinition(InDefinition))
    {
        SF_LOG(
        Error,
        "[BuildingSpawn] BuildingDefinition 주입 실패. Definition=%s",
        *GetNameSafe(InDefinition));
        
        SpawnedBuilding->Destroy();
        return nullptr;
    }
    
    SpawnedBuilding->SetSpawnMode(InSpawnMode);
    SpawnedBuilding->SetMyTeamIndex(InTeamIndex);

    /*
     * 선택 UI가 완전히 DA로 전환되기 전까지 사용하는 브리지.
     */
    const FName SelectionRowName =
        InDefinition->SelectionUIRowName.IsNone()
            ? InDefinition->BuildingId
            : InDefinition->SelectionUIRowName;

    SpawnedBuilding->SetRowName(SelectionRowName);

    SpawnedBuilding->FinishSpawning(InSpawnTransform);

    // 그리드등록에 실패하면 destroy됨
    if (!IsValid(SpawnedBuilding) || SpawnedBuilding->IsActorBeingDestroyed())
    {
        SF_LOG(
            Error,
            "[BuildingSpawn] BeginPlay 초기화 중 건물이 제거됐습니다. "
            "Definition=%s",
            *GetNameSafe(InDefinition));

        return nullptr;
    }
    
    SF_LOG(
        Log,
        "[BuildingSpawn] 생성 완료. "
        "Building=%s, Team=%d, Definition=%s",
        *GetNameSafe(SpawnedBuilding),
        InTeamIndex,
        *GetNameSafe(InDefinition));

    return SpawnedBuilding;
}