#include "InGame/HUD/Tutorial/SFTutorialSubsystem.h"

#include "Engine/DataTable.h"
#include "Datas/Building/BuildingTierData/SFBuildingTierData.h"
#include "Datas/Building/SFBuildingDefinition.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/StaticMesh.h"
#include "Components/ActorComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Game/Subsystem/SFBuildingGridSubsystem.h"
#include "GameFramework/Actor.h"
#include "InGame/HUD/Tutorial/SFTutorialGhostActor.h"
#include "Buildings/Building/SFBaseBuilding.h"
#include "TimerManager.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Widgets/SViewport.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
// TSoftObjectPtr<USoundBase>::LoadSynchronous 가 완전한 타입을 요구한다.
// 전방 선언만으로는 C2027 이 난다.
#include "Sound/SoundBase.h"
#include "Logging/SFLog.h"
#include "Blueprint/UserWidget.h"
#include "Slate/SObjectWidget.h"
#include "Layout/WidgetPath.h"
#include "Containers/Ticker.h"
#include "Engine/HitResult.h"

UE_DEFINE_GAMEPLAY_TAG(TAG_SF_Tutorial_Command_Move, "Tutorial.Command.Move");
UE_DEFINE_GAMEPLAY_TAG(TAG_SF_Tutorial_Unit_Produced, "Tutorial.Unit.Produced");
UE_DEFINE_GAMEPLAY_TAG(TAG_SF_Tutorial_Unit_Produced_Tier1, "Tutorial.Unit.Produced.Tier1");
UE_DEFINE_GAMEPLAY_TAG(TAG_SF_Tutorial_Unit_Produced_Tier2, "Tutorial.Unit.Produced.Tier2");
UE_DEFINE_GAMEPLAY_TAG(TAG_SF_Tutorial_Unit_Produced_AfterBarrackUpgrade, "Tutorial.Unit.Produced.AfterBarrackUpgrade");
UE_DEFINE_GAMEPLAY_TAG(TAG_SF_Tutorial_Unit_Produced_SpecialWarBeast, "Tutorial.Unit.Produced.SpecialWarBeast");

void USFTutorialSubsystem::Deinitialize()
{
    // ClearGuideState 가 고스트·타이머·대기 상태를 모두 정리한다.
    // 정리 대상이 늘어나도 이쪽을 고칠 필요가 없도록 그대로 재사용한다.
    //
    // 건물 잠금과 기능 잠금은 ClearGuideState 가 건드리지 않는다.
    // 안내가 끝나는 것과 스테이지가 끝나는 것은 다르기 때문이다.
    UnlockBuildingSelection();
    LockedFeatures.Reset();
    StageObjectiveSuccessSound = nullptr;

    ClearGuideState();

    Super::Deinitialize();
}

USFTutorialSubsystem* USFTutorialSubsystem::Get(const UObject* WorldContext)
{
    if (WorldContext == nullptr)
    {
        return nullptr;
    }

    const UWorld* World = GEngine != nullptr
        ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull)
        : nullptr;

    return IsValid(World) ? World->GetSubsystem<USFTutorialSubsystem>() : nullptr;
}

bool USFTutorialSubsystem::BeginObjectiveWait(
    UDataTable* InDataTable,
    FName InRowName,
    int32 InResumeNodeIndex,
    FGameplayTag InObjectiveTag
)
{
    // 태그가 비어 있으면 아무리 기다려도 풀리지 않는다.
    // 조용히 대기에 들어가면 튜토리얼이 그 자리에서 영영 멈춘 것처럼 보인다.
    if (!InObjectiveTag.IsValid())
    {
        SF_LOG(Error, "[SFTutorialSubsystem] ObjectiveTag is not set. The wait would never end, so it is skipped.");
        return false;
    }

    // 튜토리얼이 시키기 전에 플레이어가 이미 해버린 경우.
    // 여기서 델리게이트를 쏘지 않는 이유는 헤더 주석에 적어뒀다.
    if (CompletedObjectives.HasTag(InObjectiveTag))
    {
        SF_LOG(Log, "[SFTutorialSubsystem] Objective was already completed | Tag: %s", *InObjectiveTag.ToString());
        return false;
    }

    if (PendingObjectiveTag.IsValid())
    {
        SF_LOG(Warning,
            "[SFTutorialSubsystem] A previous wait was still active and has been replaced | Old: %s | New: %s",
            *PendingObjectiveTag.ToString(), *InObjectiveTag.ToString());
    }

    PendingObjectiveTag = InObjectiveTag;
    PendingDataTable = InDataTable;
    PendingRowName = InRowName;
    PendingResumeNodeIndex = InResumeNodeIndex;

    SF_LOG(Log, "[SFTutorialSubsystem] Waiting for objective | Tag: %s | Resume node: %d",
        *InObjectiveTag.ToString(), InResumeNodeIndex);

    return true;
}

void USFTutorialSubsystem::CancelObjectiveWait()
{
    // 안내 중이었다면 그쪽 상태도 함께 접는다.
    // 대기만 풀고 안내를 남겨두면 스포트라이트가 화면에 그대로 붙어 있는다.
    if (GuideStepIndex != INDEX_NONE)
    {
        CancelGuide();
        return;
    }

    if (!PendingObjectiveTag.IsValid())
    {
        return;
    }

    SF_LOG(Log, "[SFTutorialSubsystem] Objective wait cancelled | Tag: %s", *PendingObjectiveTag.ToString());

    // 완료 처리를 할 수 있게 알린다. 재개 델리게이트는 쏘지 않는다 —
    // 취소는 "그만둔다" 이지 "다음으로 간다" 가 아니다.
    const FGameplayTag CancelledTag = PendingObjectiveTag;

    PendingObjectiveTag = FGameplayTag();
    PendingDataTable = nullptr;
    PendingRowName = NAME_None;
    PendingResumeNodeIndex = INDEX_NONE;

    OnObjectiveCompleted.Broadcast(CancelledTag);
}

bool USFTutorialSubsystem::NotifyObjective(const UObject* InWorldContext, FGameplayTag InObjectiveTag, bool bRecordWhenNotWaiting)
{
    USFTutorialSubsystem* Tutorial = Get(InWorldContext);

    if (!IsValid(Tutorial))
    {
        SF_LOG(Warning, "[SFTutorialSubsystem] Subsystem was not found. Check the world context object.");
        return false;
    }

    if (!InObjectiveTag.IsValid())
    {
        SF_LOG(Warning, "[SFTutorialSubsystem] NotifyObjective was called with an empty tag.");
        return false;
    }

    // 신호가 아예 안 들어오는 경우와, 들어왔는데 걸러진 경우를 구분해야 한다.
    // 둘 다 화면에서는 "아무 일도 안 일어남" 으로 똑같이 보이기 때문이다.
    //
    // 이동 명령처럼 한 번에 수십 번 들어오는 경로는 bRecordWhenNotWaiting 이
    // false 이므로 여기서 빠진다. 건물 완성 같은 상태형 목표만 찍힌다.
    if (bRecordWhenNotWaiting)
    {
        SF_LOG(Log, "[SFTutorialSubsystem] NotifyObjective | Tag: %s | From: %s",
            *InObjectiveTag.ToString(),
            InWorldContext != nullptr ? *InWorldContext->GetName() : TEXT("(null)"));
    }

    // 계층 매칭. 기다리는 쪽이 부모 태그면 자식 태그로도 만족한다.
    //   기다림 Tutorial.Build  +  신호 Tutorial.Build.Barracks  → 만족
    //   기다림 Tutorial.Build.Barracks  +  신호 Tutorial.Build  → 불만족
    const bool bMatchesPending =
        Tutorial->PendingObjectiveTag.IsValid() && InObjectiveTag.MatchesTag(Tutorial->PendingObjectiveTag);

    // 자리를 지정한 안내라면 거리부터 본다.
    //
    // 기록보다 먼저 막아야 한다. 여기서 통과시키면 엉뚱한 자리에서 지은
    // 건물이 CompletedObjectives 에 남아, 나중에 같은 태그를 쓰는 단계가
    // 통째로 건너뛰어진다.
    if (bMatchesPending && !Tutorial->IsGuideStepLocationSatisfied(InWorldContext))
    {
        return false;
    }

    // 행동형 목표인데 지금 그걸 요구하고 있지 않다면 조용히 버린다.
    //
    // 로그도 남기지 않는다. 이동 명령은 유닛 12기를 끌어 놓고 한 번 클릭하면
    // 12번 들어오므로, 여기서 한 줄씩 찍으면 다른 로그가 전부 밀려난다.
    if (!bMatchesPending && !bRecordWhenNotWaiting)
    {
        return false;
    }

    // 대기 중이 아니어도 기록은 남긴다.
    // 튜토리얼이 그 목표를 말하기 전에 이미 해버린 플레이어를 위해서다.
    Tutorial->CompletedObjectives.AddTag(InObjectiveTag);

    // 건물이 실제로 올라갔으면 요구 건물 잠금을 푼다.
    // 고르기만 하고 취소한 경우에는 여기까지 오지 않으므로 잠금이 유지된다.
    Tutorial->ReleaseBuildingLockIfBuilt(InObjectiveTag, InWorldContext);

    // 대기 여부와 무관하게 먼저 알린다.
    // 적 스포너처럼 튜토리얼 진행과 별개로 반응해야 하는 쪽이 이걸 듣는다.
    Tutorial->OnObjectiveRecorded.Broadcast(InObjectiveTag);

    if (!Tutorial->PendingObjectiveTag.IsValid())
    {
        SF_LOG(Log, "[SFTutorialSubsystem] Objective recorded while not waiting | Tag: %s",
            *InObjectiveTag.ToString());
        return false;
    }

    if (!bMatchesPending)
    {
        SF_LOG(Log, "[SFTutorialSubsystem] Objective did not match the pending one | Notified: %s | Pending: %s",
            *InObjectiveTag.ToString(), *Tutorial->PendingObjectiveTag.ToString());
        return false;
    }

    // 상태를 먼저 비우고 브로드캐스트한다.
    // 구독자가 그 자리에서 튜토리얼을 다시 열고 다음 대기 노드까지 진행할 수
    // 있는데, 그때 이 대기가 아직 남아 있으면 "이전 대기를 덮어썼다" 경고가 뜬다.
    UDataTable* ResumeTable = Tutorial->PendingDataTable;
    const FName ResumeRow = Tutorial->PendingRowName;
    const int32 ResumeIndex = Tutorial->PendingResumeNodeIndex;
    const FGameplayTag CompletedTag = Tutorial->PendingObjectiveTag;

    Tutorial->PendingObjectiveTag = FGameplayTag();
    Tutorial->PendingDataTable = nullptr;
    Tutorial->PendingRowName = NAME_None;
    Tutorial->PendingResumeNodeIndex = INDEX_NONE;

    Tutorial->OnObjectiveCompleted.Broadcast(CompletedTag);

    // 미션 하나를 해냈다는 신호. 다음 칸으로 넘어가기 전에 울린다 —
    // EnterGuideStep 이 CurrentStepSuccessSound 를 다음 칸 것으로 바꾸기
    // 때문에 순서를 바꾸면 엉뚱한 소리가 난다.
    Tutorial->PlayObjectiveSuccessSound(InWorldContext);

    // 안내 중이라면 튜토리얼로 돌아가지 않는다. 아직 남은 칸이 있을 수 있고,
    // 마지막 칸이었는지는 EnterGuideStep 이 판단한다.
    if (Tutorial->GuideStepIndex != INDEX_NONE)
    {
        SF_LOG(Log, "[SFTutorialSubsystem] Guide step %d completed | Tag: %s",
            Tutorial->GuideStepIndex, *CompletedTag.ToString());

        // 재개 정보를 되돌려 놓는다. EnterGuideStep 이 다음 칸의 대기를
        // 등록할 때 이 값들을 그대로 다시 써야 한다.
        Tutorial->PendingDataTable = ResumeTable;
        Tutorial->PendingRowName = ResumeRow;
        Tutorial->PendingResumeNodeIndex = ResumeIndex;

        Tutorial->EnterGuideStep(Tutorial->GuideStepIndex + 1);
        return true;
    }

    SF_LOG(Log, "[SFTutorialSubsystem] Objective completed | Tag: %s | Resuming at node %d",
        *CompletedTag.ToString(), ResumeIndex);

    Tutorial->OnResumeRequested.Broadcast(ResumeTable, ResumeRow, ResumeIndex);

    return true;
}

