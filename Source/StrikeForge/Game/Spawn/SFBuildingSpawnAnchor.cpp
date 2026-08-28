#include "SFBuildingSpawnAnchor.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Datas/Building/SFBuildingDefinition.h"
#include "Datas/Building/BuildingTierData/SFBuildingTierData.h"

ASFBuildingSpawnAnchor::ASFBuildingSpawnAnchor()
{
	PrimaryActorTick.bCanEverTick = false;
	
	SceneComponent = CreateDefaultSubobject<USceneComponent>("SceneComponent");
	SetRootComponent(SceneComponent);
	
#if WITH_EDITORONLY_DATA
	PreviewMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>("PreviewMeshComponent");
	PreviewMeshComponent->SetupAttachment(GetRootComponent());
	
	PreviewMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PreviewMeshComponent->SetGenerateOverlapEvents(false);
	PreviewMeshComponent->SetCanEverAffectNavigation(false);
	// 에디터 Viewport에서는 보이고 PIE에서는 보이지 않는다.
	PreviewMeshComponent->SetHiddenInGame(true);

#endif
}

#if WITH_EDITOR
void ASFBuildingSpawnAnchor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	
	RefreshEditorPreview();
}

void ASFBuildingSpawnAnchor::RefreshEditorPreview()
{
#if WITH_EDITORONLY_DATA

	if (!IsValid(PreviewMeshComponent))
	{
		return;
	}

	PreviewMeshComponent->SetStaticMesh(nullptr);

	USFBuildingDefinition* Definition =	PreviewDefinition.LoadSynchronous();
	if (!IsValid(Definition))
	{
		return;
	}

	const FSFBuildingTierData* FirstTier = Definition->FindTier(0);
	if (!FirstTier)
	{
		return;
	}

	UStaticMesh* CompletedMesh = FirstTier->Visual.CompletedMesh.LoadSynchronous();
	PreviewMeshComponent->SetStaticMesh(CompletedMesh);

#endif
}
#endif
