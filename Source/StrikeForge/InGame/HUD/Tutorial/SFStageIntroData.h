#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "SFStageIntroData.generated.h"

class UTexture2D;

UENUM(BlueprintType)
enum class ESFStageIntroNodeType : uint8
{
    Dialogue UMETA(DisplayName = "Dialogue"),
    Choice UMETA(DisplayName = "Choice"),
    ControlGuide UMETA(DisplayName = "Control Guide"),
    End UMETA(DisplayName = "End")
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