// ─────────────────────────────────────────────────────────
// 단계별 안내
//
// 목표 대기 위에 얹은 얇은 층이다. 칸 하나가 곧 목표 대기 하나이고,
// 대기가 풀릴 때 튜토리얼로 돌아가는 대신 다음 칸을 연다.
//
// 대기 상태(PendingObjectiveTag 등)를 그대로 재사용하는 것이 핵심이다.
// 안내 전용 대기를 따로 만들면 "지금 무엇을 기다리는가" 의 답이 두 개가
// 되어, 둘 중 하나만 풀리는 상태가 반드시 생긴다.
// ─────────────────────────────────────────────────────────
bool USFTutorialSubsystem::BeginGuide(
    UDataTable* InDataTable,
    FName InRowName,
    int32 InResumeNodeIndex,
    const TArray<FSFStageIntroGuideStep>& InSteps
)
{
    if (InSteps.IsEmpty())
    {
        SF_LOG(Warning, "[SFTutorialSubsystem] The guide has no steps, so it is skipped.");
        return false;
    }

    if (GuideStepIndex != INDEX_NONE)
    {
        SF_LOG(Warning, "[SFTutorialSubsystem] A previous guide was still active and has been replaced.");
        ClearGuideState();
    }

    GuideSteps = InSteps;
    GuideStepIndex = INDEX_NONE;

    // 잠긴 건물 이름은 남겨두되, "어느 칸이 선택을 기다리는가" 는 다시 잡는다.
    // 같은 안내를 다시 열었을 때 첫 칸이 선택 칸으로 인식되어야 한다.
    LockedBuildingStepIndex = INDEX_NONE;

    PendingDataTable = InDataTable;
    PendingRowName = InRowName;
    PendingResumeNodeIndex = InResumeNodeIndex;

    SF_LOG(Log, "[SFTutorialSubsystem] Guide started | Steps: %d | Resume node: %d",
        GuideSteps.Num(), InResumeNodeIndex);

    bEnteringGuide = true;
    bGuideEndedWhileEntering = false;

    EnterGuideStep(0);

    bEnteringGuide = false;

    // 이미 전부 해버린 안내였다. 호출부가 다음 노드로 넘어가면 된다.
    if (bGuideEndedWhileEntering)
    {
        bGuideEndedWhileEntering = false;

        SF_LOG(Log, "[SFTutorialSubsystem] Every guide step was already satisfied, so the guide is skipped.");

        return false;
    }

    return true;
}

void USFTutorialSubsystem::EnterGuideStep(int32 InStepIndex)
{
    // 칸이 바뀔 때마다 정리한다. 다음 칸이 고스트를 안 쓸 수도 있고,
    // 이전 칸의 자동 넘김 타이머가 살아 있으면 두 칸이 겹쳐 진행된다.
    DestroyPlacementGhost();

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(GuideAutoAdvanceHandle);
    }

    if (!GuideSteps.IsValidIndex(InStepIndex))
    {
        FinishGuide();
        return;
    }

    GuideStepIndex = InStepIndex;
    FSFStageIntroGuideStep& MutableStep = GuideSteps[GuideStepIndex];
    SnapPlacementGuideToBuildingGrid(MutableStep);
    const FSFStageIntroGuideStep& Step = MutableStep;

    // 요구 건물은 칸을 넘어서, 안내가 끝난 뒤에도 유지된다.
    //
    // 새 값이 들어올 때만 덮어쓴다. 비어 있는 칸은 앞 칸의 요구를 그대로
    // 물려받는다 — 칸마다 같은 값을 다시 적게 하면 반드시 한 칸을 빠뜨리고,
    // 그 칸에서 모든 건물이 열린다.
    if (!Step.RequiredBuildingRowName.IsNone())
    {
        if (Step.RequiredBuildingRowName != LockedBuildingRowName)
        {
            SF_LOG(Log, "[SFTutorialSubsystem] 요구 건물 잠금 | %s -> %s",
                LockedBuildingRowName.IsNone() ? TEXT("(none)") : *LockedBuildingRowName.ToString(),
                *Step.RequiredBuildingRowName.ToString());

            LockedBuildingRowName = Step.RequiredBuildingRowName;
            LockedBuildingStepIndex = INDEX_NONE;
        }

        // 건물 선택으로 넘어가는 칸은 그 요구를 처음 선언한 칸 하나뿐이다.
        //
        // 뒤따르는 배치 칸이 같은 값을 다시 적어둔 DT 가 있는데, 거기서도
        // 선택을 완료로 처리하면 건물을 다시 고르는 것만으로 배치가 끝난
        // 것으로 처리된다.
        if (LockedBuildingStepIndex == INDEX_NONE)
        {
            LockedBuildingStepIndex = GuideStepIndex;
        }
    }

    // 이 칸의 성공음을 미리 로드한다.
    //
    // 달성하는 순간에 로드하면 그 프레임이 튄다. 하필 플레이어가 화면을
    // 보고 있는 순간이라 눈에 띈다.
    CurrentStepSuccessSound = Step.CompletionSound.IsNull()
        ? nullptr
        : Step.CompletionSound.LoadSynchronous();

    // 신호를 기다리는 칸만 대기 태그를 건다.
    // Click / Timer 칸은 스포트라이트나 타이머가 직접 넘기므로 대기가 없다.
    PendingObjectiveTag = FGameplayTag();
    if (Step.Completion == ESFGuideCompletion::ObjectiveTag)
    {
        if (Step.CompletionTag.IsValid())
        {
            // 이미 해버린 조작이면 기다릴 이유가 없다. 곧바로 다음 칸으로.
            if (CompletedObjectives.HasTag(Step.CompletionTag))
            {
                SF_LOG(Log, "[SFTutorialSubsystem] Guide step %d was already satisfied | Tag: %s",
                    GuideStepIndex, *Step.CompletionTag.ToString());

                EnterGuideStep(GuideStepIndex + 1);
                return;
            }

            PendingObjectiveTag = Step.CompletionTag;
        }
        else
        {
            // 태그를 비워두면 영영 풀리지 않는다. 조용히 멈추는 대신
            // 클릭으로 넘어갈 수 있게 두고 경고를 남긴다.
            SF_LOG(Error,
                "[SFTutorialSubsystem] Guide step %d waits for an empty tag. "
                "It falls back to click-to-advance.", GuideStepIndex);
        }
    }

    if (Step.bShowPlacementGhost && Step.TargetKind == ESFGuideTargetKind::WorldLocation)
    {
        SpawnPlacementGhost(Step);
    }

    // 자동 넘김. 신호를 기다리는 칸에서도 배선이 빠졌을 때를 대비해 걸어둔다.
    const float AutoSeconds = Step.Completion == ESFGuideCompletion::Timer
        ? FMath::Max(Step.AutoAdvanceSeconds, 0.1f)
        : Step.AutoAdvanceSeconds;

    if (AutoSeconds > 0.f)
    {
        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().SetTimer(
                GuideAutoAdvanceHandle, this, &USFTutorialSubsystem::HandleGuideAutoAdvance, AutoSeconds, false);
        }
    }

    SF_LOG(Log, "[SFTutorialSubsystem] Guide step %d/%d | Tag: %s | Text: %s",
        GuideStepIndex + 1, GuideSteps.Num(),
        PendingObjectiveTag.IsValid() ? *PendingObjectiveTag.ToString() : TEXT("(none)"),
        *Step.InstructionText.ToString());

    // 자리 제한이 실제로 걸렸는지 값으로 남긴다.
    //
    // "제한이 안 먹는다" 의 원인은 거의 언제나 DT 체크박스가 꺼져 있는
    // 것인데, 화면만 봐서는 코드가 안 도는 것과 구분이 안 된다.
    SF_LOG(Log, "[SFTutorialSubsystem]   Restrict to spot: %s | Kind: %s | Center: %s | Radius: %.0f",
        Step.bRequireNearTarget ? TEXT("YES") : TEXT("no"),
        Step.TargetKind == ESFGuideTargetKind::WorldLocation ? TEXT("World") : TEXT("Screen"),
        *Step.WorldLocation.ToString(),
        Step.WorldRadius);

    OnGuideStepStarted.Broadcast(Step, GuideStepIndex, GuideSteps.Num());
}

void USFTutorialSubsystem::CompleteCurrentGuideStep()
{
    if (GuideStepIndex == INDEX_NONE)
    {
        return;
    }

    EnterGuideStep(GuideStepIndex + 1);
}

void USFTutorialSubsystem::HandleGuideAutoAdvance()
{
    if (GuideStepIndex == INDEX_NONE)
    {
        return;
    }

    SF_LOG(Log, "[SFTutorialSubsystem] Guide step %d advanced by timer.", GuideStepIndex);

    EnterGuideStep(GuideStepIndex + 1);
}

void USFTutorialSubsystem::FinishGuide()
{
    // 돌아갈 자리를 먼저 빼둔다. ClearGuideState 가 대기 상태까지 비운다.
    UDataTable* ResumeTable = PendingDataTable;
    const FName ResumeRow = PendingRowName;
    const int32 ResumeIndex = PendingResumeNodeIndex;

    ClearGuideState();

    // BeginGuide 가 아직 실행 중이다. 여기서 재개를 쏘면 위젯이 둘이 된다.
    if (bEnteringGuide)
    {
        bGuideEndedWhileEntering = true;
        return;
    }

    SF_LOG(Log, "[SFTutorialSubsystem] Guide finished | Resuming at node %d", ResumeIndex);

    OnGuideFinished.Broadcast();
    OnResumeRequested.Broadcast(ResumeTable, ResumeRow, ResumeIndex);
}

