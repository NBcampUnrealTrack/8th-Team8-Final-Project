#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"
#include "InGame/HUD/Tutorial/SFStageIntroData.h"
#include "Subsystems/WorldSubsystem.h"
#include "SFTutorialSubsystem.generated.h"

class UDataTable;

/**
 * "유닛에게 이동 명령을 내렸다".
 *
 * 네이티브 태그라 DefaultGameplayTags.ini 에 따로 적지 않아도 된다.
 * 전투 코드가 튜토리얼 태그 문자열을 직접 들고 있게 하고 싶지 않아서
 * 여기에 둔다 — 이름이 바뀌어도 고칠 곳이 한 군데다.
 */
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SF_Tutorial_Command_Move);

class ASFTutorialGhostActor;

/** 목표 대기가 시작됐다. 목표 배너를 띄우는 쪽이 구독한다 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FSFTutorialObjectiveStarted, FGameplayTag, ObjectiveTag, const FText&, BannerText);

/** 목표가 달성됐다. 배너를 지우거나 연출을 낼 때 구독한다 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FSFTutorialObjectiveCompleted, FGameplayTag, ObjectiveTag);

/**
 * 목표 신호가 기록됐다. 대기 중이 아니어도 발동한다.
 *
 * OnObjectiveCompleted 와의 차이가 중요하다.
 *   OnObjectiveCompleted : 튜토리얼이 그 목표를 기다리고 있었을 때만 발동
 *   OnObjectiveRecorded  : 유효한 신호가 오면 무조건 발동
 *
 * 게임플레이 쪽에서 "집을 지으면 적이 나온다" 같은 반응을 걸 때는 이쪽을
 * 써야 한다. 튜토리얼이 아직 그 목표를 말하기 전에 플레이어가 먼저
 * 해버리면 OnObjectiveCompleted 는 영영 오지 않기 때문이다.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FSFTutorialObjectiveRecorded, FGameplayTag, ObjectiveTag);

/**
 * 단계별 안내의 한 칸이 시작됐다.
 *
 * 스포트라이트를 띄우는 쪽(HUD)이 구독한다. 받은 Step 을 그대로
 * USFTutorialSpotlightWidget::ApplyGuideStep 에 넘기면 된다.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
    FSFTutorialGuideStepStarted, const FSFStageIntroGuideStep&, Step, int32, StepIndex, int32, StepCount);

/** 단계별 안내가 끝났거나 취소됐다. 스포트라이트를 지울 때 구독한다 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSFTutorialGuideFinished);

/**
 * 튜토리얼을 다시 열어야 한다. 위젯을 띄우는 쪽이 구독한다.
 *
 * 받은 세 값을 그대로 USFStageIntroWidget::StartStageIntroAt 에 넘기면
 * 멈췄던 자리에서 이어진다.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
    FSFTutorialResumeRequested, UDataTable*, DataTable, FName, RowName, int32, NodeIndex);

/**
 * 튜토리얼 진행 중 "플레이어가 무언가 할 때까지" 를 들고 있는 곳.
 *
 * ── 왜 위젯이 아니라 여기인가 ───────────────────────────
 *
 * USFStageIntroWidget 은 대기에 들어가는 순간 닫힌다. 안 닫으면 플레이어가
 * 건물을 지을 수가 없기 때문이다. 그런데 위젯이 닫히면 NativeDestruct 가
 * ActiveNodes 를 비우므로 "어디까지 왔는지" 가 함께 사라진다.
 *
 * 그래서 대기에 필요한 최소한(어느 테이블/행의 몇 번 노드로 돌아와야 하는가,
 * 무엇을 기다리는가)만 위젯 밖으로 뺐다. 노드를 그리는 일은 여전히 위젯이
 * 전부 한다 — 진행 로직까지 옮기면 위젯을 크게 뒤집어야 하고, 지금 필요한
 * 것은 그게 아니다.
 *
 * ── 신호는 어디서 오는가 ────────────────────────────────
 *
 * 게임플레이 쪽에서 NotifyObjective 를 부른다. static BlueprintCallable 이라
 * 건물 BP 든 GameMode 든 아무 데서나 노드 하나로 부를 수 있고, 그쪽 C++ 를
 * 고칠 필요가 없다.
 *
 * ── 배너는 왜 여기서 안 만드는가 ────────────────────────
 *
 * 위젯을 어떻게 띄우는지(CommonUI 스택 푸시인지 AddToViewport 인지)는 HUD 쪽
 * 사정이다. 서브시스템이 그걸 알면 HUD 구조가 바뀔 때마다 여기도 바뀐다.
 * 델리게이트만 쏘고, 실제로 무엇을 띄울지는 구독하는 쪽이 정한다.
 */
