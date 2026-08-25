#include "SFStageIntroWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
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
    CurrentNodeIndex = INDEX_NONE;

    Super::NativeDestruct();
}

bool USFStageIntroWidget::StartStageIntro(UDataTable* InDataTable, FName InRowName)
{
    if (!IsValid(InDataTable) || InRowName.IsNone())
    {
        SF_LOG(Error, "[SFStageIntroWidget] Invalid DataTable or RowName.");
        return false;
    }

    const FSFStageIntroDataRow* IntroRow = InDataTable->FindRow<FSFStageIntroDataRow>(
        InRowName,
        TEXT("USFStageIntroWidget::StartStageIntro")
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

    if (!IntroRow->Nodes.IsValidIndex(IntroRow->StartNodeIndex))
    {
        SF_LOG(Error, "[SFStageIntroWidget] Invalid StartNodeIndex: %d", IntroRow->StartNodeIndex);
        return false;
    }

    ActiveNodes = IntroRow->Nodes;
    ShowNode(IntroRow->StartNodeIndex);

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
    CurrentNodeIndex = INDEX_NONE;
    DeactivateWidget();
}