void USFTutorialSubsystem::CancelGuide()
{
    if (GuideStepIndex == INDEX_NONE)
    {
        return;
    }

    SF_LOG(Log, "[SFTutorialSubsystem] Guide cancelled at step %d", GuideStepIndex);

    // 안내를 강제로 접었으면 건설 제한도 같이 푼다.
    // 안 그러면 안내가 없는데 건설 메뉴만 잠긴 상태로 남는다.
    UnlockBuildingSelection();

    ClearGuideState();

    OnGuideFinished.Broadcast();
}

void USFTutorialSubsystem::ClearGuideState()
{
    // 경고를 켜둔 채로 안내가 끝나면 문구가 화면에 남는다.
    SetBuildBlocked(false);

    DestroyPlacementGhost();

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(GuideAutoAdvanceHandle);
    }

    GuideSteps.Empty();
    GuideStepIndex = INDEX_NONE;

    // 칸에 딸린 소리다. 스테이지 기본음(StageObjectiveSuccessSound)은 건드리지
    // 않는다 — 안내가 끝나도 남은 목표들이 계속 그 소리를 써야 한다.
    CurrentStepSuccessSound = nullptr;

    PendingObjectiveTag = FGameplayTag();
    PendingDataTable = nullptr;
    PendingRowName = NAME_None;
    PendingResumeNodeIndex = INDEX_NONE;
}

bool USFTutorialSubsystem::IsGuideActive() const
{
    return GuideStepIndex != INDEX_NONE;
}

int32 USFTutorialSubsystem::GetCurrentGuideStepIndex() const
{
    return GuideStepIndex;
}

bool USFTutorialSubsystem::IsBuildLocationAllowed(const UObject* InWorldContext, const FVector& InLocation)
{
    USFTutorialSubsystem* Tutorial = Get(InWorldContext);

    // 튜토리얼이 없는 판이면 아무것도 제한하지 않는다.
    if (!IsValid(Tutorial) || !Tutorial->GuideSteps.IsValidIndex(Tutorial->GuideStepIndex))
    {
        return true;
    }

    const FSFStageIntroGuideStep& Step = Tutorial->GuideSteps[Tutorial->GuideStepIndex];

    if (!Step.bRequireNearTarget || Step.TargetKind != ESFGuideTargetKind::WorldLocation)
    {
        return true;
    }

    // 수평 거리로만 잰다. 판정 반경이 화면에 그려지는 반경과 같아야
    // 플레이어가 보고 있는 동그라미가 곧 지을 수 있는 자리가 된다.
    const float DistanceSquared = FVector::DistSquaredXY(InLocation, Step.WorldLocation);
    const float RadiusSquared = FMath::Square(FMath::Max(Step.WorldRadius, 1.f));

    const bool bAllowed = DistanceSquared <= RadiusSquared;

    // 배치 미리보기는 매 틱 부르므로 결과가 바뀔 때만 알린다.
    // 매 틱 알리면 구독하는 쪽이 같은 문구를 초당 60번 다시 세운다.
    Tutorial->SetBuildBlocked(!bAllowed);

    return bAllowed;
}

// ─────────────────────────────────────────────────────────
// 건설 메뉴
//
// 항목 하나를 고르는 것도 튜토리얼 단계가 될 수 있어야 한다.
// "건설 버튼을 누르세요" 만 있고 무엇을 고르든 넘어가면, 플레이어가
// 엉뚱한 건물을 들고 배치 단계로 들어간다.
// ─────────────────────────────────────────────────────────
const FSFStageIntroGuideStep* USFTutorialSubsystem::GetCurrentGuideStep() const
{
    return GuideSteps.IsValidIndex(GuideStepIndex) ? &GuideSteps[GuideStepIndex] : nullptr;
}

void USFTutorialSubsystem::UnlockBuildingSelection()
{
    if (LockedBuildingRowName.IsNone())
    {
        return;
    }

    SF_LOG(Log, "[SFTutorialSubsystem] 요구 건물 잠금 해제 | Row: %s",
        *LockedBuildingRowName.ToString());

    LockedBuildingRowName = NAME_None;
    LockedBuildingStepIndex = INDEX_NONE;
    ReservedTutorialBuilding = nullptr;
}

void USFTutorialSubsystem::ReleaseBuildingLockIfBuilt(
    FGameplayTag InObjectiveTag,
    const UObject* InWorldContext)
{
    if (LockedBuildingRowName.IsNone())
    {
        return;
    }

    // "건물이 지어졌다" 는 신호만 잠금을 푼다.
    //
    // 선택(Tutorial.Select.Building.*)으로는 풀리지 않는다. 고르기만 하고
    // 배치를 취소한 상태에서 풀어주면, 플레이어가 건설 메뉴로 돌아왔을 때
    // 다시 모든 건물이 열린다 — 고치려던 그 버그가 그대로 재현된다.
    //
    // 건물의 완료 태그를 Tutorial.Build 아래에 두지 않았다면 잠금이 풀리지
    // 않는다. 그때는 sf.Tutorial.DumpState 의 Completed 목록에서 실제 이름을
    // 확인하고 아래 루트 태그를 그 부모로 바꾸면 된다.
    static const FGameplayTag BuildRoot =
        FGameplayTag::RequestGameplayTag(TEXT("Tutorial.Build"), false);

    if (!BuildRoot.IsValid())
    {
        SF_LOG(Warning,
            "[SFTutorialSubsystem] 'Tutorial.Build' 태그가 없어 건물 잠금을 풀 수 없다. "
            "Project Settings > GameplayTags 에 부모 태그를 추가하라.");
        return;
    }

    if (!InObjectiveTag.MatchesTag(BuildRoot))
    {
        return;
    }

    // 고스트 위치에 실제로 확정한 건물이 있는 동안에는, 그 건물이 보낸
    // 완료 신호만 잠금을 풀 수 있다. 이미 짓고 있던 다른 건물의 완료가
    // 끼어들어 튜토리얼 건설 제한을 풀어 버리는 일을 막는다.
    if (ReservedTutorialBuilding.IsValid())
    {
        const AActor* NotifierActor = Cast<AActor>(InWorldContext);
        if (const UActorComponent* NotifierComponent = Cast<UActorComponent>(InWorldContext))
        {
            NotifierActor = NotifierComponent->GetOwner();
        }

        if (NotifierActor != ReservedTutorialBuilding.Get())
        {
            SF_LOG(Log,
                "[SFTutorialSubsystem] 다른 건물 완료 신호는 튜토리얼 잠금을 유지 | Signal: %s | Reserved: %s",
                *InObjectiveTag.ToString(),
                *GetNameSafe(ReservedTutorialBuilding.Get()));
            return;
        }
    }

    SF_LOG(Log, "[SFTutorialSubsystem] 건설 완료로 잠금 해제 | 신호: %s", *InObjectiveTag.ToString());

    UnlockBuildingSelection();
}

// ─────────────────────────────────────────────────────────
// 스테이지 단위 기능 잠금
//
// "1스테이지에서는 업그레이드 금지" 처럼 안내 칸 하나가 아니라 스테이지
// 전체에 걸리는 규칙이다. 잠그는 쪽(DT)과 잠기는 쪽(UI)을 태그로만 이어
// 두어, 새 기능을 잠글 때 코드를 고치지 않아도 되게 한다.
// ─────────────────────────────────────────────────────────
void USFTutorialSubsystem::SetStageFeatureLocks(
    const UObject* InWorldContext, const FGameplayTagContainer& InLocks)
{
    USFTutorialSubsystem* Tutorial = Get(InWorldContext);

    if (!IsValid(Tutorial))
    {
        return;
    }

    // 더하지 않고 갈아끼운다. 스테이지가 바뀌면 앞 스테이지의 잠금은
    // 남아 있으면 안 된다.
    Tutorial->LockedFeatures = InLocks;

    SF_LOG(Log, "[SFTutorialSubsystem] 스테이지 기능 잠금 %d개 | %s",
        InLocks.Num(),
        InLocks.IsEmpty() ? TEXT("(none)") : *InLocks.ToStringSimple());
}

bool USFTutorialSubsystem::IsFeatureLocked(
    const UObject* InWorldContext, FGameplayTag InFeatureTag)
{
    if (!InFeatureTag.IsValid())
    {
        return false;
    }

    const USFTutorialSubsystem* Tutorial = Get(InWorldContext);

    // 튜토리얼이 없는 판에서는 아무것도 잠기지 않는다.
    // 부르는 쪽이 튜토리얼의 유무를 따지지 않아도 되게 한다.
    return IsValid(Tutorial) && Tutorial->LockedFeatures.HasTag(InFeatureTag);
}

bool USFTutorialSubsystem::IsTierUpgradeLocked(const UObject* InWorldContext)
{
    static const FGameplayTag UpgradeTag =
        FGameplayTag::RequestGameplayTag(TEXT("Tutorial.Lock.NexusUpgrade"), false);

    // 태그가 없으면 빈 태그가 오고, IsFeatureLocked 가 false 를 돌려준다.
    // 태그를 아직 등록하지 않은 브랜치에서도 기존 동작이 그대로다.
    return IsFeatureLocked(InWorldContext, UpgradeTag);
}

bool USFTutorialSubsystem::IsBuildingTierUpgradeAllowed(
    const UObject* InWorldContext,
    const ASFBaseBuilding* InBuilding)
{
    if (!IsValid(InBuilding))
    {
        return false;
    }

    static const FGameplayTag TierUpgradeLockTag =
        FGameplayTag::RequestGameplayTag(TEXT("Tutorial.Lock.TierUpgrade"), false);
    static const FGameplayTag NexusUpgradeTag =
        FGameplayTag::RequestGameplayTag(TEXT("Tutorial.TierUp.Nexus"), false);
    static const FGameplayTag BarrackUpgradeTag =
        FGameplayTag::RequestGameplayTag(TEXT("Tutorial.TierUp.Barrack"), false);

    // 이 잠금이 없는 일반 스테이지의 기존 동작은 절대 바꾸지 않는다.
    if (!IsFeatureLocked(InWorldContext, TierUpgradeLockTag))
    {
        return true;
    }

    const USFTutorialSubsystem* Tutorial = Get(InWorldContext);
    const USFBuildingDefinition* Definition = InBuilding->GetBuildingDefinition();

    if (!IsValid(Tutorial) || !IsValid(Definition))
    {
        return false;
    }

    if (Tutorial->PendingObjectiveTag == NexusUpgradeTag)
    {
        return Definition->BuildingType == ESFBuildingType::Nexus;
    }

    if (Tutorial->PendingObjectiveTag == BarrackUpgradeTag)
    {
        return Definition->BuildingType == ESFBuildingType::Barracks;
    }

    return false;
}

