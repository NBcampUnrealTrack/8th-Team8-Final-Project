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
    UFUNCTION(BlueprintCallable, Category = "SF|Stage Intro")
    bool StartStageIntro(UDataTable* InDataTable, FName InRowName);

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
    void ShowNode(int32 NodeIndex);
    void ShowDialogueNode(const FSFStageIntroNode& Node);
    void ShowChoiceNode(const FSFStageIntroNode& Node);
    void ShowControlGuideNode(const FSFStageIntroNode& Node);
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

    int32 CurrentNodeIndex = INDEX_NONE;

    static constexpr int32 DialoguePanelIndex = 0;
    static constexpr int32 ChoicePanelIndex = 1;
    static constexpr int32 ControlGuidePanelIndex = 2;
};