UCLASS()
class STRIKEFORGE_API USFTutorialSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    /** 어디서든 접근하는 헬퍼. WorldContext 는 보통 this */
    static USFTutorialSubsystem* Get(const UObject* WorldContext);

    // ── 대기 등록 (튜토리얼 위젯이 호출) ────────────────

    /**
     * 목표 대기를 시작한다.
     *
     * @param InResumeNodeIndex 목표를 달성하면 돌아올 노드 번호
     * @return 실제로 대기에 들어갔으면 true.
     *         이미 달성된 목표라 기다릴 필요가 없으면 false 이며, 이 경우
     *         델리게이트를 쏘지 않는다 — 호출부가 그냥 다음 노드로 넘어가면 된다.
     *         여기서 재개 델리게이트를 쏘면 아직 닫지도 않은 위젯을 다시 열게 되어
     *         위젯이 둘이 되거나 방금 연 것을 곧바로 닫는 일이 생긴다.
     */
    bool BeginObjectiveWait(
        UDataTable* InDataTable,
        FName InRowName,
        int32 InResumeNodeIndex,
        FGameplayTag InObjectiveTag,
        const FText& InBannerText);

    /** 대기를 취소한다. 튜토리얼을 중간에 끄거나 스테이지를 나갈 때 부른다 */
    UFUNCTION(BlueprintCallable, Category = "SF|Tutorial")
    void CancelObjectiveWait();

    // ── 단계별 안내 ─────────────────────────────────────
    //
    // 목표 대기와 같은 자리에 산다. 안내 한 칸이 곧 목표 대기 하나이고,
    // 마지막 칸이 끝났을 때만 튜토리얼로 돌아간다. 대기 상태를 두 벌
    // 만들면 "지금 무엇을 기다리는가" 의 답이 두 개가 되어버린다.

    /**
     * 단계별 안내를 시작한다.
     *
     * @param InResumeNodeIndex 모든 칸을 마치면 돌아올 노드 번호
     * @return 실제로 안내에 들어갔으면 true. 칸이 비어 있으면 false 이며,
     *         이 경우 호출부가 그냥 다음 노드로 넘어가면 된다.
     */
    bool BeginGuide(
        UDataTable* InDataTable,
        FName InRowName,
        int32 InResumeNodeIndex,
        const TArray<FSFStageIntroGuideStep>& InSteps);

    /**
     * 지금 칸을 통과 처리하고 다음으로 넘어간다.
     *
     * Click 방식 칸에서 스포트라이트 위젯이 부른다. 신호를 기다리는 칸에서
     * 불러도 그대로 넘어가므로, 막혔을 때 손으로 밀어 넣는 용도로도 쓴다.
     */
    UFUNCTION(BlueprintCallable, Category = "SF|Tutorial|Guide")
    void CompleteCurrentGuideStep();

    /** 안내를 중단한다. 돌아갈 노드로 가지 않고 그대로 끝낸다 */
    UFUNCTION(BlueprintCallable, Category = "SF|Tutorial|Guide")
    void CancelGuide();

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SF|Tutorial|Guide")
    bool IsGuideActive() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SF|Tutorial|Guide")
    int32 GetCurrentGuideStepIndex() const;

    // ── 목표 달성 신호 (게임플레이 쪽에서 호출) ─────────

    /**
     * 목표 하나가 달성됐음을 알린다.
     *
     * 태그는 계층으로 매칭된다. Tutorial.Build 를 기다리는 중에
     * Tutorial.Build.Barracks 가 오면 만족한 것으로 본다.
     *
     * ── bRecordWhenNotWaiting ──────────────────────────────
     *
     * 목표에는 두 종류가 있고, 둘을 같게 다루면 반드시 사고가 난다.
     *
     *   상태형 (true, 기본) — "병영이 있다", "집이 있다"
     *     튜토리얼이 말하기 전에 이미 지어버린 플레이어를 그 자리에 가둘 수
     *     없으므로, 대기 중이 아니어도 기록해 두고 나중에 통과시킨다.
     *
     *   행동형 (false) — "유닛을 움직였다", "정지 명령을 내렸다"
     *     지금 요구되지 않은 행동은 없던 일로 한다. 기록해 두면 앞 단계에서
     *     이미 유닛을 움직인 플레이어에게 "유닛을 움직여보세요" 단계가
     *     통째로 건너뛰어진다.
     *
     * @return 이 신호로 대기가 풀렸으면 true
     */
    UFUNCTION(BlueprintCallable, Category = "SF|Tutorial", meta = (WorldContext = "InWorldContext", AdvancedDisplay = "bRecordWhenNotWaiting"))
    static bool NotifyObjective(const UObject* InWorldContext, FGameplayTag InObjectiveTag, bool bRecordWhenNotWaiting = true);

    /** 달성 기록을 지운다. 튜토리얼을 처음부터 다시 돌릴 때 부른다 */
    UFUNCTION(BlueprintCallable, Category = "SF|Tutorial")
    void ResetCompletedObjectives();

    // ── 조회 ────────────────────────────────────────────

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SF|Tutorial")
    bool IsWaitingForObjective() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SF|Tutorial")
    FGameplayTag GetActiveObjectiveTag() const;

    /** 대기 중인 목표의 배너 문구. 배너 위젯을 늦게 만들어도 문구를 가져갈 수 있다 */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SF|Tutorial")
    FText GetActiveObjectiveText() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SF|Tutorial")
    bool IsObjectiveCompleted(FGameplayTag InObjectiveTag) const;

    // ── 델리게이트 ──────────────────────────────────────

    UPROPERTY(BlueprintAssignable, Category = "SF|Tutorial")
    FSFTutorialObjectiveStarted OnObjectiveStarted;

    UPROPERTY(BlueprintAssignable, Category = "SF|Tutorial")
    FSFTutorialObjectiveCompleted OnObjectiveCompleted;

    UPROPERTY(BlueprintAssignable, Category = "SF|Tutorial")
    FSFTutorialObjectiveRecorded OnObjectiveRecorded;

    UPROPERTY(BlueprintAssignable, Category = "SF|Tutorial")
    FSFTutorialResumeRequested OnResumeRequested;

    UPROPERTY(BlueprintAssignable, Category = "SF|Tutorial|Guide")
    FSFTutorialGuideStepStarted OnGuideStepStarted;

    UPROPERTY(BlueprintAssignable, Category = "SF|Tutorial|Guide")
    FSFTutorialGuideFinished OnGuideFinished;

    /**
     * 배치 고스트로 쓸 액터 클래스.
     *
     * 반투명 회색 머티리얼은 이 클래스의 블루프린트에서 지정한다 — 어떤
     * 머티리얼이 어울리는지는 아트 쪽 판단이라 C++ 에 박지 않는다.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Tutorial|Guide")
    TSoftClassPtr<ASFTutorialGhostActor> GhostActorClass;

    /** 콘솔 명령 sf.Tutorial.DumpState 가 호출한다 */
    void DumpState() const;

