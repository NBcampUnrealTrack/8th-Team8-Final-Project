#include "SFStageIntroWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "Engine/Texture2D.h"
#include "InGame/HUD/Tutorial/SFTutorialSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
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

    // 티커를 안 떼면 위젯이 사라진 뒤에도 계속 불린다.
    StopTypewriterTicker();

    TypingTarget = nullptr;
    FullTypingText.Reset();
    RevealedCharCount = 0;

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
        // 타자 중에는 다음으로 넘기지 않고 그 줄을 마저 채운다.
        //
        // 이게 없으면 빨리 읽는 플레이어가 클릭 한 번으로 문장을 통째로
        // 건너뛰게 된다. 두 번 클릭해야 넘어가는 것이 이 연출의 표준이다.
        if (IsTypewriterRunning())
        {
            CompleteTypewriter();
            return FReply::Handled();
        }

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

    // 이전 노드의 타자가 돌고 있으면 먼저 끈다.
    // 안 끄면 새 노드의 문구에 이전 문장이 이어서 찍힌다.
    StopTypewriterTicker();

    TypingTarget = nullptr;
    FullTypingText.Reset();
    RevealedCharCount = 0;

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

    case ESFStageIntroNodeType::InteractiveGuide:
        ShowInteractiveGuideNode(Node);
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

    // 선택지·조작 설명 노드가 이 이미지를 Collapsed 로 만들어 두므로
    // 대사 노드로 돌아올 때마다 되살려야 한다.
    //
    // 이 위젯에 "다음" 화살표가 아니라 대사창 배경 전체가 들어 있는 구성도
    // 있어서, 여기서 안 켜면 액자가 영영 안 돌아온다.
    if (IsValid(NextIndicatorImage))
    {
        NextIndicatorImage->SetVisibility(ESlateVisibility::HitTestInvisible);
    }

    BeginTypewriter(DialogueText, Node.BodyText, Node);

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

    BeginTypewriter(GuideDescriptionText, Node.BodyText, Node);

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

    SuspendStageIntro();
}

// ─────────────────────────────────────────────────────────
// 단계별 안내
//
// 목표 대기와 같은 구조다. 위젯을 닫고, 서브시스템이 칸을 하나씩 진행하고,
// 다 끝나면 OnResumeRequested 로 돌아온다. 다른 점은 기다리는 대상이
// 하나가 아니라 여러 개라는 것뿐이다.
// ─────────────────────────────────────────────────────────
void USFStageIntroWidget::ShowInteractiveGuideNode(const FSFStageIntroNode& Node)
{
    // Node 는 ActiveNodes 안의 원소를 가리키는 참조다. 아래에서 배열을
    // 비우면 무효가 되므로 쓸 값을 먼저 전부 복사한다.
    const TArray<FSFStageIntroGuideStep> Steps = Node.GuideSteps;
    const int32 ResumeNodeIndex = Node.NextNodeIndex;

    USFTutorialSubsystem* Tutorial = USFTutorialSubsystem::Get(this);

    if (!IsValid(Tutorial))
    {
        SF_LOG(Error, "[SFStageIntroWidget] Tutorial subsystem was not found. The guide is skipped.");
        ShowNode(ResumeNodeIndex);
        return;
    }

    // 칸이 비었거나 이미 전부 해버린 안내면 false 가 온다.
    // 이때 서브시스템은 재개 델리게이트를 쏘지 않으므로 여기서 직접 넘어간다.
    const bool bStarted = Tutorial->BeginGuide(
        ActiveDataTable,
        ActiveRowName,
        ResumeNodeIndex,
        Steps
    );

    if (!bStarted)
    {
        ShowNode(ResumeNodeIndex);
        return;
    }

    SF_LOG(Log, "[SFStageIntroWidget] Suspended for a guide | Steps: %d | Resume node: %d",
        Steps.Num(), ResumeNodeIndex);

    SuspendStageIntro();
}

// FinishStageIntro 와 하는 일은 같지만 의미가 다르다.
// 저쪽은 "튜토리얼이 끝났다", 이쪽은 "잠시 멈춘다" 다.
// 로그를 쫓을 때 이 구분이 없으면 어디서 끝난 건지 알 수가 없다.
void USFStageIntroWidget::SuspendStageIntro()
{
    StopTypewriterTicker();

    TypingTarget = nullptr;
    FullTypingText.Reset();
    RevealedCharCount = 0;

    ActiveNodes.Empty();
    ActiveDataTable = nullptr;
    ActiveRowName = NAME_None;
    CurrentNodeIndex = INDEX_NONE;

    DeactivateWidget();
}

