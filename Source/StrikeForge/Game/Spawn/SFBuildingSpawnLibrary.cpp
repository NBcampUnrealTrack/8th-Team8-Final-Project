#include "SFBuildingSpawnLibrary.h"
#include "Datas/Building/SFBuildingDefinition.h"
#include "Engine/Engine.h"
#include "Logging/SFLog.h"

FRotator USFBuildingSpawnLibrary::GetConstructionRotation()
{
    return FRotator(0.f, 90.f, 0.f);
}

ASFBaseBuilding*
USFBuildingSpawnLibrary::SpawnBuildingFromDefinition(
    UObject* WorldContextObject,
    USFBuildingDefinition* InDefinition,
    int32 InTeamIndex,
    const FTransform& InSpawnTransform,
    ESFBuildingSpawnMode InSpawnMode,
    int32 InInitialTierIndex)
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

    FTransform SpawnTransform = InSpawnTransform;
    // Normalize authored non-Nexus starting buildings to the common facing.
    // Player placement already carries its stage-specific rotation from the controller.
    // Nexus facing is authored separately for each base.
    if (InSpawnMode != ESFBuildingSpawnMode::PlayerPlacedMode &&
        InDefinition->BuildingType != ESFBuildingType::Nexus)
    {
        SpawnTransform.SetRotation(GetConstructionRotation().Quaternion());
    }

    ASFBaseBuilding* SpawnedBuilding =
        World->SpawnActorDeferred<ASFBaseBuilding>(
            SpawnClass,
            SpawnTransform,
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
        SpawnedBuilding->Destroy();
        return nullptr;
    }

    if (!SpawnedBuilding->SetInitialTier(InInitialTierIndex))
    {
        SF_LOG(Error,
            "[BuildingSpawn] 시작 Tier 설정 실패. "
            "Definition=%s, Tier=%d",
            *GetNameSafe(InDefinition),
            InInitialTierIndex);

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

    SpawnedBuilding->FinishSpawning(SpawnTransform);

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