bool USFTutorialSubsystem::IsUnitProductionAllowed(
    const UObject* InWorldContext,
    const ASFBaseBuilding* InBuilding,
    FName InUnitRowName)
{
    if (!IsValid(InBuilding))
    {
        return false;
    }

    static const FGameplayTag UnitProductionLockTag =
        FGameplayTag::RequestGameplayTag(TEXT("Tutorial.Lock.UnitProduction"), false);

    if (!IsFeatureLocked(InWorldContext, UnitProductionLockTag))
    {
        return true;
    }

    const USFTutorialSubsystem* Tutorial = Get(InWorldContext);
    const USFBuildingDefinition* Definition = InBuilding->GetBuildingDefinition();

    if (!IsValid(Tutorial) || !IsValid(Definition))
    {
        return false;
    }

    const bool bStage2Berserker =
        Tutorial->PendingObjectiveTag == TAG_SF_Tutorial_Unit_Produced_AfterBarrackUpgrade &&
        Definition->BuildingType == ESFBuildingType::Barracks &&
        InBuilding->GetCurrentTier() >= 1 &&
        InUnitRowName == TEXT("Berserker");

    const bool bStage3WarBeast =
        Tutorial->PendingObjectiveTag == TAG_SF_Tutorial_Unit_Produced_SpecialWarBeast &&
        Definition->BuildingType == ESFBuildingType::SpecialProduction &&
        InUnitRowName == TEXT("WarBeast");

    return bStage2Berserker || bStage3WarBeast;
}

void USFTutorialSubsystem::NotifyBuildingTierActivated(
    const UObject* InWorldContext,
    const ASFBaseBuilding* InBuilding,
    int32 InActivatedTier)
{
    // 시작 앵커의 Tier 0 활성화도 이 경로를 탄다. 행동형 목표(false)로
    // 보내면 해당 목표를 실제로 기다릴 때만 반응하므로, 시작 직후 미리
    // 달성 처리되는 일이 없다.
    if (!IsValid(InBuilding) || InActivatedTier < 1)
    {
        return;
    }

    const USFBuildingDefinition* Definition = InBuilding->GetBuildingDefinition();
    if (!IsValid(Definition))
    {
        return;
    }

    FGameplayTag ObjectiveTag;
    switch (Definition->BuildingType)
    {
    case ESFBuildingType::Nexus:
        ObjectiveTag = FGameplayTag::RequestGameplayTag(TEXT("Tutorial.TierUp.Nexus"), false);
        break;
    case ESFBuildingType::Barracks:
        ObjectiveTag = FGameplayTag::RequestGameplayTag(TEXT("Tutorial.TierUp.Barrack"), false);
        break;
    default:
        return;
    }

    if (ObjectiveTag.IsValid())
    {
        NotifyObjective(InWorldContext, ObjectiveTag, false);
    }
}

void USFTutorialSubsystem::NotifyBuildingConstructionCompleted(
    const UObject* InWorldContext,
    const ASFBaseBuilding* InBuilding,
    int32 InActivatedTier)
{
    // Tier 1 이상은 기존 건물의 업그레이드다. 건설 목표로 다시 기록하면
    // 다른 안내 단계가 의도치 않게 미리 완료될 수 있으므로 최초 건설만 보낸다.
    if (!IsValid(InBuilding) || InActivatedTier != 0)
    {
        return;
    }

    const USFBuildingDefinition* Definition = InBuilding->GetBuildingDefinition();
    if (!IsValid(Definition))
    {
        return;
    }

    FGameplayTag ObjectiveTag;
    switch (Definition->BuildingType)
    {
    case ESFBuildingType::EtherMine:
        ObjectiveTag = FGameplayTag::RequestGameplayTag(TEXT("Tutorial.Build.EtherMine"), false);
        break;
    case ESFBuildingType::House:
        ObjectiveTag = FGameplayTag::RequestGameplayTag(TEXT("Tutorial.Build.House"), false);
        break;
    case ESFBuildingType::MineralMine:
        ObjectiveTag = FGameplayTag::RequestGameplayTag(TEXT("Tutorial.Build.Mining"), false);
        break;
    case ESFBuildingType::Barracks:
        ObjectiveTag = FGameplayTag::RequestGameplayTag(TEXT("Tutorial.Build.Product"), false);
        break;
    case ESFBuildingType::RangedProduction:
        ObjectiveTag = FGameplayTag::RequestGameplayTag(TEXT("Tutorial.Build.Academy"), false);
        break;
    case ESFBuildingType::SpecialProduction:
        ObjectiveTag = FGameplayTag::RequestGameplayTag(TEXT("Tutorial.Build.Special"), false);
        break;
    default:
        return;
    }

    if (ObjectiveTag.IsValid())
    {
        // 건설 목표는 "지금 안내한 건물을 실제로 완성했는가"를 묻는
        // 행동형 목표다. 맵 로딩 중에 이미 완성된 시작/상대 건물까지
        // CompletedObjectives 에 기록해 두면, 나중에 건설 메뉴에서 대상만
        // 고른 뒤 다음 배치 칸이 자동으로 넘어가 버린다.
        //
        // 따라서 현재 이 목표를 기다릴 때만 통과시킨다. 실제 건설 완료는
        // PendingObjectiveTag 와 일치하므로 아래 false여도 정상 진행된다.
        NotifyObjective(InWorldContext, ObjectiveTag, false);
    }
}

// ─────────────────────────────────────────────────────────
// 성공 효과음
//
// 목표가 풀리는 순간은 NotifyObjective 한 곳뿐이다. 건설이든 생산이든
// 이동이든 전부 그리로 들어오므로, 소리도 거기서 한 번만 울리면 된다.
// 신호를 보내는 쪽(건물 BP, 유닛)은 소리를 몰라도 된다.
// ─────────────────────────────────────────────────────────
void USFTutorialSubsystem::SetStageObjectiveSuccessSound(
    const UObject* InWorldContext, const TSoftObjectPtr<USoundBase>& InSound)
{
    USFTutorialSubsystem* Tutorial = Get(InWorldContext);

    if (!IsValid(Tutorial))
    {
        return;
    }

    Tutorial->StageObjectiveSuccessSound = InSound.IsNull() ? nullptr : InSound.LoadSynchronous();

    SF_LOG(Log, "[SFTutorialSubsystem] 스테이지 성공음 | %s",
        IsValid(Tutorial->StageObjectiveSuccessSound)
        ? *Tutorial->StageObjectiveSuccessSound->GetName()
        : TEXT("(none)"));
}

void USFTutorialSubsystem::PlayObjectiveSuccessSound(const UObject* InWorldContext) const
{
    // 칸이 자기 소리를 지정했으면 그것이 이긴다.
    USoundBase* Sound = IsValid(CurrentStepSuccessSound.Get())
        ? CurrentStepSuccessSound.Get()
        : StageObjectiveSuccessSound.Get();

    if (!IsValid(Sound))
    {
        return;
    }

    // bIsUISound = true.
    //
    // 목표가 풀리는 순간 튜토리얼 창이 곧바로 다시 열리면서 게임이
    // 일시정지된다. UI 사운드가 아니면 소리가 그 자리에서 잘린다.
    UGameplayStatics::PlaySound2D(
        InWorldContext,
        Sound,
        1.f,        // VolumeMultiplier
        1.f,        // PitchMultiplier
        0.f,        // StartTime
        nullptr,    // ConcurrencySettings
        nullptr,    // OwningActor
        true);      // bIsUISound

    SF_LOG(Log, "[SFTutorialSubsystem] 성공음 재생 | %s", *Sound->GetName());
}

FGameplayTag USFTutorialSubsystem::MakeBuildingSelectTag(FName InBuildingRowName)
{
    if (InBuildingRowName.IsNone())
    {
        return FGameplayTag();
    }

    const FString TagString =
        FString::Printf(TEXT("Tutorial.Select.Building.%s"), *InBuildingRowName.ToString());

    // 두 번째 인자가 false 라 없는 태그면 경고 없이 빈 태그가 돌아온다.
    return FGameplayTag::RequestGameplayTag(FName(*TagString), false);
}

void USFTutorialSubsystem::NotifyBuildingSelected(const UObject* InWorldContext, FName InBuildingRowName)
{
    USFTutorialSubsystem* Tutorial = Get(InWorldContext);

    if (!IsValid(Tutorial) || InBuildingRowName.IsNone())
    {
        return;
    }

    // 1순위 — 요구 건물이 잠겨 있는 경우.
    //
    // 이쪽이면 태그 이름과 행 이름이 달라도 된다. 태그는 DT 작성자가
    // Tutorial.Select.Building.Product 처럼 의미대로 지으면 되고, 어떤 건물을
    // 요구하는지는 잠금이 따로 들고 있다.
    if (!Tutorial->LockedBuildingRowName.IsNone())
    {
        // 완료 신호는 "잠금을 건 그 칸" 에 있을 때만 보낸다.
        //
        // 잠금은 칸을 넘어서, 안내가 끝난 뒤에도 유지된다. 잠금만 보고
        // 신호를 보내면 배치 칸에서 건물을 다시 골랐을 때 그 칸의 완료
        // 태그(Tutorial.Build.*)가 나가 배치가 끝난 것으로 처리된다.
        const bool bOnLockingStep =
            Tutorial->LockedBuildingStepIndex != INDEX_NONE &&
            Tutorial->LockedBuildingStepIndex == Tutorial->GuideStepIndex;

        // 건설 메뉴에서의 "선택"만 이 경로로 완료될 수 있다.
        //
        // 스테이지 1처럼 하나의 안내 칸이 곧바로 Tutorial.Build.* 를
        // 기다리는 경우도 RequiredBuildingRowName 을 사용해 다른 건물을
        // 막는다. 이때 CompletionTag 를 그대로 쏘면 카탈로그에서 건물을
        // 고르는 것만으로 "건설 완료"가 되어 단계가 건너뛰어진다.
        // 선택 목표(Tutorial.Select.Building.*)일 때만 클릭 신호를
        // 완료로 인정하고, 건설/생산/티어업 태그는 실제 게임플레이 이벤트가
        // 들어올 때까지 기다린다.
        static const FGameplayTag SelectRoot =
            FGameplayTag::RequestGameplayTag(TEXT("Tutorial.Select.Building"), false);

        if (bOnLockingStep && Tutorial->LockedBuildingRowName == InBuildingRowName)
        {
            if (const FSFStageIntroGuideStep* Step = Tutorial->GetCurrentGuideStep())
            {
                if (SelectRoot.IsValid() &&
                    Step->CompletionTag.IsValid() &&
                    Step->CompletionTag.MatchesTag(SelectRoot))
                {
                    NotifyObjective(InWorldContext, Step->CompletionTag, false);
                }
            }
        }

        // 요구 건물이 정해져 있는 동안에는 이름 규칙 태그로 새지 않는다.
        //
        // 안내가 끝났는데 잠금만 남은 상태(선택 칸이 마지막 칸이었고 배치를
        // 취소한 경우)에서 Tutorial.Select.Building.<행> 을 쏘면, 뒤에 오는
        // 선택 단계가 미리 달성된 것으로 기록된다.
        return;
    }

    // 2순위 — 이름 규칙으로 조립한 태그.
    // 안내 칸을 쓰지 않는 단순 대기(WaitForObjective)에서도 쓸 수 있게 남겨둔다.
    const FGameplayTag SelectTag = MakeBuildingSelectTag(InBuildingRowName);

    if (SelectTag.IsValid())
    {
        NotifyObjective(InWorldContext, SelectTag, false);
    }
}

