#include "SFNexusBuilding.h"

#include "ActorComponent/Combat/CoreObjectiveComponent.h"
#include "Game/GameMode/SFGameMode.h"
#include "Logging/SFLog.h"

ASFNexusBuilding::ASFNexusBuilding(const FObjectInitializer& ObjectInitializer)
	// 베이스가 만드는 CombatTargetComponent 를 CoreObjective 로 교체한다.
	// 이래야 타겟 분류가 CoreBuilding 이 되어 전투 유닛의 진격 목표가 된다.
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UCoreObjectiveComponent>(TEXT("CombatTargetComponent")))
	, bFinishStageOnDestroyed(true)
{
	PrimaryActorTick.bCanEverTick = false;
}

FSFSelectableInfo ASFNexusBuilding::GetSelectableInfo_Implementation()
{
	FSFSelectableInfo Info =  Super::GetSelectableInfo_Implementation();
	Info.SelectableType = ESFSelectableType::MainBuilding;
	return Info;
}

void ASFNexusBuilding::BeginPlay()
{
	Super::BeginPlay();

	// 컴포넌트를 BP 에서 지우거나 다른 클래스로 바꾸면
	// 유닛이 이 건물을 진격 목표로 인식하지 못한다.
	// 조용히 동작을 멈추므로 시작할 때 한 번 알린다.
	if (!FindComponentByClass<UCoreObjectiveComponent>())
	{
		SF_LOG(Error,
			"[SFNexusBuilding] %s : CoreObjectiveComponent 가 없습니다. "
			"전투 유닛이 이 건물을 진격 목표로 인식하지 못합니다.",
			*GetName());
	}
	
	//if (GetTeamIndex() == 1) return;
	
	ASFGameState* SFGS = GetWorld()->GetGameState<ASFGameState>();
	if (!IsValid(SFGS)) return;
	
	if (!SFGS->RegisterNexus(GetTeamIndex(), this))
	{
		SF_LOG(Error, "[SFNexusBuilding] %s 등록 실패. TeamIndex=%d", *GetName(), GetTeamIndex());
	}
	
}

void ASFNexusBuilding::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ASFGameState* SFGS = GetWorld()->GetGameState<ASFGameState>())
	{
		SFGS->UnregisterNexus(GetTeamIndex(), this);
	}
	Super::EndPlay(EndPlayReason);
}

// ─────────────────────────────────────────────────────────
// 파괴 — 승패 판정
//
// 호출 경로
//   피해 → UHealthComponent::ApplyDamage
//        → OnHealthIsZero
//        → ASFBaseBuilding::HandleHealthZero
//        → OnBuildingDestroyed()   ← 여기
// ─────────────────────────────────────────────────────────
void ASFNexusBuilding::OnBuildingDestroyed()
{
	Super::OnBuildingDestroyed();

	const int32 DestroyedTeam = GetTeamIndex();

	SF_LOG(Warning, "[SFNexusBuilding] %s 파괴 (Team %d)", *GetName(), DestroyedTeam);

	// 연출 훅. 스테이지 종료 여부와 무관하게 항상 호출한다.
	OnNexusDestroyed(DestroyedTeam);

	if (!bFinishStageOnDestroyed)
	{
		SF_LOG(Warning, "[SFNexusBuilding] bFinishStageOnDestroyed 가 꺼져 있어 스테이지를 끝내지 않습니다.");
		return;
	}

	const UWorld* World = GetWorld();
	ASFGameMode* GameMode = IsValid(World) ? World->GetAuthGameMode<ASFGameMode>() : nullptr;

	// 조용히 빠져나가면 "로그가 안 찍힌다" 는 증상만 남아 원인 파악이 어렵다.
	if (!IsValid(GameMode))
	{
		SF_LOG(Error,
			"[SFNexusBuilding] ASFGameMode 를 찾지 못했습니다. "
			"World Settings 의 GameMode Override 를 확인하세요. 현재 GameMode : %s",
			World && World->GetAuthGameMode() ? *World->GetAuthGameMode()->GetClass()->GetName() : TEXT("None"));
		return;
	}

	// 어느 팀 코어가 부서졌는지만 넘긴다.
	// 승리인지 패배인지는 GameMode 가 PlayerTeamIndex 와 비교해 판단한다.
	if (!GameMode->ProcessCoreDestroyed(DestroyedTeam))
	{
		// GameMode 쪽에서 이미 사유를 로그로 남긴다.
		// 가장 흔한 원인은 스테이지가 Playing 상태가 아닌 것이다.
		SF_LOG(Warning, "[SFNexusBuilding] 스테이지 종료 요청이 거절됐습니다. 위 로그를 확인하세요.");
	}
	
	Destroy();
}