private:
    /** 안내 한 칸을 화면에 올린다. 범위를 벗어나면 안내를 끝낸다 */
    void EnterGuideStep(int32 InStepIndex);

    /** 모든 칸을 마쳤다. 상태를 비우고 튜토리얼로 돌아간다 */
    void FinishGuide();

    /** 안내 상태만 비운다. 고스트와 타이머도 함께 정리한다 */
    void ClearGuideState();

    void SpawnPlacementGhost(const FSFStageIntroGuideStep& InStep);
    void DestroyPlacementGhost();

    UFUNCTION()
    void HandleGuideAutoAdvance();

    /** 지금 기다리는 목표. 유효하지 않으면 대기 중이 아니다 */
    UPROPERTY(Transient)
    FGameplayTag PendingObjectiveTag;

    UPROPERTY(Transient)
    FText PendingBannerText;

    UPROPERTY(Transient)
    TObjectPtr<UDataTable> PendingDataTable = nullptr;

    UPROPERTY(Transient)
    FName PendingRowName = NAME_None;

    /** 대기가 풀리면 돌아갈 노드 번호 */
    int32 PendingResumeNodeIndex = INDEX_NONE;

    /**
     * 지금까지 달성한 목표.
     *
     * 튜토리얼이 그 목표를 말하기 전에 미리 해버린 경우를 위해 남긴다.
     * 대기 등록 시점에 여기 있으면 기다리지 않고 바로 통과시킨다.
     */
    UPROPERTY(Transient)
    FGameplayTagContainer CompletedObjectives;

    // ── 단계별 안내 상태 ────────────────────────────────

    UPROPERTY(Transient)
    TArray<FSFStageIntroGuideStep> GuideSteps;

    /** INDEX_NONE 이면 안내 중이 아니다 */
    int32 GuideStepIndex = INDEX_NONE;

    /** 배치 고스트. 칸이 바뀌거나 안내가 끝나면 파괴한다 */
    UPROPERTY(Transient)
    TObjectPtr<ASFTutorialGhostActor> ActiveGhost = nullptr;

    FTimerHandle GuideAutoAdvanceHandle;

    /**
     * BeginGuide 안에서 EnterGuideStep 이 도는 동안 켜진다.
     *
     * 모든 칸이 이미 달성돼 있으면 BeginGuide 가 끝나기도 전에 안내가
     * 종료된다. 그때 재개 델리게이트를 쏘면 아직 닫지도 않은 튜토리얼
     * 위젯 위에 위젯이 하나 더 열린다 — 목표 대기에서 겪었던 것과 같은
     * 문제다. 이 플래그가 켜져 있으면 브로드캐스트를 미루고, BeginGuide 가
     * false 를 돌려주어 호출부가 직접 다음 노드로 넘어가게 한다.
     */
    bool bEnteringGuide = false;

    bool bGuideEndedWhileEntering = false;
};