bool USFTutorialSubsystem::IsBuildingSelectionAllowed(const UObject* InWorldContext, FName InBuildingRowName)
{
    // 어느 갈래에서 통과됐는지 남긴다.
    //
    // "전부 눌린다" 는 증상은 원인이 여럿인데(안내가 안 돌고 있다 / 요구 건물을
    // 안 적었다 / 부모 태그가 없다) 결과만 보면 구분이 되지 않는다.
    // 클릭할 때만 불리므로 로그가 넘치지 않는다.
    const USFTutorialSubsystem* Tutorial = Get(InWorldContext);

    if (!IsValid(Tutorial))
    {
        SF_LOG(Log, "[SFTutorialSubsystem] Selection allowed: 서브시스템이 없다 | Row: %s",
            *InBuildingRowName.ToString());
        return true;
    }

    // 고스트 자리에 건설을 확정한 뒤에는 건설 중인 그 건물을 포함해 전부 막는다.
    // 같은 건물을 한 번 더 지어 광물이 부족해지는 문제를 여기서 끊는다.
    if (Tutorial->ReservedTutorialBuilding.IsValid())
    {
        SF_LOG(Log,
            "[SFTutorialSubsystem] Selection BLOCKED | Tutorial construction in progress: %s | Clicked: %s",
            *GetNameSafe(Tutorial->ReservedTutorialBuilding.Get()), *InBuildingRowName.ToString());
        return false;
    }

    // 1순위 — 요구 건물이 잠겨 있으면 그것만 통과시킨다.
    //
    // 안내 칸이 아니라 잠금을 본다. 선택 칸이 그 노드의 마지막 칸이면
    // 건물을 고르는 즉시 안내가 끝나는데, 그 뒤에 배치를 취소하면
    // 플레이어는 건설 메뉴 앞으로 돌아온다. 그때 칸에게 물어보면 이미
    // 요구 건물을 잃어버린 뒤라 모든 건물이 열린다.
    if (!Tutorial->LockedBuildingRowName.IsNone())
    {
        const bool bMatches = (Tutorial->LockedBuildingRowName == InBuildingRowName);

        SF_LOG(Log, "[SFTutorialSubsystem] Selection %s | Required: %s | Clicked: %s",
            bMatches ? TEXT("ALLOWED") : TEXT("BLOCKED"),
            *Tutorial->LockedBuildingRowName.ToString(),
            *InBuildingRowName.ToString());

        return bMatches;
    }

    // Stage2처럼 "이번 임무 외 행동 금지"가 필요한 경우에는 건설 임무가
    // 끝난 뒤에도 새 건설을 열지 않는다. 에테르 광산 선택/건설 단계에서는
    // 위의 요구 건물 잠금이 먼저 처리하므로 그 건물만 정상적으로 허용된다.
    static const FGameplayTag BuildingConstructionLockTag =
        FGameplayTag::RequestGameplayTag(TEXT("Tutorial.Lock.BuildingConstruction"), false);
    if (IsFeatureLocked(InWorldContext, BuildingConstructionLockTag))
    {
        SF_LOG(Log,
            "[SFTutorialSubsystem] Selection BLOCKED | Current tutorial mission does not allow new construction: %s",
            *InBuildingRowName.ToString());
        return false;
    }

    SF_LOG(Log, "[SFTutorialSubsystem] 요구 건물 잠금이 없다 | GuideStepIndex: %d",
        Tutorial->GuideStepIndex);

    // 2순위 — 이름 규칙 태그를 기다리는 중인지 본다.
    static const FGameplayTag SelectRoot =
        FGameplayTag::RequestGameplayTag(TEXT("Tutorial.Select.Building"), false);

    if (!SelectRoot.IsValid())
    {
        SF_LOG(Warning,
            "[SFTutorialSubsystem] 'Tutorial.Select.Building' 태그가 없어 건물 선택을 제한하지 못한다. "
            "Project Settings > GameplayTags 에 부모 태그를 추가하라.");
        return true;
    }

    if (!Tutorial->PendingObjectiveTag.IsValid() ||
        !Tutorial->PendingObjectiveTag.MatchesTag(SelectRoot))
    {
        SF_LOG(Log, "[SFTutorialSubsystem] 지금 기다리는 것이 건물 선택이 아니다 | Pending: %s",
            Tutorial->PendingObjectiveTag.IsValid()
            ? *Tutorial->PendingObjectiveTag.ToString()
            : TEXT("(none)"));
        return true;
    }

    const FGameplayTag SelectTag = MakeBuildingSelectTag(InBuildingRowName);
    const bool bAllowed = SelectTag.IsValid() && SelectTag.MatchesTag(Tutorial->PendingObjectiveTag);

    SF_LOG(Log, "[SFTutorialSubsystem] Selection %s (이름 규칙) | Pending: %s | Clicked: %s",
        bAllowed ? TEXT("ALLOWED") : TEXT("BLOCKED"),
        *Tutorial->PendingObjectiveTag.ToString(),
        *InBuildingRowName.ToString());

    return bAllowed;
}

void USFTutorialSubsystem::SetBuildBlocked(bool bInBlocked)
{
    if (bLastBuildBlocked == bInBlocked)
    {
        return;
    }

    bLastBuildBlocked = bInBlocked;

    SF_LOG(Log, "[SFTutorialSubsystem] Build location %s",
        bInBlocked ? TEXT("BLOCKED") : TEXT("ALLOWED"));

    OnBuildBlockedChanged.Broadcast(bInBlocked);
}

void USFTutorialSubsystem::HidePlacementGhost(const UObject* InWorldContext)
{
    if (USFTutorialSubsystem* Tutorial = Get(InWorldContext))
    {
        Tutorial->DestroyPlacementGhost();
    }
}

void USFTutorialSubsystem::NotifyTutorialBuildingPlacementConfirmed(
    const UObject* InWorldContext,
    ASFBaseBuilding* InBuilding)
{
    USFTutorialSubsystem* Tutorial = Get(InWorldContext);

    if (!IsValid(Tutorial) || !IsValid(InBuilding) || Tutorial->LockedBuildingRowName.IsNone())
    {
        return;
    }

    const USFBuildingDefinition* Definition = InBuilding->GetBuildingDefinition();
    if (!IsValid(Definition) || Definition->BuildingId != Tutorial->LockedBuildingRowName)
    {
        return;
    }

    Tutorial->ReservedTutorialBuilding = InBuilding;

    SF_LOG(Log,
        "[SFTutorialSubsystem] 튜토리얼 건설 예약 | Building: %s | Row: %s",
        *GetNameSafe(InBuilding), *Tutorial->LockedBuildingRowName.ToString());
}

void USFTutorialSubsystem::NotifyTutorialBuildingPlacementCancelled(
    const UObject* InWorldContext,
    const ASFBaseBuilding* InBuilding)
{
    USFTutorialSubsystem* Tutorial = Get(InWorldContext);

    if (!IsValid(Tutorial) || !Tutorial->ReservedTutorialBuilding.IsValid() ||
        Tutorial->ReservedTutorialBuilding.Get() != InBuilding)
    {
        return;
    }

    SF_LOG(Log, "[SFTutorialSubsystem] 취소된 튜토리얼 건설 예약 해제 | Building: %s",
        *GetNameSafe(InBuilding));
    Tutorial->ReservedTutorialBuilding = nullptr;
}

// ─────────────────────────────────────────────────────────
// 자리 판정
//
// "표시된 자리에 지으세요" 를 말로만 하면 플레이어는 어디에 지어도 넘어간다.
// 그러면 다음 단계의 설명이 화면과 맞지 않게 되고, 튜토리얼이 가르치려던
// 것도 전달되지 않는다.
// ─────────────────────────────────────────────────────────
bool USFTutorialSubsystem::IsGuideStepLocationSatisfied(const UObject* InWorldContext) const
{
    if (!GuideSteps.IsValidIndex(GuideStepIndex))
    {
        return true;
    }

    const FSFStageIntroGuideStep& Step = GuideSteps[GuideStepIndex];

    if (!Step.bRequireNearTarget || Step.TargetKind != ESFGuideTargetKind::WorldLocation)
    {
        return true;
    }

    FVector NotifierLocation = FVector::ZeroVector;

    if (!TryGetNotifierLocation(InWorldContext, NotifierLocation))
    {
        SF_LOG(Warning,
            "[SFTutorialSubsystem] The guide step requires a nearby action but the signal has no location. "
            "It is accepted so the tutorial does not get stuck.");

        return true;
    }

    // 수평 거리로만 잰다. 건물은 바닥에, 신호를 보내는 액터의 원점은 그보다
    // 위에 있을 수 있어서 높이까지 재면 멀쩡한 위치가 밖으로 판정된다.
    const float DistanceSquared = FVector::DistSquaredXY(NotifierLocation, Step.WorldLocation);
    const float RadiusSquared = FMath::Square(FMath::Max(Step.WorldRadius, 1.f));

    if (DistanceSquared <= RadiusSquared)
    {
        return true;
    }

    SF_LOG(Log,
        "[SFTutorialSubsystem] The action happened outside the marked spot, so it does not count | "
        "Distance: %.0f | Allowed: %.0f",
        FMath::Sqrt(DistanceSquared), Step.WorldRadius);

    return false;
}

bool USFTutorialSubsystem::TryGetNotifierLocation(const UObject* InWorldContext, FVector& OutLocation)
{
    OutLocation = FVector::ZeroVector;

    if (InWorldContext == nullptr)
    {
        return false;
    }

    if (const AActor* Actor = Cast<AActor>(InWorldContext))
    {
        OutLocation = Actor->GetActorLocation();
        return true;
    }

    if (const UActorComponent* Component = Cast<UActorComponent>(InWorldContext))
    {
        if (const AActor* Owner = Component->GetOwner())
        {
            OutLocation = Owner->GetActorLocation();
            return true;
        }
    }

    // 위젯이나 서브오브젝트에서 부른 경우까지 받아준다.
    if (const AActor* OuterActor = InWorldContext->GetTypedOuter<AActor>())
    {
        OutLocation = OuterActor->GetActorLocation();
        return true;
    }

    return false;
}

