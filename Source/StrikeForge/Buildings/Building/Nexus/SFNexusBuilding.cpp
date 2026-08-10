#include "SFNexusBuilding.h"

#include "Logging/SFLog.h"
// 안쓰는 include 제거해도 상관없습니다.
//#include "Game/GameMode/SFGameMode.h"
//#include "Kismet/GameplayStatics.h"

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
	
	SetBuildingState(EBuildingState::Completed);
}

void ASFNexusBuilding::OnBuildingDestroyed()
{
	Super::OnBuildingDestroyed();

	SF_LOG(Log, "[SFNexusBuilding] Nexus destroyed : %s", *GetName());

	// CoreObjectiveComponent 부분에서 승리, 패배 전달 담당하므로 밑에 주석 제거해도 상관없습니다.
	//if (!HasAuthority()) return;
	//ASFGameMode* GameMode =	Cast<ASFGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
	//if (!IsValid(GameMode)) return;
	//
	//UE_LOG(LogTemp, Warning, TEXT("ANexusBuilding::OnBuildingDestroyed: Lose/Win"));
	//// win or lose > gamemode
}

void ASFNexusBuilding::InitInfo(const FBuildingAppearance* InData)
{
	Super::InitInfo(InData);
	
}
