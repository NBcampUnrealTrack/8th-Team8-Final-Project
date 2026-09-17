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
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SF_Tutorial_Unit_Produced);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SF_Tutorial_Unit_Produced_Tier1);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SF_Tutorial_Unit_Produced_Tier2);

class ASFTutorialGhostActor;
class USFBuildingDefinition;
class USoundBase;

/** 목표가 달성됐다. 완료 연출이나 후속 처리를 할 때 구독한다 */
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
 * 지금 겨누고 있는 자리가 안내가 허용하지 않는 곳으로 바뀌었다.
 *
 * 배치 미리보기가 매 틱 자리를 묻는데, 그 결과가 바뀔 때만 알린다.
 * 매 틱 알리면 구독하는 쪽이 같은 문구를 초당 60번 다시 세운다.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSFTutorialBuildBlockedChanged, bool, bBlocked);

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
 * ── UI는 왜 여기서 만들지 않는가 ────────────────────────
 *
 * 위젯을 어떻게 띄우는지(CommonUI 스택 푸시인지 AddToViewport 인지)는 HUD 쪽
 * 사정이다. 서브시스템이 그걸 알면 HUD 구조가 바뀔 때마다 여기도 바뀐다.
 * 튜토리얼 진행만 관리하고, 실제 UI 표현은 각 위젯이 담당한다.
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
        FGameplayTag InObjectiveTag);

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

    /**
     * 지금 안내가 이 지점의 건설을 허용하는가.
     *
     * 안내 중이 아니거나, 지금 칸이 자리를 제한하지 않으면 언제나 true 다.
     * 제한하는 칸이면 표시된 반경 안일 때만 true 다.
     *
     * ── 건설 배치 쪽에서 이 함수를 부르는 이유 ─────────────
     *
     * 목표 판정만으로 막으면 플레이어는 엉뚱한 곳에 건물을 지어버린 뒤에야
     * "안 넘어가네" 를 알게 된다. 자원은 이미 나갔고 건물은 서 있는데
     * 튜토리얼은 멈춘 상태가 된다.
     *
     * 배치 단계에서 막으면 애초에 지어지지 않으므로 그런 상태가 없다.
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SF|Tutorial|Guide",
        meta = (WorldContext = "InWorldContext"))
    static bool IsBuildLocationAllowed(const UObject* InWorldContext, const FVector& InLocation);

    /**
     * 배치 고스트를 지운다. 칸은 그대로 둔다.
     *
     * 플레이어가 그 자리에 짓겠다고 클릭한 순간 부른다. 고스트는 "여기에
     * 지으세요" 라는 안내인데, 짓기 시작한 뒤에도 남아 있으면 실제 건물과
     * 겹쳐서 무엇이 진짜인지 알 수 없게 된다.
     *
     * 칸을 넘기지는 않는다 — 건설이 끝났는지는 여전히 목표 신호가 정한다.
     */
    UFUNCTION(BlueprintCallable, Category = "SF|Tutorial|Guide",
        meta = (WorldContext = "InWorldContext"))
    static void HidePlacementGhost(const UObject* InWorldContext);

    // ── 건설 메뉴 ───────────────────────────────────────

    /**
     * 건설 메뉴에서 건물 하나를 골랐음을 알린다.
     *
     * 행 이름으로 태그를 조립한다.
     *
     *     House  ->  Tutorial.Select.Building.House
     *
     * 건물이 늘어나도 코드를 고칠 필요가 없다. 태그만 추가하면 된다.
     * 그런 태그가 없으면 조용히 무시한다 — 튜토리얼이 그 건물을 다루지
     * 않는다는 뜻이므로 오류가 아니다.
     *
     * bRecordWhenNotWaiting 은 false 다. 메뉴를 여닫으며 이것저것 눌러보는
     * 것은 흔한 행동이라, 요구하지 않은 시점의 선택까지 기록하면 나중에
     * 그 건물을 고르라는 단계가 통째로 건너뛰어진다.
     */
    UFUNCTION(BlueprintCallable, Category = "SF|Tutorial|Guide",
        meta = (WorldContext = "InWorldContext"))
    static void NotifyBuildingSelected(const UObject* InWorldContext, FName InBuildingRowName);

    /**
     * 지금 이 건물을 고를 수 있는가.
     *
     * 안내가 특정 건물을 요구하는 중이면 그 건물만 true 다.
     * 안내가 없거나 건물을 지정하지 않은 칸이면 항상 true 를 돌려준다.
     *
     * 건설 메뉴 항목이 클릭됐을 때 이 값이 false 면 진행을 막으면 된다.
     * 강조만 하고 막지 않으면 플레이어가 다른 건물을 골라 튜토리얼과
     * 어긋난 상태로 들어간다.
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SF|Tutorial|Guide",
        meta = (WorldContext = "InWorldContext"))
    static bool IsBuildingSelectionAllowed(const UObject* InWorldContext, FName InBuildingRowName);

    /**
     * 건물 선택 제한을 강제로 푼다.
     *
     * 정상 경로에서는 건물이 실제로 지어질 때 저절로 풀린다. 이 함수는
     * 안내를 강제로 끊었을 때와 콘솔(sf.Tutorial.UnlockBuilding)용이다.
     */
    void UnlockBuildingSelection();

    // ── 스테이지 단위 기능 잠금 ─────────────────────────
    //
    // "1스테이지에서는 업그레이드 금지" 처럼 한 스테이지 전체에 걸리는
    // 규칙이다. 안내 칸이 아니라 행(FSFStageIntroDataRow)에 적는다.
    //
    // 잠그는 쪽과 잠기는 쪽을 태그로만 이어 둔다. 새 기능을 잠글 때
    // 태그만 추가하면 되고, 튜토리얼이 그 기능을 알 필요가 없다.

    /**
     * 스테이지 인트로가 열릴 때 그 행의 잠금 목록으로 갈아끼운다.
     *
     * 더하지 않고 통째로 바꾼다. 스테이지가 바뀌면 앞 스테이지의 잠금은
     * 남아 있으면 안 된다.
     */
    static void SetStageFeatureLocks(const UObject* InWorldContext, const FGameplayTagContainer& InLocks);

    /**
     * 이 기능이 지금 잠겨 있는가.
     *
     * 튜토리얼이 없는 판에서는 항상 false 다. 버튼의 IsEnabled 나
     * Visibility 바인딩에 그대로 물려도 기존 동작이 바뀌지 않는다.
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SF|Tutorial|Lock",
        meta = (WorldContext = "InWorldContext"))
    static bool IsFeatureLocked(const UObject* InWorldContext, FGameplayTag InFeatureTag);

    /**
     * 건물 티어 업그레이드가 잠겨 있는가.
     *
     * IsFeatureLocked 를 Tutorial.Lock.NexusUpgrade 와 함께 부르는 것과 같다.
     * 부르는 쪽이 태그를 알 필요가 없도록 따로 둔다 — UI 코드의 변경 폭을
     * 한 줄로 줄이기 위해서다.
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SF|Tutorial|Lock",
        meta = (WorldContext = "InWorldContext"))
    static bool IsTierUpgradeLocked(const UObject* InWorldContext);

    // ── 성공 효과음 ─────────────────────────────────────

    /**
     * 스테이지 인트로가 열릴 때 그 행의 기본 성공음을 걸어 둔다.
     *
     * 여기서 한 번만 로드한다. 목표를 달성하는 순간에 로드하면 그 프레임이
     * 튀는데, 하필 그때가 플레이어가 화면을 보고 있는 순간이다.
     */
    static void SetStageObjectiveSuccessSound(
        const UObject* InWorldContext, const TSoftObjectPtr<USoundBase>& InSound);

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

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SF|Tutorial")
    bool IsObjectiveCompleted(FGameplayTag InObjectiveTag) const;

    // ── 델리게이트 ──────────────────────────────────────

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

    UPROPERTY(BlueprintAssignable, Category = "SF|Tutorial|Guide")
    FSFTutorialBuildBlockedChanged OnBuildBlockedChanged;

    /**
     * 월드가 내려갈 때 남은 것을 정리한다.
     *
     * 안내 중에 PIE 를 끄면 배치 고스트 액터와 자동 진행 타이머가 살아 있는
     * 채로 월드가 사라진다. 다음 판에서 바로 티가 나지는 않지만, 정리 지점을
     * 한 곳으로 두는 편이 추적하기 쉽다.
     */
    virtual void Deinitialize() override;

    /** 콘솔 명령 sf.Tutorial.DumpState 가 호출한다 */
    void DumpState() const;