void USFTutorialSubsystem::SnapPlacementGuideToBuildingGrid(
    FSFStageIntroGuideStep& InOutStep) const
{
    // 월드 안내 고스트가 아닌 칸, 또는 어떤 건물인지 모르는 고스트는
    // 그리드 보정을 할 근거가 없으므로 DT 좌표를 그대로 쓴다.
    if (!InOutStep.bShowPlacementGhost ||
        InOutStep.TargetKind != ESFGuideTargetKind::WorldLocation ||
        InOutStep.GhostBuildingDefinition.IsNull())
    {
        return;
    }

    UWorld* World = GetWorld();
    const USFBuildingDefinition* Definition =
        InOutStep.GhostBuildingDefinition.LoadSynchronous();
    USFBuildingGridSubsystem* GridSubsystem =
        IsValid(World) ? World->GetSubsystem<USFBuildingGridSubsystem>() : nullptr;

    if (!IsValid(Definition) || !IsValid(GridSubsystem))
    {
        return;
    }

    // UBuildingPlacement::UpdatePlacementMode와 같은 변환을 써야 한다.
    // 특히 1x1이 아닌 오두막/병영은 중심 좌표가 셀 크기의 절반만큼
    // 달라질 수 있어, DT 원시 좌표에 세운 고스트와 실제 건물이 어긋났다.
    const FBuildingGridInfo GridInfo = GridSubsystem->WorldCenterToGridInfo(
        InOutStep.WorldLocation,
        Definition->GridSize);

    if (!GridInfo.IsValid())
    {
        return;
    }

    FVector SnappedLocation = GridSubsystem->GridInfoToWorldCenter(GridInfo);
    // 그리드는 XY만 관리한다. 지형 높이는 DT가 보관한 값을 유지한다.
    SnappedLocation.Z = InOutStep.WorldLocation.Z;

    if (!SnappedLocation.Equals(InOutStep.WorldLocation, KINDA_SMALL_NUMBER))
    {
        SF_LOG(Log,
            "[SFTutorialSubsystem] Placement guide snapped to building grid | Building: %s | %s -> %s",
            *Definition->BuildingId.ToString(),
            *InOutStep.WorldLocation.ToString(),
            *SnappedLocation.ToString());
        InOutStep.WorldLocation = SnappedLocation;
    }
}

void USFTutorialSubsystem::SpawnPlacementGhost(const FSFStageIntroGuideStep& InStep)
{
    UWorld* World = GetWorld();

    if (!IsValid(World))
    {
        return;
    }

    // 무엇을 세울지 먼저 정한다.
    //
    // 건물 클래스를 지정했으면 그쪽이 우선이다. 메시만 꺼내 세우면 건물 BP 가
    // 메시에 걸어둔 회전·크기·바닥 보정이 전부 빠져서, 플레이어가 마우스로
    // 들고 있는 배치 미리보기와 크기도 방향도 어긋난다.
    UStaticMesh* GhostMesh = nullptr;
    FTransform GhostRelative = FTransform::Identity;

    if (!InStep.GhostBuildingDefinition.IsNull())
    {
        ResolveDefinitionGhostVisual(
            InStep.GhostBuildingDefinition.LoadSynchronous(),
            InStep.GhostTierIndex,
            GhostMesh,
            GhostRelative);
    }

    if (!IsValid(GhostMesh) && !InStep.GhostBuildingClass.IsNull())
    {
        UClass* BuildingClass = InStep.GhostBuildingClass.LoadSynchronous();

        if (!ResolveBuildingMeshTemplate(BuildingClass, GhostMesh, GhostRelative) || !IsValid(GhostMesh))
        {
            SF_LOG(Warning,
                "[SFTutorialSubsystem] No static mesh was found on the ghost building class. "
                "This project injects meshes from a Definition, so set Ghost Building Definition instead | Class: %s",
                BuildingClass != nullptr ? *BuildingClass->GetName() : TEXT("None"));
        }
    }

    if (!IsValid(GhostMesh))
    {
        GhostMesh = InStep.GhostMesh.LoadSynchronous();
    }

    if (!IsValid(GhostMesh))
    {
        SF_LOG(Warning, "[SFTutorialSubsystem] The guide step asks for a ghost but no mesh could be resolved.");
        return;
    }

    // 칸마다 다른 건물을 세울 수 있어야 하므로 클래스도 칸에서 가져온다.
    UClass* GhostClass = InStep.GhostActorClass.IsNull()
        ? nullptr
        : InStep.GhostActorClass.LoadSynchronous();

    if (GhostClass == nullptr)
    {
        // 지정하지 않았으면 C++ 기본 클래스로 세운다. 모양은 나오지만
        // 머티리얼이 없어 실제 건물과 구분이 안 되므로 경고를 남긴다.
        SF_LOG(Warning,
            "[SFTutorialSubsystem] The guide step has no ghost actor class. "
            "It falls back to the base class, so the ghost will not look translucent.");

        GhostClass = ASFTutorialGhostActor::StaticClass();
    }

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    ActiveGhost = World->SpawnActor<ASFTutorialGhostActor>(
        GhostClass,
        FTransform(InStep.GhostRotation, InStep.WorldLocation, InStep.GhostScale),
        Params);

    if (!IsValid(ActiveGhost))
    {
        SF_LOG(Error, "[SFTutorialSubsystem] The placement ghost failed to spawn.");
        return;
    }

    ActiveGhost->SetGhostMesh(GhostMesh, GhostRelative);

    // 스폰됐는데 안 보이는 경우와 아예 안 스폰된 경우를 구분할 수 있어야 한다.
    // 크기가 0 이면 스폰은 되고 화면에는 아무것도 없다 — 눈으로는 보이지
    // 않는 실패라서 로그에 값을 그대로 남긴다.
    SF_LOG(Log,
        "[SFTutorialSubsystem] Placement ghost spawned | Mesh: %s | Location: %s | Actor scale: %s | Mesh relative: %s",
        *GhostMesh->GetName(),
        *InStep.WorldLocation.ToString(),
        *InStep.GhostScale.ToString(),
        *GhostRelative.ToString());

    if (InStep.GhostScale.IsNearlyZero())
    {
        SF_LOG(Warning, "[SFTutorialSubsystem] The ghost scale is zero, so nothing will be visible.");
    }
}

// ─────────────────────────────────────────────────────────
// 건물 클래스에서 겉모습 꺼내기
//
// 두 곳을 본다.
//   1) 클래스 기본값(CDO) — C++ 생성자가 만든 컴포넌트가 여기 산다.
//      SFRootBase 계열의 StaticMeshComponent 가 이쪽이다.
//   2) SimpleConstructionScript — 블루프린트에서 손으로 추가한 컴포넌트.
//
// 프로젝트마다 어느 쪽에 있는지 다르고, 한 건물 안에 둘 다 있기도 하다.
// 먼저 찾은 쪽을 쓴다.
// ─────────────────────────────────────────────────────────
bool USFTutorialSubsystem::ResolveBuildingMeshTemplate(
    UClass* InBuildingClass,
    UStaticMesh*& OutMesh,
    FTransform& OutRelativeTransform
)
{
    OutMesh = nullptr;
    OutRelativeTransform = FTransform::Identity;

    if (InBuildingClass == nullptr)
    {
        return false;
    }

    // 1) 네이티브 컴포넌트. SFRootBase 계열의 StaticMeshComponent 가 여기다.
    if (const AActor* DefaultActor = InBuildingClass->GetDefaultObject<AActor>())
    {
        if (const UStaticMeshComponent* MeshComponent =
            DefaultActor->FindComponentByClass<UStaticMeshComponent>())
        {
            OutMesh = MeshComponent->GetStaticMesh();
            OutRelativeTransform = MeshComponent->GetRelativeTransform();
            return true;
        }
    }

    // 2) 블루프린트에서 추가한 컴포넌트.
    //    부모 클래스까지 거슬러 올라간다 — 상위 BP 에 있을 수 있다.
    for (UClass* Current = InBuildingClass; Current != nullptr; Current = Current->GetSuperClass())
    {
        UBlueprintGeneratedClass* BlueprintClass = Cast<UBlueprintGeneratedClass>(Current);

        if (BlueprintClass == nullptr || BlueprintClass->SimpleConstructionScript == nullptr)
        {
            continue;
        }

        for (const USCS_Node* Node : BlueprintClass->SimpleConstructionScript->GetAllNodes())
        {
            const UStaticMeshComponent* Template =
                Node != nullptr ? Cast<UStaticMeshComponent>(Node->ComponentTemplate) : nullptr;

            if (Template == nullptr)
            {
                continue;
            }

            OutMesh = Template->GetStaticMesh();
            OutRelativeTransform = Template->GetRelativeTransform();
            return true;
        }
    }

    return false;
}

bool USFTutorialSubsystem::ResolveDefinitionGhostVisual(
    const USFBuildingDefinition* InDefinition,
    int32 InTierIndex,
    UStaticMesh*& OutMesh,
    FTransform& OutRelativeTransform
)
{
    OutMesh = nullptr;
    OutRelativeTransform = FTransform::Identity;

    if (!IsValid(InDefinition))
    {
        SF_LOG(Warning, "[SFTutorialSubsystem] The ghost building definition could not be loaded.");
        return false;
    }

    const FSFBuildingTierData* Tier = InDefinition->FindTier(InTierIndex);

    if (Tier == nullptr)
    {
        SF_LOG(Warning, "[SFTutorialSubsystem] Ghost tier %d does not exist | Definition: %s",
            InTierIndex, *InDefinition->GetName());
        return false;
    }

    OutMesh = Tier->Visual.CompletedMesh.LoadSynchronous();

    if (!IsValid(OutMesh))
    {
        SF_LOG(Warning, "[SFTutorialSubsystem] Tier %d has no completed mesh | Definition: %s",
            InTierIndex, *InDefinition->GetName());
        return false;
    }

    // 상대 트랜스폼은 Definition 이 아니라 건물 클래스에 있다.
    // 메시는 런타임 주입이라 클래스 쪽 메시는 비어 있는 게 정상이므로,
    // 여기서는 트랜스폼만 가져다 쓴다.
    //
    // LoadSynchronous() 대신 경로로 로드하는 이유: TSoftClassPtr 의
    // LoadSynchronous 는 대상 타입이 완전해야 해서 ASFBaseBuilding 헤더를
    // 통째로 끌어와야 한다. 튜토리얼이 건물 구현을 알 이유가 없으므로
    // 경로만 받아 UClass 로 캐스팅한다.
    const FSoftObjectPath BuildingClassPath = InDefinition->BuildingClass.ToSoftObjectPath();

    if (UClass* BuildingClass = Cast<UClass>(BuildingClassPath.TryLoad()))
    {
        UStaticMesh* UnusedMesh = nullptr;
        FTransform Relative = FTransform::Identity;

        if (ResolveBuildingMeshTemplate(BuildingClass, UnusedMesh, Relative))
        {
            OutRelativeTransform = Relative;
        }
    }
    else
    {
        SF_LOG(Warning,
            "[SFTutorialSubsystem] The building class could not be loaded, so the ghost transform may not match | Definition: %s",
            *InDefinition->GetName());
    }

    return true;
}

void USFTutorialSubsystem::DestroyPlacementGhost()
{
    if (IsValid(ActiveGhost))
    {
        ActiveGhost->Destroy();
    }

    ActiveGhost = nullptr;
}

void USFTutorialSubsystem::ResetCompletedObjectives()
{
    CompletedObjectives.Reset();

    SF_LOG(Log, "[SFTutorialSubsystem] Completed objectives cleared.");
}

bool USFTutorialSubsystem::IsWaitingForObjective() const
{
    return PendingObjectiveTag.IsValid();
}

FGameplayTag USFTutorialSubsystem::GetActiveObjectiveTag() const
{
    return PendingObjectiveTag;
}

bool USFTutorialSubsystem::IsObjectiveCompleted(FGameplayTag InObjectiveTag) const
{
    return InObjectiveTag.IsValid() && CompletedObjectives.HasTag(InObjectiveTag);
}

