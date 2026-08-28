#include "SFStageIntroWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "InGame/HUD/Tutorial/SFTutorialSubsystem.h"
#include "Input/Events.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"
#include "Logging/SFLog.h"

void USFStageIntroWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (IsValid(YesButton))
    {
        YesButton->OnClicked.RemoveDynamic(this, &USFStageIntroWidget::HandleYesClicked);
        YesButton->OnClicked.AddUniqueDynamic(this, &USFStageIntroWidget::HandleYesClicked);
    }

    if (IsValid(NoButton))
    {
        NoButton->OnClicked.RemoveDynamic(this, &USFStageIntroWidget::HandleNoClicked);
        NoButton->OnClicked.AddUniqueDynamic(this, &USFStageIntroWidget::HandleNoClicked);
    }

    if (IsValid(PreviousGuideButton))
    {
        PreviousGuideButton->OnClicked.RemoveDynamic(this, &USFStageIntroWidget::HandlePreviousGuideClicked);
        PreviousGuideButton->OnClicked.AddUniqueDynamic(this, &USFStageIntroWidget::HandlePreviousGuideClicked);
    }

    if (IsValid(NextGuideButton))
    {
        NextGuideButton->OnClicked.RemoveDynamic(this, &USFStageIntroWidget::HandleNextGuideClicked);
        NextGuideButton->OnClicked.AddUniqueDynamic(this, &USFStageIntroWidget::HandleNextGuideClicked);
    }
}

void USFStageIntroWidget::NativeDestruct()
{
    if (IsValid(YesButton))
    {
        YesButton->OnClicked.RemoveDynamic(this, &USFStageIntroWidget::HandleYesClicked);
    }

    if (IsValid(NoButton))
    {
        NoButton->OnClicked.RemoveDynamic(this, &USFStageIntroWidget::HandleNoClicked);
    }

    if (IsValid(PreviousGuideButton))
    {
        PreviousGuideButton->OnClicked.RemoveDynamic(this, &USFStageIntroWidget::HandlePreviousGuideClicked);
    }

    if (IsValid(NextGuideButton))
    {
        NextGuideButton->OnClicked.RemoveDynamic(this, &USFStageIntroWidget::HandleNextGuideClicked);
    }

    ActiveNodes.Empty();
    ActiveDataTable = nullptr;
    ActiveRowName = NAME_None;
    CurrentNodeIndex = INDEX_NONE;

    Super::NativeDestruct();
}

bool USFStageIntroWidget::StartStageIntro(UDataTable* InDataTable, FName InRowName)
{
    // INDEX_NONE 을 넘기면 행에 지정된 StartNodeIndex 를 쓴다.
    // 두 함수가 같은 검증을 각자 들고 있으면 반드시 한쪽만 고쳐진다.
    return StartStageIntroAt(InDataTable, InRowName, INDEX_NONE);
}

bool USFStageIntroWidget::StartStageIntroAt(UDataTable* InDataTable, FName InRowName, int32 InNodeIndex)
{
    if (!IsValid(InDataTable) || InRowName.IsNone())
    {
        SF_LOG(Error, "[SFStageIntroWidget] Invalid DataTable or RowName.");
        return false;
    }

    const FSFStageIntroDataRow* IntroRow = InDataTable->FindRow<FSFStageIntroDataRow>(
        InRowName,
        TEXT("USFStageIntroWidget::StartStageIntroAt")
    );

    if (IntroRow == nullptr)
    {
        SF_LOG(Error, "[SFStageIntroWidget] Intro row was not found | Row: %s", *InRowName.ToString());
        return false;
    }

    if (IntroRow->Nodes.IsEmpty())
    {
        SF_LOG(Error, "[SFStageIntroWidget] Intro nodes are empty | Row: %s", *InRowName.ToString());
        return false;
    }

    const int32 StartIndex = InNodeIndex == INDEX_NONE ? IntroRow->StartNodeIndex : InNodeIndex;

    if (!IntroRow->Nodes.IsValidIndex(StartIndex))
    {
        SF_LOG(Error, "[SFStageIntroWidget] Invalid start node index: %d | Row: %s | Node count: %d",
            StartIndex, *InRowName.ToString(), IntroRow->Nodes.Num());
        return false;
    }

    // 목표 대기에 들어갈 때 "어디로 돌아와야 하는지" 를 넘겨야 하므로 함께 남긴다.
    ActiveDataTable = InDataTable;
    ActiveRowName = InRowName;

    ActiveNodes = IntroRow->Nodes;
    ShowNode(StartIndex);

    return true;
}