private:
    /** 안내 한 칸을 화면에 올린다. 범위를 벗어나면 안내를 끝낸다 */
    void EnterGuideStep(int32 InStepIndex);

    /** 모든 칸을 마쳤다. 상태를 비우고 튜토리얼로 돌아간다 */
    void FinishGuide();

    /** 안내 상태만 비운다. 고스트와 타이머도 함께 정리한다 */
    void ClearGuideState();

    /**
     * 지금 안내 칸이 "표시한 자리에서" 를 요구할 때, 신호가 그 자리에서
     * 왔는지 본다.
     *
     * 요구하지 않는 칸이거나 위치를 알아낼 수 없으면 true 를 돌려준다.
     * 여기서 막아버리면 위치를 못 읽는 신호원 하나 때문에 튜토리얼이
     * 영영 멈춘다 — 조용히 통과시키고 경고만 남기는 편이 낫다.
     */
    bool IsGuideStepLocationSatisfied(const UObject* InWorldContext) const;

    /** 신호를 보낸 쪽의 월드 위치를 찾는다. 액터든 컴포넌트든 받는다 */
    static bool TryGetNotifierLocation(const UObject* InWorldContext, FVector& OutLocation);

    /** 값이 바뀔 때만 알린다 */
    void SetBuildBlocked(bool bInBlocked);

    /** 행 이름을 Tutorial.Select.Building.<행이름> 태그로 바꾼다 */
    static FGameplayTag MakeBuildingSelectTag(FName InBuildingRowName);

    /** 지금 진행 중인 안내 칸. 없으면 nullptr */
    const FSFStageIntroGuideStep* GetCurrentGuideStep() const;

    /**
     * 건물이 실제로 지어졌으면 요구 건물 잠금을 푼다.
     *
     * 고르기만 하고 취소한 경우에는 아무 목표도 기록되지 않으므로 잠금이
     * 그대로 유지된다. 그게 목적이다.
     */
    void ReleaseBuildingLockIfBuilt(FGameplayTag InObjectiveTag);

    /**
     * 목표를 달성한 순간에 성공음을 울린다.
     *
     * 안내 칸이 자기 소리를 지정했으면 그것을, 아니면 스테이지 기본음을 쓴다.
     * 둘 다 없으면 조용히 지나간다 — 소리를 안 넣은 것은 오류가 아니다.
     */
    void PlayObjectiveSuccessSound(const UObject* InWorldContext) const;

    void SpawnPlacementGhost(const FSFStageIntroGuideStep& InStep);
    void DestroyPlacementGhost();

    /**
     * 건물 클래스에서 메시 컴포넌트의 겉모습을 꺼낸다.
     *
     * 건물을 스폰하지 않는다 — 클래스 기본값(CDO)과 컴포넌트 템플릿만
     * 들여다본다. 스폰하면 BeginPlay 가 돌아 전투 레지스트리와 건설 그리드에
     * 등록되어 버린다.
     *
     * OutMesh 는 비어 있을 수 있다. 이 프로젝트의 건물은 메시를 런타임에
     * Definition 에서 주입받으므로 클래스에는 상대 트랜스폼만 남아 있다.
     * 그래도 그 트랜스폼은 꺼내 써야 하므로, 메시 유무와 반환값을 분리한다.
     *
     * @return 스태틱 메시 컴포넌트를 찾았으면 true
     */
    static bool ResolveBuildingMeshTemplate(
        UClass* InBuildingClass,
        UStaticMesh*& OutMesh,
        FTransform& OutRelativeTransform);

    /**
     * 건물 Definition 에서 겉모습을 꺼낸다.
     *
     * 메시는 Definition 의 티어에서, 상대 트랜스폼은 Definition 이 가리키는
     * 건물 클래스에서 가져와 합친다.
     */
    static bool ResolveDefinitionGhostVisual(
        const USFBuildingDefinition* InDefinition,
        int32 InTierIndex,
        UStaticMesh*& OutMesh,
        FTransform& OutRelativeTransform);

    UFUNCTION()
    void HandleGuideAutoAdvance();

    /** 지금 기다리는 목표. 유효하지 않으면 대기 중이 아니다 */
    UPROPERTY(Transient)
    FGameplayTag PendingObjectiveTag;

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

    /**
     * 이 스테이지 동안 잠긴 기능들.
     *
     * 스테이지 인트로가 열릴 때 채워지고 End 노드에서 비워진다.
     * 목표 대기로 창이 닫혀 있는 동안에도 유지되어야 하므로 안내 상태와
     * 함께 정리하지 않는다.
     */
    UPROPERTY(Transient)
    FGameplayTagContainer LockedFeatures;

    /**
     * 이 스테이지의 기본 성공음. 인트로가 열릴 때 한 번 로드해 둔다.
     *
     * 목표 달성은 튜토리얼 창이 닫혀 있는 동안 일어나므로, 위젯이 아니라
     * 서브시스템이 들고 있어야 한다.
     */
    UPROPERTY(Transient)
    TObjectPtr<USoundBase> StageObjectiveSuccessSound = nullptr;

    /** 지금 칸이 따로 지정한 성공음. EnterGuideStep 이 미리 로드한다 */
    UPROPERTY(Transient)
    TObjectPtr<USoundBase> CurrentStepSuccessSound = nullptr;

    // ── 단계별 안내 상태 ────────────────────────────────

    UPROPERTY(Transient)
    TArray<FSFStageIntroGuideStep> GuideSteps;

    /** INDEX_NONE 이면 안내 중이 아니다 */
    int32 GuideStepIndex = INDEX_NONE;

    /**
     * 튜토리얼이 지금 요구하는 건물의 행 이름. 비어 있으면 제한하지 않는다.
     *
     * 안내 칸이 아니라 서브시스템이 들고 있다. 선택 칸이 그 노드의 마지막
     * 칸이면 건물을 고르는 즉시 EnterGuideStep 이 범위를 벗어나 FinishGuide
     * 가 돌고, 안내가 끝나 버린다. 그 상태에서 배치를 취소하면 플레이어는
     * 건설 메뉴 앞으로 돌아오는데, 칸에게 물어보면 요구 건물을 이미
     * 잃어버린 뒤라 모든 건물이 열린다.
     *
     * 그래서 ClearGuideState 에서 비우지 않는다. 푸는 곳은 세 군데뿐이다.
     *   - 건물이 실제로 지어졌을 때 (ReleaseBuildingLockIfBuilt)
     *   - 안내를 강제로 끊었을 때 (CancelGuide)
     *   - 월드가 내려갈 때 (Deinitialize)
     */
    FName LockedBuildingRowName = NAME_None;

    /**
     * 위 잠금을 건 안내 칸의 인덱스. 잠금이 없으면 INDEX_NONE.
     *
     * "이 칸이 건물 선택을 기다리는 칸인가" 를 가르는 데 쓴다. 잠금은 칸을
     * 넘어서 유지되므로, 잠금만 보고 완료 신호를 보내면 배치 칸에서 건물을
     * 다시 골랐을 때 배치가 끝난 것으로 처리된다.
     */
    int32 LockedBuildingStepIndex = INDEX_NONE;

    /** 배치 고스트. 칸이 바뀌거나 안내가 끝나면 파괴한다 */
    UPROPERTY(Transient)
    TObjectPtr<ASFTutorialGhostActor> ActiveGhost = nullptr;

    FTimerHandle GuideAutoAdvanceHandle;

    /** 마지막으로 알린 배치 가능 여부. 값이 바뀔 때만 브로드캐스트한다 */
    bool bLastBuildBlocked = false;

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
