#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "SFStageIntroData.h"
#include "SFStageIntroWidget.generated.h"

class UButton;
class UDataTable;
class UImage;
class UTextBlock;
class UTexture2D;
class UWidgetSwitcher;

UCLASS(Abstract, Blueprintable)
class STRIKEFORGE_API USFStageIntroWidget : public UCommonActivatableWidget
{
    GENERATED_BODY()

public:
    /** 행에 지정된 StartNodeIndex 부터 시작한다 */
    UFUNCTION(BlueprintCallable, Category = "SF|Stage Intro")
    bool StartStageIntro(UDataTable* InDataTable, FName InRowName);

    /**
     * 지정한 노드부터 시작한다. 목표 대기 후 이어서 열 때 쓴다.
     *
     * USFTutorialSubsystem::OnResumeRequested 가 넘겨주는 세 값을 그대로
     * 여기 넣으면 멈췄던 자리에서 이어진다.
     */
    UFUNCTION(BlueprintCallable, Category = "SF|Stage Intro")
    bool StartStageIntroAt(UDataTable* InDataTable, FName InRowName, int32 InNodeIndex);

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
    void ShowNode(int32 NodeIndex);
    void ShowDialogueNode(const FSFStageIntroNode& Node);
    void ShowChoiceNode(const FSFStageIntroNode& Node);
    void ShowControlGuideNode(const FSFStageIntroNode& Node);
    void ShowWaitForObjectiveNode(const FSFStageIntroNode& Node);
    void UpdateCharacterImages(const FSFStageIntroNode& Node);
    void SetImageTexture(UImage* TargetImage, const TSoftObjectPtr<UTexture2D>& Texture);
    void UpdateGuidePageText();
    void FinishStageIntro();

    const FSFStageIntroNode* GetCurrentNode() const;

    UFUNCTION()
    void HandleYesClicked();

    UFUNCTION()
    void HandleNoClicked();

    UFUNCTION()
    void HandlePreviousGuideClicked();

    UFUNCTION()
    void HandleNextGuideClicked();

protected:
    UPROPERTY(BlueprintReadOnly, Category = "SF|Stage Intro", meta = (BindWidget))
    TObjectPtr<UWidgetSwitcher> StageIntroSwitcher = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "SF|Stage Intro", meta = (BindWidget))
    TObjectPtr<UImage> LeftCharacterImage = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "SF|Stage Intro", meta = (BindWidget))
    TObjectPtr<UImage> RightCharacterImage = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "SF|Stage Intro", meta = (BindWidget))
    TObjectPtr<UTextBlock> SpeakerNameText = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "SF|Stage Intro", meta = (BindWidget))
    TObjectPtr<UTextBlock> DialogueText = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "SF|Stage Intro", meta = (BindWidget))
    TObjectPtr<UImage> NextIndicatorImage = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "SF|Stage Intro", meta = (BindWidget))
    TObjectPtr<UTextBlock> ChoiceQuestionText = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "SF|Stage Intro", meta = (BindWidget))
    TObjectPtr<UButton> YesButton = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "SF|Stage Intro", meta = (BindWidget))
    TObjectPtr<UTextBlock> YesButtonText = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "SF|Stage Intro", meta = (BindWidget))
    TObjectPtr<UButton> NoButton = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "SF|Stage Intro", meta = (BindWidget))
    TObjectPtr<UTextBlock> NoButtonText = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "SF|Stage Intro", meta = (BindWidget))
    TObjectPtr<UTextBlock> GuideTitleText = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "SF|Stage Intro", meta = (BindWidget))
    TObjectPtr<UImage> GuideImage = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "SF|Stage Intro", meta = (BindWidget))
    TObjectPtr<UTextBlock> GuideDescriptionText = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "SF|Stage Intro", meta = (BindWidget))
    TObjectPtr<UTextBlock> GuidePageText = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "SF|Stage Intro", meta = (BindWidget))
    TObjectPtr<UButton> PreviousGuideButton = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "SF|Stage Intro", meta = (BindWidget))
    TObjectPtr<UButton> NextGuideButton = nullptr;

private:
    UPROPERTY(Transient)
    TArray<FSFStageIntroNode> ActiveNodes;

    /**
     * 지금 재생 중인 테이블과 행.
     *
     * 목표 대기에 들어갈 때 "어디로 돌아와야 하는지" 를 서브시스템에 넘겨야
     * 하는데, 노드 배열만으로는 그 출처를 알 수가 없어서 함께 들고 있는다.
     */
    UPROPERTY(Transient)
    TObjectPtr<UDataTable> ActiveDataTable = nullptr;

    FName ActiveRowName = NAME_None;

    int32 CurrentNodeIndex = INDEX_NONE;

    static constexpr int32 DialoguePanelIndex = 0;
    static constexpr int32 ChoicePanelIndex = 1;
    static constexpr int32 ControlGuidePanelIndex = 2;
};