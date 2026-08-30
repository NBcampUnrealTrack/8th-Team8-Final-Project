#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "SFStageIntroData.generated.h"

class ASFTutorialGhostActor;
class UStaticMesh;
class UTexture2D;

UENUM(BlueprintType)
enum class ESFStageIntroNodeType : uint8
{
    Dialogue UMETA(DisplayName = "Dialogue"),
    Choice UMETA(DisplayName = "Choice"),
    ControlGuide UMETA(DisplayName = "Control Guide"),
    End UMETA(DisplayName = "End"),

    /**
     * 플레이어가 무언가 할 때까지 기다린다.
     *
     * 이 노드에 들어가면 튜토리얼 위젯이 닫히고, ObjectiveTag 로 신호가 올
     * 때까지 플레이어가 자유롭게 조작한다. 신호가 오면 NextNodeIndex 로
     * 돌아온다. 대기 정보는 USFTutorialSubsystem 이 들고 있다.
     *
     * 새 값은 반드시 목록 맨 뒤에 추가할 것. 중간에 끼워 넣으면 그 뒤 값들의
     * 번호가 밀려서, 이미 만들어둔 DT 의 End 노드가 다른 타입으로 읽힌다.
     */
    WaitForObjective UMETA(DisplayName = "Wait For Objective"),

    /**
     * 화면을 짚어 가며 조작을 시키는 단계별 안내.
     *
     * WaitForObjective 와 동작이 같다 — 위젯을 닫고, 플레이어가 직접 조작하고,
     * 다 끝나면 NextNodeIndex 로 돌아온다. 다른 점은 기다리는 대상이 하나가
     * 아니라 GuideSteps 배열이라는 것뿐이다.
     *
     * "여기를 클릭 -> 드래그 -> 우클릭" 처럼 여러 조작을 순서대로 시킬 때 쓴다.
     * 한 단계짜리 안내라면 WaitForObjective 로도 되지만, 스포트라이트를 띄우고
     * 싶다면 이쪽을 쓴다.
     */
    InteractiveGuide UMETA(DisplayName = "Interactive Guide")
};

/** 스포트라이트가 무엇을 기준으로 자리를 잡는가 */
UENUM(BlueprintType)
enum class ESFGuideTargetKind : uint8
{
    /** 화면 비율 좌표. 0~1 이며 (0,0) 이 좌상단이다 */
    ScreenRegion UMETA(DisplayName = "Screen Region"),

    /**
     * 월드 좌표를 화면에 투영한다.
     *
     * 카메라가 움직이면 스포트라이트도 따라간다. 대상이 화면 밖으로 나가면
     * 가장자리에 화살표만 남는다.
     */
    WorldLocation UMETA(DisplayName = "World Location")
};

/** 이 단계를 무엇으로 통과 처리하는가 */
UENUM(BlueprintType)
enum class ESFGuideCompletion : uint8
{
    /**
     * CompletionTag 신호를 기다린다.
     *
     * 신호는 USFTutorialSubsystem::NotifyObjective 로 온다 — 건물 건설이든
     * 유닛 이동이든 이미 쓰고 있는 그 경로 그대로다.
     */
    ObjectiveTag UMETA(DisplayName = "Objective Tag"),

    /**
     * 화면 아무 곳이나 클릭하면 넘어간다.
     *
     * 조작을 시키는 게 아니라 "여기를 보세요" 라고 짚어주기만 할 때 쓴다.
     */
    Click UMETA(DisplayName = "Click Anywhere"),

    /** AutoAdvanceSeconds 가 지나면 저절로 넘어간다 */
    Timer UMETA(DisplayName = "Timer")
};

/**
 * 단계별 안내 한 칸.
 *
 * 한 칸이 화면에 스포트라이트 하나와 문구 한 줄을 띄우고, 통과 조건이
 * 만족되면 다음 칸으로 넘어간다.
 */
USTRUCT(BlueprintType)
struct STRIKEFORGE_API FSFStageIntroGuideStep
{
    GENERATED_BODY()

