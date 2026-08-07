#include "SFNexusBuilding.h"
#include "Game/GameMode/SFGameMode.h"
#include "Kismet/GameplayStatics.h"

ASFNexusBuilding::ASFNexusBuilding()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ASFNexusBuilding::BeginPlay()
{
	Super::BeginPlay();
	
	//if (!HasAuthority()) return;
	
	// get team index
	//
	// gamemode
	
	// repec =============================================
	// SetBuildingState(EBuildingState::Completed);
	// repec ============================================
}

void ASFNexusBuilding::OnBuildingDestroyed()
{
	Super::OnBuildingDestroyed();
	//if (!HasAuthority()) return;
	ASFGameMode* GameMode =	Cast<ASFGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
	if (!IsValid(GameMode)) return;
	
	UE_LOG(LogTemp, Warning, TEXT("ANexusBuilding::OnBuildingDestroyed: Lose/Win"));
	// win or lose > gamemode
}

void ASFNexusBuilding::InitInfo(const FBuildingAppearance* InData)
{
	Super::InitInfo(InData);
	
}