// ─────────────────────────────────────────────────────────
// 디버그
//
// 대기가 안 풀리는 원인은 거의 언제나 태그 불일치다. 기다리는 태그와 실제로
// 날아온 태그를 눈으로 비교할 수 있어야 한다.
// ─────────────────────────────────────────────────────────
void USFTutorialSubsystem::DumpState() const
{
    SF_LOG(Warning, "===== Tutorial state =====");

    if (PendingObjectiveTag.IsValid())
    {
        SF_LOG(Warning, "  Waiting for : %s", *PendingObjectiveTag.ToString());
        SF_LOG(Warning, "  Resume      : %s / %s / node %d",
            IsValid(PendingDataTable) ? *PendingDataTable->GetName() : TEXT("None"),
            *PendingRowName.ToString(),
            PendingResumeNodeIndex);
    }
    else
    {
        SF_LOG(Warning, "  Not waiting for any objective.");
    }

    // 잠금은 안내가 끝난 뒤에도 살아 있으므로 안내 정보 밖에 찍는다.
    // 갇혔을 때 가장 먼저 봐야 하는 값이다.
    SF_LOG(Warning, "  Build lock  : %s | armed by step %d",
        LockedBuildingRowName.IsNone() ? TEXT("(none)") : *LockedBuildingRowName.ToString(),
        LockedBuildingStepIndex);

    SF_LOG(Warning, "  Feature lock: %s",
        LockedFeatures.IsEmpty() ? TEXT("(none)") : *LockedFeatures.ToStringSimple());

    if (GuideStepIndex != INDEX_NONE)
    {
        SF_LOG(Warning, "  Guide       : step %d / %d", GuideStepIndex + 1, GuideSteps.Num());

        // 안내가 안 넘어갈 때 확인해야 할 값은 전부 여기 있다.
        // DT 에 적어 둔 값과 눈으로 대조하라고 한 번에 찍는다.
        if (GuideSteps.IsValidIndex(GuideStepIndex))
        {
            const FSFStageIntroGuideStep& Step = GuideSteps[GuideStepIndex];

            SF_LOG(Warning, "  Step wants  : Completion=%s | Tag=%s",
                *StaticEnum<ESFGuideCompletion>()->GetNameStringByValue(static_cast<int64>(Step.Completion)),
                Step.CompletionTag.IsValid() ? *Step.CompletionTag.ToString() : TEXT("(none)"));

            SF_LOG(Warning, "  Step needs  : RequiredBuildingRowName=%s",
                Step.RequiredBuildingRowName.IsNone()
                ? TEXT("(none)")
                : *Step.RequiredBuildingRowName.ToString());

            // 종류에 따라 실제로 쓰이는 값이 다르다.
            // 반대쪽 값을 찍으면 멀쩡해 보이는 숫자에 속는다.
            if (Step.TargetKind == ESFGuideTargetKind::ScreenRegion)
            {
                SF_LOG(Warning, "  Step target : Kind=ScreenRegion | Anchor=%s | ScreenRadius=%.3f",
                    *Step.ScreenAnchor.ToString(),
                    Step.ScreenRadius);
                SF_LOG(Warning, "                (ScreenRegion 은 bRequireNearTarget 과 건설 제한을 쓰지 않는다)");
            }
            else
            {
                SF_LOG(Warning, "  Step target : Kind=WorldLocation | RequireNear=%s | Location=%s | WorldRadius=%.0f",
                    Step.bRequireNearTarget ? TEXT("YES") : TEXT("no"),
                    *Step.WorldLocation.ToCompactString(),
                    Step.WorldRadius);
            }
        }
    }
    else
    {
        SF_LOG(Warning, "  Guide       : not running");
    }

    SF_LOG(Warning, "  Completed (%d) : %s",
        CompletedObjectives.Num(),
        CompletedObjectives.IsEmpty() ? TEXT("none") : *CompletedObjectives.ToStringSimple());
}

#if !UE_BUILD_SHIPPING
static void SFTutorialDumpState(const TArray<FString>& Args, UWorld* World)
{
    const USFTutorialSubsystem* Tutorial = IsValid(World) ? World->GetSubsystem<USFTutorialSubsystem>() : nullptr;

    if (!IsValid(Tutorial))
    {
        SF_LOG(Error, "[SFTutorialSubsystem] Subsystem was not found. Are you in play mode?");
        return;
    }

    Tutorial->DumpState();
}

// ─────────────────────────────────────────────────────────
// 입력 상태 덤프
//
// "클릭이 안 된다" 는 증상 하나에 원인이 여럿 얽혀 있다.
//   - 게임이 멈춰 있다        → 입력 액션이 아예 실행되지 않는다
//   - 위젯이 포커스를 쥐고 있다 → 키가 그쪽으로 간다
//   - 커서가 없다             → 클릭 지점이 없다
// 눈으로는 셋이 똑같이 보이므로 값으로 찍어 가른다.
// ─────────────────────────────────────────────────────────
/**
 * 입력이 막히는 순간을 콘솔로 관찰할 수 없다는 문제가 있다.
 *
 * 에디터 Output Log 의 입력창에 명령을 치면 그쪽이 포커스를 가져가고,
 * 게임 안 콘솔(~)을 열면 콘솔이 포커스를 뷰포트로 되돌려 준다.
 * 어느 쪽이든 "관찰 행위가 상태를 바꿔 버려서" 고장난 순간을 볼 수 없다.
 *
 * 그래서 콘솔을 건드리지 않고 상태를 계속 찍어 두는 감시기를 둔다.
 * 일시정지 중에도 돌아야 하므로 월드 타이머가 아니라 FTSTicker 를 쓴다.
 *
 *   sf.Tutorial.WatchInput 1   켜기
 *   sf.Tutorial.WatchInput 0   끄기
 */
namespace SFTutorialInputWatch
{
    static FTSTicker::FDelegateHandle TickerHandle;
    static TWeakObjectPtr<UWorld> WatchedWorld;
    static FString LastSignature;
    static float HeartbeatAccumulator = 0.f;

    /** 커서 아래에 쌓여 있는 위젯을 잎에서 뿌리 방향으로 나열한다. */
    static FString BuildHoverPath()
    {
        if (!FSlateApplication::IsInitialized())
        {
            return TEXT("(slate not ready)");
        }

        FSlateApplication& Slate = FSlateApplication::Get();

        const FWidgetPath Path = Slate.LocateWindowUnderMouse(
            Slate.GetCursorPos(),
            Slate.GetInteractiveTopLevelWindows());

        if (!Path.IsValid() || Path.Widgets.Num() <= 0)
        {
            return TEXT("(none)");
        }

        FString Result;

        // 뿌리까지 다 찍으면 한 줄이 너무 길어진다.
        // 게임을 가리고 있는 놈은 잎 쪽에 있으므로 앞의 몇 개면 충분하다.
        const int32 MaxDepth = 6;

        for (int32 Index = Path.Widgets.Num() - 1, Printed = 0;
            Index >= 0 && Printed < MaxDepth;
            --Index, ++Printed)
        {
            const TSharedRef<SWidget>& Widget = Path.Widgets[Index].Widget;

            if (!Result.IsEmpty())
            {
                Result += TEXT(" < ");
            }

            Result += Widget->GetTypeAsString();

            if (Widget->GetType() == TEXT("SObjectWidget"))
            {
                const SObjectWidget& ObjectWidget =
                    static_cast<const SObjectWidget&>(Widget.Get());

                if (const UUserWidget* UserWidget = ObjectWidget.GetWidgetObject())
                {
                    Result += FString::Printf(TEXT("(%s)"), *UserWidget->GetClass()->GetName());
                }
            }
        }

        return Result;
    }

    /** 마우스가 움직일 때마다 바뀌므로 변화 감지에서는 제외한다. */
    static FString BuildStateSignature(UWorld* World)
    {
        FString Signature;

        Signature += FString::Printf(
            TEXT("Paused=%d"),
            UGameplayStatics::IsGamePaused(World) ? 1 : 0);

        if (APlayerController* PlayerController = World->GetFirstPlayerController())
        {
            Signature += FString::Printf(
                TEXT(" | Cursor=%d Click=%d Look=%d Move=%d"),
                PlayerController->bShowMouseCursor ? 1 : 0,
                PlayerController->bEnableClickEvents ? 1 : 0,
                PlayerController->IsLookInputIgnored() ? 1 : 0,
                PlayerController->IsMoveInputIgnored() ? 1 : 0);
        }

        if (UGameViewportClient* ViewportClient = World->GetGameViewport())
        {
            Signature += FString::Printf(
                TEXT(" | Capture=%d Lock=%d IgnoreInput=%d"),
                static_cast<int32>(ViewportClient->GetMouseCaptureMode()),
                static_cast<int32>(ViewportClient->GetMouseLockMode()),
                ViewportClient->IgnoreInput() ? 1 : 0);
        }

        if (FSlateApplication::IsInitialized())
        {
            const TSharedPtr<SWidget> Focused =
                FSlateApplication::Get().GetUserFocusedWidget(0);

            Signature += FString::Printf(
                TEXT(" | Focus=%s Captor=%d"),
                Focused.IsValid() ? *Focused->GetTypeAsString() : TEXT("none"),
                FSlateApplication::Get().HasAnyMouseCaptor() ? 1 : 0);
        }

        if (const USFTutorialSubsystem* Tutorial = World->GetSubsystem<USFTutorialSubsystem>())
        {
            Signature += FString::Printf(
                TEXT(" | Guide=%d Waiting=%d"),
                Tutorial->IsGuideActive() ? 1 : 0,
                Tutorial->IsWaitingForObjective() ? 1 : 0);
        }

        return Signature;
    }

    /** 커서 아래에 실제로 무엇이 잡히는지. 선택이 안 되는 원인을 가른다. */
    static FString BuildTraceInfo(UWorld* World)
    {
        APlayerController* PlayerController = World->GetFirstPlayerController();

        if (!IsValid(PlayerController))
        {
            return TEXT("(no controller)");
        }

        FHitResult HitResult;

        const bool bHit = PlayerController->GetHitResultUnderCursor(
            ECC_Visibility,
            false,
            HitResult);

        const AActor* HitActor = bHit ? HitResult.GetActor() : nullptr;

        return IsValid(HitActor) ? HitActor->GetName() : TEXT("none");
    }

    static bool Tick(float InDeltaTime)
    {
        UWorld* World = WatchedWorld.Get();

        if (!IsValid(World))
        {
            TickerHandle.Reset();
            LastSignature.Empty();
            return false;
        }

        const FString Signature = BuildStateSignature(World);

        HeartbeatAccumulator += InDeltaTime;

        // 상태가 바뀌면 즉시, 아니면 2초마다 한 번.
        // 마우스 위치는 신호에서 뺐으므로 움직인다고 로그가 쏟아지지 않는다.
        const bool bChanged = Signature != LastSignature;
        const bool bHeartbeat = HeartbeatAccumulator >= 2.f;

        if (bChanged || bHeartbeat)
        {
            LastSignature = Signature;
            HeartbeatAccumulator = 0.f;

            SF_LOG(
                Warning,
                "[SFTutorialWatch]%s %s | UnderCursor=%s | Hover=%s",
                bChanged ? TEXT(" *") : TEXT(""),
                *Signature,
                *BuildTraceInfo(World),
                *BuildHoverPath());
        }

        return true;
    }

