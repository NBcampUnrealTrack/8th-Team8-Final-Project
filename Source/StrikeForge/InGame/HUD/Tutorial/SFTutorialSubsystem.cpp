#include "InGame/HUD/Tutorial/SFTutorialSubsystem.h"

#include "Engine/DataTable.h"
#include "InGame/HUD/Tutorial/SFTutorialGhostActor.h"
#include "TimerManager.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Logging/SFLog.h"

UE_DEFINE_GAMEPLAY_TAG(TAG_SF_Tutorial_Command_Move, "Tutorial.Command.Move");

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
    FGameplayTag InObjectiveTag,
    const FText& InBannerText
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
    PendingBannerText = InBannerText;
    PendingDataTable = InDataTable;
    PendingRowName = InRowName;
    PendingResumeNodeIndex = InResumeNodeIndex;

    SF_LOG(Log, "[SFTutorialSubsystem] Waiting for objective | Tag: %s | Resume node: %d",
        *InObjectiveTag.ToString(), InResumeNodeIndex);

    OnObjectiveStarted.Broadcast(PendingObjectiveTag, PendingBannerText);

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

    // 배너를 지울 수 있게 알린다. 재개 델리게이트는 쏘지 않는다 —
    // 취소는 "그만둔다" 이지 "다음으로 간다" 가 아니다.
    const FGameplayTag CancelledTag = PendingObjectiveTag;

    PendingObjectiveTag = FGameplayTag();
    PendingBannerText = FText::GetEmpty();
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

    // 계층 매칭. 기다리는 쪽이 부모 태그면 자식 태그로도 만족한다.
    //   기다림 Tutorial.Build  +  신호 Tutorial.Build.Barracks  → 만족
    //   기다림 Tutorial.Build.Barracks  +  신호 Tutorial.Build  → 불만족
    const bool bMatchesPending =
        Tutorial->PendingObjectiveTag.IsValid() && InObjectiveTag.MatchesTag(Tutorial->PendingObjectiveTag);

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
    Tutorial->PendingBannerText = FText::GetEmpty();
    Tutorial->PendingDataTable = nullptr;
    Tutorial->PendingRowName = NAME_None;
    Tutorial->PendingResumeNodeIndex = INDEX_NONE;

    Tutorial->OnObjectiveCompleted.Broadcast(CompletedTag);

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
    const FSFStageIntroGuideStep& Step = GuideSteps[GuideStepIndex];

    // 신호를 기다리는 칸만 대기 태그를 건다.
    // Click / Timer 칸은 스포트라이트나 타이머가 직접 넘기므로 대기가 없다.
    PendingObjectiveTag = FGameplayTag();
    PendingBannerText = Step.InstructionText;

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

    ClearGuideState();

    OnGuideFinished.Broadcast();
}

void USFTutorialSubsystem::ClearGuideState()
{
    DestroyPlacementGhost();

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(GuideAutoAdvanceHandle);
    }

    GuideSteps.Empty();
    GuideStepIndex = INDEX_NONE;

    PendingObjectiveTag = FGameplayTag();
    PendingBannerText = FText::GetEmpty();
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

void USFTutorialSubsystem::SpawnPlacementGhost(const FSFStageIntroGuideStep& InStep)
{
    if (InStep.GhostMesh.IsNull())
    {
        SF_LOG(Warning, "[SFTutorialSubsystem] The guide step asks for a ghost but no mesh is set.");
        return;
    }

    UWorld* World = GetWorld();

    if (!IsValid(World))
    {
        return;
    }

    UClass* GhostClass = GhostActorClass.IsNull()
        ? ASFTutorialGhostActor::StaticClass()
        : GhostActorClass.LoadSynchronous();

    if (GhostClass == nullptr)
    {
        GhostClass = ASFTutorialGhostActor::StaticClass();
    }

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    ActiveGhost = World->SpawnActor<ASFTutorialGhostActor>(
        GhostClass,
        FTransform(InStep.GhostRotation, InStep.WorldLocation, InStep.GhostScale),
        Params);

    if (IsValid(ActiveGhost))
    {
        ActiveGhost->SetGhostMesh(InStep.GhostMesh.LoadSynchronous());
    }
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

FText USFTutorialSubsystem::GetActiveObjectiveText() const
{
    return PendingBannerText;
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
        SF_LOG(Warning, "  Banner      : %s", *PendingBannerText.ToString());
        SF_LOG(Warning, "  Resume      : %s / %s / node %d",
            IsValid(PendingDataTable) ? *PendingDataTable->GetName() : TEXT("None"),
            *PendingRowName.ToString(),
            PendingResumeNodeIndex);
    }
    else
    {
        SF_LOG(Warning, "  Not waiting for any objective.");
    }

    if (GuideStepIndex != INDEX_NONE)
    {
        SF_LOG(Warning, "  Guide       : step %d / %d", GuideStepIndex + 1, GuideSteps.Num());
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