    /** "여기를 클릭하세요" 처럼 지금 무엇을 해야 하는지 한 줄 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guide", meta = (MultiLine = true))
    FText InstructionText;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guide")
    ESFGuideTargetKind TargetKind = ESFGuideTargetKind::ScreenRegion;

    /**
     * 화면 비율 좌표(0~1). TargetKind 가 ScreenRegion 일 때 쓴다.
     *
     * 해상도가 달라져도 같은 자리를 가리키도록 픽셀이 아니라 비율로 둔다.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guide|Screen",
        meta = (EditCondition = "TargetKind == ESFGuideTargetKind::ScreenRegion", EditConditionHides))
    FVector2D ScreenAnchor = FVector2D(0.5f, 0.5f);

    /**
     * 구멍 반지름. 화면 짧은 변에 대한 비율이다.
     *
     * 세로 기준으로 재므로 화면이 넓어져도 크기가 그대로다.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guide|Screen",
        meta = (EditCondition = "TargetKind == ESFGuideTargetKind::ScreenRegion", EditConditionHides,
                ClampMin = "0.01", ClampMax = "1.0"))
    float ScreenRadius = 0.12f;

    /** TargetKind 가 WorldLocation 일 때 가리킬 월드 좌표 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guide|World",
        meta = (EditCondition = "TargetKind == ESFGuideTargetKind::WorldLocation", EditConditionHides,
                MakeEditWidget = true))
    FVector WorldLocation = FVector::ZeroVector;

    /** 월드 기준 반지름(cm). 화면 반지름은 카메라 거리에 따라 자동 계산된다 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guide|World",
        meta = (EditCondition = "TargetKind == ESFGuideTargetKind::WorldLocation", EditConditionHides,
                ClampMin = "10.0"))
    float WorldRadius = 300.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guide|Completion")
    ESFGuideCompletion Completion = ESFGuideCompletion::Click;

    /**
     * 기다릴 신호. Completion 이 ObjectiveTag 일 때만 쓴다.
     *
     * WaitForObjective 와 같은 규칙으로 계층 매칭된다.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guide|Completion",
        meta = (EditCondition = "Completion == ESFGuideCompletion::ObjectiveTag", EditConditionHides))
    FGameplayTag CompletionTag;

    /**
     * 신호를 기다리는 단계라도 이 시간이 지나면 넘어간다. 0 이면 무한 대기.
     *
     * 신호 배선을 빠뜨렸을 때 튜토리얼이 그 자리에서 영영 멈추는 것을 막는
     * 안전장치다. Completion 이 Timer 면 이 값이 곧 대기 시간이다.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guide|Completion", meta = (ClampMin = "0.0"))
    float AutoAdvanceSeconds = 0.f;

    /**
     * 화면을 어둡게 덮을지.
     *
     * 끄면 스포트라이트 테두리와 문구만 그린다. 플레이어가 전체 상황을 보면서
     * 조작해야 하는 단계(드래그 선택 등)에서는 끄는 편이 낫다.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guide|Look")
    bool bDimBackground = true;

    /**
     * 회색 배치 고스트를 띄울지. TargetKind 가 WorldLocation 일 때만 뜬다.
     *
     * "여기에 지으세요" 를 말로 하는 것보다 자리에 반투명 모형을 세워두는
     * 편이 훨씬 빠르다. 플레이어가 그 자리에 실제로 지으면 사라진다.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guide|Ghost")
    bool bShowPlacementGhost = false;

    /**
     * 이 건물처럼 보이게 한다. 지정하면 아래 GhostMesh 는 무시된다.
     *
     * ── 왜 메시가 아니라 건물 클래스인가 ────────────────
     *
     * 건물 BP 안의 메시 컴포넌트는 대개 자기 상대 트랜스폼을 갖고 있다.
     * 90도 돌려 붙였거나, 0.8배로 줄였거나, 바닥에 맞추려고 Z 를 내렸거나.
     * 메시만 꺼내 세우면 그 보정이 전부 빠져서 실제 건물과 크기도 방향도
     * 어긋난다.
     *
     * 여기에 건물 BP 를 넣으면 메시와 상대 트랜스폼을 함께 읽어온다.
     * 건물 쪽에서 크기를 바꿔도 고스트가 알아서 따라간다.
     *
     * 건물을 스폰하지는 않는다 — 클래스 기본값만 들여다본다.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guide|Ghost",
        meta = (EditCondition = "bShowPlacementGhost", EditConditionHides))
    TSoftClassPtr<AActor> GhostBuildingClass;

    /**
     * 고스트로 세울 메시. GhostBuildingClass 를 비워뒀을 때만 쓴다.
     *
     * 건물이 아닌 것(자원 노드, 표식 등)을 가리킬 때 쓴다.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guide|Ghost",
        meta = (EditCondition = "bShowPlacementGhost", EditConditionHides))
    TSoftObjectPtr<UStaticMesh> GhostMesh;

    /**
     * 고스트로 쓸 액터 클래스. 반투명 회색 머티리얼이 여기에 들어 있다.
     *
     * 비워두면 C++ 기본 클래스로 스폰되는데, 그러면 메시 원본 머티리얼을
     * 그대로 써서 실제 건물과 구분이 안 된다. BP_TutorialGhost 같은
     * 블루프린트를 지정하는 편이 낫다.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guide|Ghost",
        meta = (EditCondition = "bShowPlacementGhost", EditConditionHides))
    TSoftClassPtr<ASFTutorialGhostActor> GhostActorClass;

    /**
     * 액터 자체의 회전. 건물 BP 안의 메시 회전과는 별개로 더해진다.
     *
     * 건물 클래스를 지정했다면 대개 0 으로 두면 된다.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guide|Ghost",
        meta = (EditCondition = "bShowPlacementGhost", EditConditionHides))
    FRotator GhostRotation = FRotator::ZeroRotator;

    /** 액터 자체의 크기. 건물 클래스를 지정했다면 1 로 두면 된다 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guide|Ghost",
        meta = (EditCondition = "bShowPlacementGhost", EditConditionHides))
    FVector GhostScale = FVector::OneVector;
};

UENUM(BlueprintType)
enum class ESFStageIntroSpeakerSide : uint8
{
    None UMETA(DisplayName = "None"),
    Left UMETA(DisplayName = "Left"),
    Right UMETA(DisplayName = "Right")
};

USTRUCT(BlueprintType)
struct STRIKEFORGE_API FSFStageIntroNode
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SF|Stage Intro")
    ESFStageIntroNodeType NodeType = ESFStageIntroNodeType::Dialogue;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SF|Stage Intro")
    FText TitleText;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SF|Stage Intro", meta = (MultiLine = true))
    FText BodyText;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SF|Stage Intro|Character")
    TSoftObjectPtr<UTexture2D> LeftCharacterImage;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SF|Stage Intro|Character")
    TSoftObjectPtr<UTexture2D> RightCharacterImage;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SF|Stage Intro|Character")
    ESFStageIntroSpeakerSide SpeakerSide = ESFStageIntroSpeakerSide::None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SF|Stage Intro|Guide")
    TSoftObjectPtr<UTexture2D> GuideImage;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SF|Stage Intro|Choice")
    FText YesText;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SF|Stage Intro|Choice")
    FText NoText;

    /**
     * 기다릴 행동. NodeType 이 WaitForObjective 일 때만 쓰인다.
     *
     * 계층으로 매칭된다. Tutorial.Build 로 적어두면 Tutorial.Build.Barracks
     * 든 Tutorial.Build.Factory 든 아무거나 하나만 지어도 통과한다.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SF|Stage Intro|Objective")
    FGameplayTag ObjectiveTag;

    /**
     * 대기 중 화면에 남길 목표 문구. 비우면 배너를 띄우지 않는다.
     *
     * 대사 노드에서 이미 설명했더라도 한 줄은 남기는 편이 낫다. 플레이어가
     * 조작하다가 뭘 하던 중이었는지 잊으면 다시 볼 방법이 없다.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SF|Stage Intro|Objective", meta = (MultiLine = true))
    FText ObjectiveBannerText;

    /**
     * 단계별 안내. NodeType 이 InteractiveGuide 일 때만 쓰인다.
     *
     * 위에서부터 순서대로 진행하고, 마지막 칸이 끝나면 NextNodeIndex 로
     * 돌아온다. 비어 있으면 아무것도 하지 않고 곧바로 다음 노드로 간다.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SF|Stage Intro|Guide Steps")
    TArray<FSFStageIntroGuideStep> GuideSteps;

    /**
     * 이 노드의 타자 속도(초당 글자 수). 0 이면 위젯 기본값을 쓴다.
     *
     * 긴 설명은 빠르게, 결정적인 한 줄은 느리게 — 연출 목적의 값이므로
     * 노드마다 따로 둔다.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SF|Stage Intro|Typewriter", meta = (ClampMin = "0.0"))
    float TypeSpeedOverride = 0.f;

    /** 이 노드만 타자 효과 없이 즉시 전부 보여준다 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SF|Stage Intro|Typewriter")
    bool bSkipTypewriter = false;

    /** WaitForObjective 노드에서는 "목표를 달성하면 돌아올 자리" 가 된다 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SF|Stage Intro|Flow")
    int32 NextNodeIndex = INDEX_NONE;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SF|Stage Intro|Flow")
    int32 PreviousNodeIndex = INDEX_NONE;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SF|Stage Intro|Flow")
    int32 YesNodeIndex = INDEX_NONE;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SF|Stage Intro|Flow")
    int32 NoNodeIndex = INDEX_NONE;
};

USTRUCT(BlueprintType)
struct STRIKEFORGE_API FSFStageIntroDataRow : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SF|Stage Intro", meta = (ClampMin = "0"))
    int32 StartNodeIndex = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SF|Stage Intro")
    TArray<FSFStageIntroNode> Nodes;
};