// ─────────────────────────────────────────────────────────
// 타자 효과
//
// 틱이 아니라 타이머로 돈다.
//
// UUserWidget 의 TickFrequency 기본값(Auto)은 블루프린트 Tick 이 없으면
// 틱을 아예 돌리지 않는다. C++ 에서 NativeTick 을 오버라이드해도 마찬가지라,
// 틱에 기대어 만들면 "어떤 위젯에서는 되고 어떤 위젯에서는 안 되는" 상태가
// 된다. 타이머는 그런 조건이 없고 프레임 속도와도 무관하다.
// ─────────────────────────────────────────────────────────
void USFStageIntroWidget::BeginTypewriter(
    UTextBlock* InTarget,
    const FText& InText,
    const FSFStageIntroNode& InNode
)
{
    if (!IsValid(InTarget))
    {
        return;
    }

    StopTypewriterTicker();

    TypingTarget = InTarget;
    FullTypingText = InText.ToString();
    RevealedCharCount = 0;
    TypeAccumulator = 0.f;

    const float Speed = InNode.TypeSpeedOverride > 0.f ? InNode.TypeSpeedOverride : CharactersPerSecond;

    const bool bInstant =
        !bUseTypewriter ||
        InNode.bSkipTypewriter ||
        Speed <= 0.f ||
        FullTypingText.IsEmpty();

    if (bInstant)
    {
        CompleteTypewriter();
        return;
    }

    ActiveTypeInterval = 1.f / Speed;
    NextCharacterDelay = 0.f;

    // 첫 글자는 기다리지 않고 바로 내보낸다.
    //
    // 빈 문자열로 한 프레임이라도 두면, 대사창 배경이 글자 크기를 따라가는
    // 레이아웃에서는 창이 통째로 사라졌다가 나타난다.
    RevealNextCharacter();

    TypewriterTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateUObject(this, &USFStageIntroWidget::HandleTypewriterTicker));

    // 다 읽기도 전에 "다음" 표시가 깜빡이면 클릭을 재촉하는 꼴이지만,
    // 기본값은 꺼짐이다 — 이유는 헤더 주석에 적어뒀다.
    if (bHideNextIndicatorWhileTyping && IsValid(NextIndicatorImage))
    {
        NextIndicatorImage->SetVisibility(ESlateVisibility::Hidden);
    }
}

// 한 글자(대리 문자쌍이면 두 칸)를 드러내고 다음 간격을 정한다.
void USFStageIntroWidget::RevealNextCharacter()
{
    if (!IsValid(TypingTarget) || RevealedCharCount >= FullTypingText.Len())
    {
        return;
    }

    // 이모지처럼 두 칸을 차지하는 문자는 반쪽만 잘라내면 깨져서 나온다.
    const TCHAR Current = FullTypingText[RevealedCharCount];
    const bool bSurrogatePair =
        Current >= 0xD800 && Current <= 0xDBFF && RevealedCharCount + 1 < FullTypingText.Len();

    RevealedCharCount += bSurrogatePair ? 2 : 1;

    TypingTarget->SetText(FText::FromString(FullTypingText.Left(RevealedCharCount)));

    if (IsValid(TypeSound) && SoundEveryNCharacters > 0 && RevealedCharCount % SoundEveryNCharacters == 0)
    {
        UGameplayStatics::PlaySound2D(this, TypeSound);
    }

    // 문장부호 뒤에서 살짝 쉰다.
    // 균일한 속도로 흘리면 사람이 읽어주는 느낌이 나지 않는다.
    const bool bPunctuation =
        Current == TEXT('.') || Current == TEXT(',') ||
        Current == TEXT('!') || Current == TEXT('?') ||
        Current == TEXT('\n');

    NextCharacterDelay = ActiveTypeInterval +
        (bPunctuation ? FMath::Max(PunctuationPauseSeconds, 0.f) : 0.f);
}

bool USFStageIntroWidget::HandleTypewriterTicker(float InDeltaTime)
{
    if (!IsValid(TypingTarget))
    {
        TypewriterTickerHandle.Reset();
        return false;
    }

    TypeAccumulator += InDeltaTime;

    // 한 프레임이 길어졌다면 그동안 나왔어야 할 글자를 모두 내보낸다.
    // 프레임 하나에 한 글자씩만 내보내면 프레임이 떨어질 때 문장이 늘어진다.
    while (TypeAccumulator >= NextCharacterDelay && RevealedCharCount < FullTypingText.Len())
    {
        TypeAccumulator -= NextCharacterDelay;
        RevealNextCharacter();
    }

    if (RevealedCharCount >= FullTypingText.Len())
    {
        CompleteTypewriter();
        return false;
    }

    return true;
}

void USFStageIntroWidget::StopTypewriterTicker()
{
    if (TypewriterTickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(TypewriterTickerHandle);
        TypewriterTickerHandle.Reset();
    }

    TypeAccumulator = 0.f;
}

void USFStageIntroWidget::CompleteTypewriter()
{
    StopTypewriterTicker();

    if (IsValid(TypingTarget))
    {
        TypingTarget->SetText(FText::FromString(FullTypingText));
    }

    RevealedCharCount = FullTypingText.Len();
    TypingTarget = nullptr;

    // 대사 노드에서만 "다음" 표시를 켠다.
    // 선택지나 조작 설명은 각자 켜고 끄는 규칙이 따로 있다.
    //
    // 숨긴 적이 없으면 켜지도 않는다. 원래 보이던 것을 건드리지 않는 편이
    // 안전하다 — 이 위젯이 무엇인지는 위젯을 만든 쪽이 정한다.
    const FSFStageIntroNode* CurrentNode = GetCurrentNode();

    if (
        bHideNextIndicatorWhileTyping &&
        IsValid(NextIndicatorImage) &&
        CurrentNode != nullptr &&
        CurrentNode->NodeType == ESFStageIntroNodeType::Dialogue
        )
    {
        NextIndicatorImage->SetVisibility(ESlateVisibility::HitTestInvisible);
    }
}

bool USFStageIntroWidget::IsTypewriterRunning() const
{
    return IsValid(TypingTarget) && RevealedCharCount < FullTypingText.Len();
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