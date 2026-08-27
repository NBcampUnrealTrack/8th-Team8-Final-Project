#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "SFStageIntroData.generated.h"

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
    WaitForObjective UMETA(DisplayName = "Wait For Objective")
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