    static void Stop()
    {
        if (TickerHandle.IsValid())
        {
            FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
            TickerHandle.Reset();
        }

        WatchedWorld.Reset();
        LastSignature.Empty();
        HeartbeatAccumulator = 0.f;
    }
}

static void SFTutorialWatchInput(const TArray<FString>& Args, UWorld* World)
{
    const bool bEnable = Args.Num() <= 0 || Args[0] != TEXT("0");

    SFTutorialInputWatch::Stop();

    if (!bEnable)
    {
        SF_LOG(Warning, "[SFTutorialSubsystem] Input watch stopped.");
        return;
    }

    if (!IsValid(World))
    {
        SF_LOG(Error, "[SFTutorialSubsystem] World was not found. Are you in play mode?");
        return;
    }

    SFTutorialInputWatch::WatchedWorld = World;
    SFTutorialInputWatch::TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateStatic(&SFTutorialInputWatch::Tick),
        0.25f);

    SF_LOG(Warning, "[SFTutorialSubsystem] Input watch started. 콘솔을 닫고 증상을 재현하세요.");
}

static FAutoConsoleCommandWithWorldAndArgs GSFTutorialWatchInput(
    TEXT("sf.Tutorial.WatchInput"),
    TEXT("입력 상태를 계속 감시해서 바뀔 때마다 로그로 남긴다. 인자 0 이면 끈다."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(SFTutorialWatchInput));

static void SFTutorialDumpInput(const TArray<FString>& Args, UWorld* World)
{
    if (!IsValid(World))
    {
        return;
    }

    SF_LOG(Warning, "===== Input state =====");
    SF_LOG(Warning, "  Game paused    : %s", UGameplayStatics::IsGamePaused(World) ? TEXT("YES") : TEXT("no"));

    if (const APlayerController* PlayerController = World->GetFirstPlayerController())
    {
        SF_LOG(Warning, "  Show cursor    : %s", PlayerController->bShowMouseCursor ? TEXT("yes") : TEXT("NO"));
        SF_LOG(Warning, "  Ignore look    : %d | Ignore move : %d",
            PlayerController->IsLookInputIgnored() ? 1 : 0,
            PlayerController->IsMoveInputIgnored() ? 1 : 0);
    }
    else
    {
        SF_LOG(Warning, "  PlayerController was not found.");
    }

    if (FSlateApplication::IsInitialized())
    {
        const TSharedPtr<SWidget> Focused = FSlateApplication::Get().GetUserFocusedWidget(0);

        SF_LOG(Warning, "  Focused widget : %s",
            Focused.IsValid() ? *Focused->GetTypeAsString() : TEXT("(none)"));

        // 타입 이름만으로는 어느 위젯 블루프린트에 있는 놈인지 알 수 없다.
        // 부모를 타고 올라가면서 SObjectWidget 을 만나면 그게 UMG 위젯이므로,
        // 거기서 UUserWidget 의 클래스 이름을 뽑아 범인을 특정한다.
        if (Focused.IsValid())
        {
            FString Chain;
            int32 Depth = 0;

            for (TSharedPtr<SWidget> Node = Focused; Node.IsValid() && Depth < 32; Node = Node->GetParentWidget(), ++Depth)
            {
                if (!Chain.IsEmpty())
                {
                    Chain += TEXT(" < ");
                }

                Chain += Node->GetTypeAsString();

                if (Node->GetType() == TEXT("SObjectWidget"))
                {
                    const SObjectWidget* ObjectWidget = static_cast<const SObjectWidget*>(Node.Get());
                    const UUserWidget* UserWidget = ObjectWidget != nullptr ? ObjectWidget->GetWidgetObject() : nullptr;

                    if (IsValid(UserWidget))
                    {
                        Chain += FString::Printf(TEXT("(%s)"), *UserWidget->GetClass()->GetName());
                    }
                }
            }

            SF_LOG(Warning, "  Focus chain    : %s", *Chain);
        }

        // GetMouseCaptor 는 5.5 에 없다. 누가 잡았는지까지는 몰라도
        // 잡혀 있는지 여부만 알면 원인을 가르는 데는 충분하다.
        SF_LOG(Warning, "  Mouse captured : %s",
            FSlateApplication::Get().HasAnyMouseCaptor() ? TEXT("YES") : TEXT("no"));
    }

    if (UGameViewportClient* ViewportClient = World->GetGameViewport())
    {
        SF_LOG(Warning, "  Viewport       : Capture=%d Lock=%d IgnoreInput=%d",
            static_cast<int32>(ViewportClient->GetMouseCaptureMode()),
            static_cast<int32>(ViewportClient->GetMouseLockMode()),
            ViewportClient->IgnoreInput() ? 1 : 0);
    }

    SF_LOG(Warning, "  Under cursor   : %s", *SFTutorialInputWatch::BuildTraceInfo(World));
    SF_LOG(Warning, "  Hover path     : %s", *SFTutorialInputWatch::BuildHoverPath());

    if (const USFTutorialSubsystem* Tutorial = World->GetSubsystem<USFTutorialSubsystem>())
    {
        SF_LOG(Warning, "  Guide active   : %s | Waiting: %s",
            Tutorial->IsGuideActive() ? TEXT("YES") : TEXT("no"),
            Tutorial->IsWaitingForObjective() ? TEXT("YES") : TEXT("no"));
    }
}

// 포커스가 편집 가능한 텍스트 위젯에 잡혀 있으면 뷰포트가 입력을 못 받는다.
// 이 명령으로 조작이 돌아오면 원인은 포커스이고, 그대로면 다른 곳을 봐야 한다.
static void SFTutorialFixFocus(const TArray<FString>& Args, UWorld* World)
{
    if (!IsValid(World) || !FSlateApplication::IsInitialized())
    {
        return;
    }

    FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::SetDirectly);

    if (UGameViewportClient* ViewportClient = World->GetGameViewport())
    {
        if (const TSharedPtr<SViewport> ViewportWidget = ViewportClient->GetGameViewportWidget())
        {
            FSlateApplication::Get().SetAllUserFocus(ViewportWidget, EFocusCause::SetDirectly);
        }
    }

    SF_LOG(Warning, "[SFTutorialSubsystem] Focus was forced back to the game viewport.");
}

static FAutoConsoleCommandWithWorldAndArgs GSFTutorialFixFocus(
    TEXT("sf.Tutorial.FixFocus"),
    TEXT("키보드 포커스를 게임 뷰포트로 되돌린다. 입력이 막혔을 때 원인이 포커스인지 가른다."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(SFTutorialFixFocus));

static FAutoConsoleCommandWithWorldAndArgs GSFTutorialDumpInput(
    TEXT("sf.Tutorial.DumpInput"),
    TEXT("일시정지·포커스·커서 상태를 찍는다. 조작이 막혔을 때 원인을 가른다."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(SFTutorialDumpInput));

// 화면이 클릭을 먹는 원인이 스포트라이트인지 가르는 용도다.
// 이 명령으로 안내를 끊었을 때 조작이 돌아오면 범인은 위젯이고,
// 그대로면 다른 곳을 봐야 한다.
static void SFTutorialCancelGuide(const TArray<FString>& Args, UWorld* World)
{
    USFTutorialSubsystem* Tutorial = IsValid(World) ? World->GetSubsystem<USFTutorialSubsystem>() : nullptr;

    if (!IsValid(Tutorial))
    {
        SF_LOG(Error, "[SFTutorialSubsystem] Subsystem was not found. Are you in play mode?");
        return;
    }

    if (!Tutorial->IsGuideActive())
    {
        SF_LOG(Warning, "[SFTutorialSubsystem] No guide is active.");
        return;
    }

    Tutorial->CancelGuide();

    SF_LOG(Warning, "[SFTutorialSubsystem] The guide was cancelled by console command.");
}

static FAutoConsoleCommandWithWorldAndArgs GSFTutorialCancelGuide(
    TEXT("sf.Tutorial.CancelGuide"),
    TEXT("진행 중인 단계별 안내를 강제로 끝낸다. 입력이 막히는 원인을 가를 때 쓴다."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(SFTutorialCancelGuide));

// 잠금이 안 풀려 건설 메뉴에 갇히는 사고에서 빠져나오는 용도다.
// 완료 태그가 Tutorial.Build 아래에 없으면 실제로 갇힐 수 있다.
static void SFTutorialUnlockBuilding(const TArray<FString>& Args, UWorld* World)
{
    USFTutorialSubsystem* Tutorial = IsValid(World) ? World->GetSubsystem<USFTutorialSubsystem>() : nullptr;

    if (!IsValid(Tutorial))
    {
        SF_LOG(Error, "[SFTutorialSubsystem] Subsystem was not found. Are you in play mode?");
        return;
    }

    Tutorial->UnlockBuildingSelection();

    SF_LOG(Warning, "[SFTutorialSubsystem] The building lock was released by console command.");
}

static FAutoConsoleCommandWithWorldAndArgs GSFTutorialUnlockBuilding(
    TEXT("sf.Tutorial.UnlockBuilding"),
    TEXT("튜토리얼의 건물 선택 제한을 강제로 푼다. 잠금이 안 풀려 갇혔을 때 쓴다."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(SFTutorialUnlockBuilding));

static FAutoConsoleCommandWithWorldAndArgs GSFTutorialDumpState(
    TEXT("sf.Tutorial.DumpState"),
    TEXT("Prints the objective the tutorial is waiting for and the ones already completed."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SFTutorialDumpState));

/**
 * 대기를 강제로 통과시킨다.
 *
 * 튜토리얼 뒷부분을 확인할 때마다 건물을 실제로 짓고 있을 수는 없다.
 * 인자를 주면 그 태그로, 안 주면 지금 기다리는 태그로 신호를 보낸다.
 *
 *   sf.Tutorial.CompleteObjective
 *   sf.Tutorial.CompleteObjective Tutorial.Build.Barracks
 */
static void SFTutorialCompleteObjective(const TArray<FString>& Args, UWorld* World)
{
    USFTutorialSubsystem* Tutorial = IsValid(World) ? World->GetSubsystem<USFTutorialSubsystem>() : nullptr;

    if (!IsValid(Tutorial))
    {
        SF_LOG(Error, "[SFTutorialSubsystem] Subsystem was not found. Are you in play mode?");
        return;
    }

    FGameplayTag Tag = Args.Num() > 0
        ? FGameplayTag::RequestGameplayTag(FName(*Args[0]), false)
        : Tutorial->GetActiveObjectiveTag();

    if (!Tag.IsValid())
    {
        SF_LOG(Error,
            "[SFTutorialSubsystem] No valid tag. Pass one as an argument, "
            "or run this while the tutorial is waiting.");
        return;
    }

    USFTutorialSubsystem::NotifyObjective(World, Tag);
}

static FAutoConsoleCommandWithWorldAndArgs GSFTutorialCompleteObjective(
    TEXT("sf.Tutorial.CompleteObjective"),
    TEXT("Fires an objective signal. With no argument it completes the objective currently being waited on."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SFTutorialCompleteObjective));
#endif
