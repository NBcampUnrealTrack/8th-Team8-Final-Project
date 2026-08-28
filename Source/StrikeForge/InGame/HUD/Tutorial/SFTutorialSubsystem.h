#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"
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

    /** 콘솔 명령 sf.Tutorial.DumpState 가 호출한다 */
    void DumpState() const;

private:
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
};