FReply USFStageIntroWidget::NativeOnMouseButtonDown(
    const FGeometry& InGeometry,
    const FPointerEvent& InMouseEvent
)
{
    const FSFStageIntroNode* CurrentNode = GetCurrentNode();

    if (
        CurrentNode != nullptr &&
        CurrentNode->NodeType == ESFStageIntroNodeType::Dialogue &&
        InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton
        )
    {
        ShowNode(CurrentNode->NextNodeIndex);
        return FReply::Handled();
    }

    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void USFStageIntroWidget::ShowNode(int32 NodeIndex)
{
    if (!ActiveNodes.IsValidIndex(NodeIndex))
    {
        FinishStageIntro();
        return;
    }

    CurrentNodeIndex = NodeIndex;
    const FSFStageIntroNode& Node = ActiveNodes[CurrentNodeIndex];

    switch (Node.NodeType)
    {
    case ESFStageIntroNodeType::Dialogue:
        ShowDialogueNode(Node);
        break;

    case ESFStageIntroNodeType::Choice:
        ShowChoiceNode(Node);
        break;

    case ESFStageIntroNodeType::ControlGuide:
        ShowControlGuideNode(Node);
        break;

    case ESFStageIntroNodeType::WaitForObjective:
        ShowWaitForObjectiveNode(Node);
        break;

    case ESFStageIntroNodeType::End:
    default:
        FinishStageIntro();
        break;
    }
}

void USFStageIntroWidget::ShowDialogueNode(const FSFStageIntroNode& Node)
{
    if (IsValid(StageIntroSwitcher))
    {
        StageIntroSwitcher->SetActiveWidgetIndex(DialoguePanelIndex);
    }

    if (IsValid(SpeakerNameText))
    {
        SpeakerNameText->SetText(Node.TitleText);
    }

    if (IsValid(DialogueText))
    {
        DialogueText->SetText(Node.BodyText);
    }

    if (IsValid(NextIndicatorImage))
    {
        NextIndicatorImage->SetVisibility(ESlateVisibility::HitTestInvisible);
    }

    UpdateCharacterImages(Node);
}

void USFStageIntroWidget::ShowChoiceNode(const FSFStageIntroNode& Node)
{
    if (IsValid(StageIntroSwitcher))
    {
        StageIntroSwitcher->SetActiveWidgetIndex(ChoicePanelIndex);
    }

    if (IsValid(ChoiceQuestionText))
    {
        ChoiceQuestionText->SetText(Node.BodyText);
    }

    if (IsValid(YesButtonText))
    {
        YesButtonText->SetText(
            Node.YesText.IsEmpty()
            ? NSLOCTEXT("SFStageIntro", "DefaultYesText", "예")
            : Node.YesText
        );
    }

    if (IsValid(NoButtonText))
    {
        NoButtonText->SetText(
            Node.NoText.IsEmpty()
            ? NSLOCTEXT("SFStageIntro", "DefaultNoText", "아니오")
            : Node.NoText
        );
    }

    if (
        !Node.LeftCharacterImage.IsNull() ||
        !Node.RightCharacterImage.IsNull() ||
        Node.SpeakerSide != ESFStageIntroSpeakerSide::None
        )
    {
        UpdateCharacterImages(Node);
    }

    if (IsValid(NextIndicatorImage))
    {
        NextIndicatorImage->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void USFStageIntroWidget::ShowControlGuideNode(const FSFStageIntroNode& Node)
{
    if (IsValid(StageIntroSwitcher))
    {
        StageIntroSwitcher->SetActiveWidgetIndex(ControlGuidePanelIndex);
    }

    if (IsValid(GuideTitleText))
    {
        GuideTitleText->SetText(Node.TitleText);
    }

    if (IsValid(GuideDescriptionText))
    {
        GuideDescriptionText->SetText(Node.BodyText);
    }

    SetImageTexture(GuideImage, Node.GuideImage);

    if (IsValid(LeftCharacterImage))
    {
        LeftCharacterImage->SetVisibility(ESlateVisibility::Collapsed);
    }

    if (IsValid(RightCharacterImage))
    {
        RightCharacterImage->SetVisibility(ESlateVisibility::Collapsed);
    }

    if (IsValid(PreviousGuideButton))
    {
        PreviousGuideButton->SetIsEnabled(Node.PreviousNodeIndex != INDEX_NONE);
    }

    if (IsValid(NextGuideButton))
    {
        NextGuideButton->SetIsEnabled(true);
    }

    if (IsValid(NextIndicatorImage))
    {
        NextIndicatorImage->SetVisibility(ESlateVisibility::Collapsed);
    }

    UpdateGuidePageText();
}

// ─────────────────────────────────────────────────────────
// 목표 대기
//
// 이 노드에서는 위젯을 닫는다. 안 닫으면 플레이어가 건물을 지을 수가 없다.
// 닫으면 NativeDestruct 가 ActiveNodes 를 비우므로 진행도가 사라지는데,
// 그래서 돌아올 자리를 USFTutorialSubsystem 에 맡긴다.
// ─────────────────────────────────────────────────────────
void USFStageIntroWidget::ShowWaitForObjectiveNode(const FSFStageIntroNode& Node)
{
    // Node 는 ActiveNodes 안의 원소를 가리키는 참조다. 아래에서 배열을 비우면
    // 그 순간 무효가 되므로, 쓸 값은 먼저 전부 복사해 둔다.
    const FGameplayTag ObjectiveTag = Node.ObjectiveTag;
    const FText BannerText = Node.ObjectiveBannerText;
    const int32 ResumeNodeIndex = Node.NextNodeIndex;

    USFTutorialSubsystem* Tutorial = USFTutorialSubsystem::Get(this);

    if (!IsValid(Tutorial))
    {
        // 여기서 위젯을 닫아버리면 튜토리얼이 그대로 끝난 것처럼 보인다.
        // 대기를 포기하고 다음 노드로 넘어가는 쪽이 낫다.
        SF_LOG(Error, "[SFStageIntroWidget] Tutorial subsystem was not found. The objective wait is skipped.");
        ShowNode(ResumeNodeIndex);
        return;
    }

    // 대기에 들어가지 못하는 경우는 두 가지다 — 태그가 비었거나, 플레이어가
    // 튜토리얼이 시키기 전에 이미 해버렸거나. 둘 다 그냥 다음 노드로 간다.
    //
    // 서브시스템이 여기서 재개 델리게이트를 쏘지 않는 것이 중요하다. 쐈다면
    // 아직 닫지도 않은 이 위젯 위에 위젯이 하나 더 열린다.
    const bool bWaiting = Tutorial->BeginObjectiveWait(
        ActiveDataTable,
        ActiveRowName,
        ResumeNodeIndex,
        ObjectiveTag,
        BannerText
    );

    if (!bWaiting)
    {
        ShowNode(ResumeNodeIndex);
        return;
    }

    SF_LOG(Log, "[SFStageIntroWidget] Suspended for objective | Tag: %s | Resume node: %d",
        *ObjectiveTag.ToString(), ResumeNodeIndex);

    // FinishStageIntro 와 하는 일은 같지만 의미가 다르다.
    // 저쪽은 "튜토리얼이 끝났다", 이쪽은 "잠시 멈춘다" 다.
    ActiveNodes.Empty();
    ActiveDataTable = nullptr;
    ActiveRowName = NAME_None;
    CurrentNodeIndex = INDEX_NONE;

    DeactivateWidget();
}

void USFStageIntroWidget::UpdateCharacterImages(const FSFStageIntroNode& Node)
{
    SetImageTexture(LeftCharacterImage, Node.LeftCharacterImage);
    SetImageTexture(RightCharacterImage, Node.RightCharacterImage);

    if (IsValid(LeftCharacterImage))
    {
        LeftCharacterImage->SetRenderOpacity(
            Node.SpeakerSide == ESFStageIntroSpeakerSide::Right ? 0.45f : 1.f
        );
    }

    if (IsValid(RightCharacterImage))
    {
        RightCharacterImage->SetRenderOpacity(
            Node.SpeakerSide == ESFStageIntroSpeakerSide::Left ? 0.45f : 1.f
        );
    }
}

void USFStageIntroWidget::SetImageTexture(
    UImage* TargetImage,
    const TSoftObjectPtr<UTexture2D>& Texture
)
{
    if (!IsValid(TargetImage))
    {
        return;
    }

    UTexture2D* LoadedTexture = Texture.LoadSynchronous();

    if (!IsValid(LoadedTexture))
    {
        TargetImage->SetBrushFromTexture(nullptr, false);
        TargetImage->SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    TargetImage->SetBrushFromTexture(LoadedTexture, false);
    TargetImage->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void USFStageIntroWidget::UpdateGuidePageText()
{
    if (!IsValid(GuidePageText))
    {
        return;
    }

    int32 TotalGuideCount = 0;
    int32 CurrentGuideNumber = 0;

    for (int32 Index = 0; Index < ActiveNodes.Num(); ++Index)
    {
        if (ActiveNodes[Index].NodeType != ESFStageIntroNodeType::ControlGuide)
        {
            continue;
        }

        ++TotalGuideCount;

        if (Index == CurrentNodeIndex)
        {
            CurrentGuideNumber = TotalGuideCount;
        }
    }

    GuidePageText->SetText(
        FText::Format(
            NSLOCTEXT("SFStageIntro", "GuidePageFormat", "{0} / {1}"),
            FText::AsNumber(CurrentGuideNumber),
            FText::AsNumber(TotalGuideCount)
        )
    );
}

const FSFStageIntroNode* USFStageIntroWidget::GetCurrentNode() const
{
    return ActiveNodes.IsValidIndex(CurrentNodeIndex)
        ? &ActiveNodes[CurrentNodeIndex]
        : nullptr;
}

void USFStageIntroWidget::HandleYesClicked()
{
    const FSFStageIntroNode* CurrentNode = GetCurrentNode();

    if (
        CurrentNode != nullptr &&
        CurrentNode->NodeType == ESFStageIntroNodeType::Choice
        )
    {
        ShowNode(CurrentNode->YesNodeIndex);
    }
}

void USFStageIntroWidget::HandleNoClicked()
{
    const FSFStageIntroNode* CurrentNode = GetCurrentNode();

    if (
        CurrentNode != nullptr &&
        CurrentNode->NodeType == ESFStageIntroNodeType::Choice
        )
    {
        ShowNode(CurrentNode->NoNodeIndex);
    }
}

void USFStageIntroWidget::HandlePreviousGuideClicked()
{
    const FSFStageIntroNode* CurrentNode = GetCurrentNode();

    if (
        CurrentNode != nullptr &&
        CurrentNode->NodeType == ESFStageIntroNodeType::ControlGuide
        )
    {
        ShowNode(CurrentNode->PreviousNodeIndex);
    }
}

void USFStageIntroWidget::HandleNextGuideClicked()
{
    const FSFStageIntroNode* CurrentNode = GetCurrentNode();

    if (
        CurrentNode != nullptr &&
        CurrentNode->NodeType == ESFStageIntroNodeType::ControlGuide
        )
    {
        ShowNode(CurrentNode->NextNodeIndex);
    }
}

void USFStageIntroWidget::FinishStageIntro()
{
    ActiveNodes.Empty();
    ActiveDataTable = nullptr;
    ActiveRowName = NAME_None;
    CurrentNodeIndex = INDEX_NONE;
    DeactivateWidget();
}