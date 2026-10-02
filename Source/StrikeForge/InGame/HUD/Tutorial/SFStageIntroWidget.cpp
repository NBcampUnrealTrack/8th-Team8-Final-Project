#include "SFStageIntroWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "Engine/Texture2D.h"
#include "Game/GameState/SFGameState.h"
#include "InGame/HUD/Tutorial/SFTutorialSubsystem.h"
#include "InGame/HUD/Tutorial/SFStageIntroPortraitFrame.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
// TObjectPtr<USoundMix/USoundClass> 를 UObject* 로 넘기려면 완전한 타입이 필요하다.
// 전방 선언만으로는 IsValid 와 GameplayStatics 호출에서 C2664 가 난다.
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "TimerManager.h"
#include "Input/Events.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"
#include "Logging/SFLog.h"

TOptional<FUIInputConfig> USFStageIntroWidget::GetDesiredInputConfig() const
{
    // ECommonInputMode::All          : 게임과 UI 가 모두 입력을 받는다.
    // CaptureDuringMouseDown         : 누르는 동안에만 캡처한다. RTS 기본값이다.
    // bHideCursorDuringViewportCapture=false : 드래그 중에도 커서가 보여야 한다.
    //
    // ASFInGamePlayerController::ConfigureMouseInput 이 설정하는 값과 같다.
    // 둘이 어긋나면 위젯이 열리고 닫힐 때마다 조작 방식이 바뀐다.
    return FUIInputConfig(
        ECommonInputMode::All,
        EMouseCaptureMode::CaptureDuringMouseDown,
        false);
}

void USFStageIntroWidget::NativeOnActivated()
{
    Super::NativeOnActivated();

    ApplyMusicDuck(true);
}

void USFStageIntroWidget::NativeOnDeactivated()
{
    // Super 보다 먼저 되돌린다. 부모가 이 위젯을 정리하고 나면
    // 월드 컨텍스트를 얻지 못할 수 있다.
    ApplyMusicDuck(false);

    Super::NativeOnDeactivated();
}

void USFStageIntroWidget::ApplyMusicDuck(bool bInDuck)
{
    // 이미 그 상태면 아무것도 하지 않는다.
    //
    // Push 를 두 번 하면 그만큼 더 눌린다. 창을 여닫을 때마다 배경음이
    // 조금씩 작아져서 나중에는 안 들리게 된다.
    if (bMusicDucked == bInDuck)
    {
        return;
    }

    if (!IsValid(DuckSoundMix))
    {
        return;
    }

    if (bInDuck)
    {
        // SoundClass 를 지정했으면 볼륨을 런타임에 덮어쓴다.
        // 에셋을 열지 않고 DuckVolume 만 바꿔가며 맞출 수 있다.
        if (IsValid(DuckSoundClass))
        {
            UGameplayStatics::SetSoundMixClassOverride(
                this,
                DuckSoundMix,
                DuckSoundClass,
                DuckVolume,
                1.f,                // 피치는 건드리지 않는다
                DuckFadeSeconds,
                true);              // 하위 클래스까지 적용
        }

        UGameplayStatics::PushSoundMixModifier(this, DuckSoundMix);
    }
    else
    {
        UGameplayStatics::PopSoundMixModifier(this, DuckSoundMix);
    }

    bMusicDucked = bInDuck;

    SF_LOG(Log, "[SFStageIntroWidget] 배경음 감쇠 %s | Volume: %.2f",
        bInDuck ? TEXT("ON") : TEXT("OFF"), DuckVolume);
}

void USFStageIntroWidget::NativeOnInitialized()
{
    // Slate 생성 전에 감싸야 자식 순서와 기존 Canvas/Overlay 슬롯 설정이 유지된다.
    LeftPortraitFrame = WrapCharacterImage(LeftCharacterImage);
    RightPortraitFrame = WrapCharacterImage(RightCharacterImage);
    Super::NativeOnInitialized();
    CacheConversationLayout();
}

void USFStageIntroWidget::CacheConversationLayout()
{
    for (UWidget* Widget : {static_cast<UWidget*>(StageIntroSwitcher), static_cast<UWidget*>(NextIndicatorImage),
        static_cast<UWidget*>(SpeakerNameText), static_cast<UWidget*>(DialogueText),
        static_cast<UWidget*>(LeftPortraitFrame), static_cast<UWidget*>(RightPortraitFrame)})
    {
        if (IsValid(Widget))
        {
            if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot))
            {
                OriginalConversationLayouts.Add(CanvasSlot, CanvasSlot->GetLayout());
            }
        }
    }
}

void USFStageIntroWidget::RestoreConversationLayout()
{
    // 매 노드마다 원본에서 계산한다. 대화를 여러 번 열어도 누적 축소/이동하지 않는다.
    for (const auto& Entry : OriginalConversationLayouts)
    {
        if (IsValid(Entry.Key)) Entry.Key->SetLayout(Entry.Value);
    }
}

void USFStageIntroWidget::PositionPortraitFrames(bool bHasFrame)
{
    // 기본은 WBP 배치를 그대로 둔다.
    //
    // 여기서 캔버스 슬롯을 다시 계산하면, 대사창이 스트레치 앵커일 때
    // GetSize() 가 크기가 아니라 우/하 여백을 돌려주므로 계산이 전부 어긋나고
    // 휘장 칸이 0 이나 음수가 되어 휘장과 초상화가 함께 사라진다.
    // 켜고 싶으면 bAutoPositionPortraitFrames 를 켠다.
    if (!bAutoPositionPortraitFrames || !bHasFrame || !IsValid(StageIntroSwitcher)) return;
    const UCanvasPanelSlot* PanelSlot = Cast<UCanvasPanelSlot>(StageIntroSwitcher->Slot);
    if (!PanelSlot) return;

    // Offsets 를 위치·크기로 읽을 수 있는 것은 점 앵커일 때뿐이다.
    const FAnchors PanelAnchors = PanelSlot->GetAnchors();
    if (!PanelAnchors.Minimum.Equals(PanelAnchors.Maximum))
    {
        SF_LOG(Warning, "[SFStageIntroWidget] 대사창이 스트레치 앵커라 휘장 자동 배치를 건너뜁니다.");
        return;
    }

    const FVector2D PanelSize = PanelSlot->GetSize();
    if (PanelSize.X <= 0. || PanelSize.Y <= 0.) return;
    const FVector2D PanelPosition = PanelSlot->GetPosition();
    const FVector2D PanelAlignment = PanelSlot->GetAlignment();
    const UPanelWidget* PanelParent = StageIntroSwitcher->GetParent();
    auto PositionFrame = [&](USFStageIntroPortraitFrame* Frame, bool bLeft)
    {
        UCanvasPanelSlot* CanvasSlot = IsValid(Frame) ? Cast<UCanvasPanelSlot>(Frame->Slot) : nullptr;
        if (!CanvasSlot) return;
        // 같은 캔버스 안에 있을 때만 대사창 좌표를 그대로 쓸 수 있다.
        if (Frame->GetParent() != PanelParent) return;
        CanvasSlot->SetAnchors(PanelAnchors);
        CanvasSlot->SetAutoSize(false);
        CanvasSlot->SetAlignment(FVector2D(0.5f, 1.f));
        CanvasSlot->SetSize(PortraitFrameSize);
        const double X = PanelPosition.X - PanelSize.X * PanelAlignment.X
            + (bLeft ? PortraitInsetFromPanelEdge : PanelSize.X - PortraitInsetFromPanelEdge);
        const double Y = PanelPosition.Y - PanelSize.Y * PanelAlignment.Y + PortraitDialogueOverlap;
        CanvasSlot->SetPosition(FVector2D(X, Y));
    };
    PositionFrame(LeftPortraitFrame, true);
    PositionFrame(RightPortraitFrame, false);
}

USFStageIntroPortraitFrame* USFStageIntroWidget::WrapCharacterImage(UImage* Image)
{
    if (!IsValid(WidgetTree) || !IsValid(Image) || !IsValid(Image->GetParent()))
    {
        return nullptr;
    }

    UPanelWidget* Parent = Image->GetParent();
    UPanelSlot* OriginalSlot = Image->Slot;
    const int32 ChildIndex = Parent->GetChildIndex(Image);
    USFStageIntroPortraitFrame* Frame = WidgetTree->ConstructWidget<USFStageIntroPortraitFrame>();
    Frame->SetVisibility(Image->GetVisibility());
    Image->RemoveFromParent();
    Parent->InsertChildAt(ChildIndex, Frame, OriginalSlot);
    Frame->SetContent(Image);
    return Frame;
}

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

    // 비활성화를 거치지 않고 파괴되는 경로가 있으면 감쇠가 남는다.
    // 그러면 튜토리얼이 끝난 뒤에도 배경음이 계속 작다.
    ApplyMusicDuck(false);

    // 티커를 안 떼면 위젯이 사라진 뒤에도 계속 불린다.
    StopTypewriterTicker();
    StopSpeakerBlendTicker();

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

    // 창을 새로 열 때는 이전 화자 상태를 들고 오지 않는다.
    // 남아 있으면 첫 대사에서 엉뚱한 쪽이 어두운 채로 시작한다.
    LastLeftCharacterImage.Reset();
    LastRightCharacterImage.Reset();

    // 이 스테이지 동안 잠글 기능을 서브시스템에 걸어 둔다.
    //
    // 목표 대기에서 재개될 때 이 함수가 다시 불리는데, 같은 행의 같은 값을
    // 다시 넣는 것이라 무해하다. 오히려 그래야 재개 뒤에도 잠금이 유지된다.
    USFTutorialSubsystem::SetStageTutorialActive(this, true);
    USFTutorialSubsystem::SetStageFeatureLocks(this, IntroRow->LockedFeatures);

    // 목표를 달성하는 순간은 이 위젯이 닫혀 있을 때다.
    // 그래서 소리를 위젯이 들고 있지 않고 서브시스템에 넘겨 둔다.
    USFTutorialSubsystem::SetStageObjectiveSuccessSound(this, IntroRow->ObjectiveSuccessSound);

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

    RestoreConversationLayout();

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

    // 패널과 배경은 WBP의 원래 크기를 유지한다. 대사 길이는 줄바꿈으로 처리한다.
    if (IsValid(DialogueText))
    {
        DialogueText->SetAutoWrapText(true);
        if (UCanvasPanelSlot* TextSlot = Cast<UCanvasPanelSlot>(DialogueText->Slot))
        {
            TextSlot->SetAutoSize(false);
            if (!TextSlot->GetAnchors().IsStretchedHorizontal() && TextSlot->GetSize().X > 0.f)
            {
                DialogueText->SetWrapTextAt(TextSlot->GetSize().X);
            }
        }
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
    else
    {
        // 선택지의 초상화 생략은 기존 인물을 유지하지만, 휘장 None은 반드시 반영한다.
        UpdateCharacterFrames(Node);
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

    if (IsValid(LeftPortraitFrame))
    {
        LeftPortraitFrame->SetVisibility(ESlateVisibility::Collapsed);
    }
    if (IsValid(RightPortraitFrame))
    {
        RightPortraitFrame->SetVisibility(ESlateVisibility::Collapsed);
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
        ObjectiveTag
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
    UpdateCharacterFrames(Node);

    // 그림이 바뀌는 노드는 화면 전환에 가까우므로 보간 없이 즉시 맞춘다.
    // 새 인물이 작은 상태에서 커지면 등장 연출처럼 읽힌다.
    const bool bPortraitChanged =
        LastLeftCharacterImage != Node.LeftCharacterImage ||
        LastRightCharacterImage != Node.RightCharacterImage;

    LastLeftCharacterImage = Node.LeftCharacterImage;
    LastRightCharacterImage = Node.RightCharacterImage;

    ApplySpeakerHighlight(Node.SpeakerSide, bPortraitChanged);
}

void USFStageIntroWidget::UpdateCharacterFrames(const FSFStageIntroNode& Node)
{
    UTexture2D* FrameTexture = Node.PortraitFrameImage.LoadSynchronous();
    auto UpdateFrame = [&](USFStageIntroPortraitFrame* Frame, UImage* Portrait, float Zoom, const FVector2D& Offset)
    {
        if (IsValid(Frame) && IsValid(Portrait))
        {
            Frame->SetFrame(FrameTexture, PortraitFrameSize, PortraitFrameInset,
                DefaultPortraitZoom * Zoom, Offset, PortraitBackdropColor);
            Frame->SetVisibility(Portrait->GetVisibility());
        }
    };
    UpdateFrame(LeftPortraitFrame, LeftCharacterImage, Node.LeftPortraitZoom, Node.LeftPortraitOffset);
    UpdateFrame(RightPortraitFrame, RightCharacterImage, Node.RightPortraitZoom, Node.RightPortraitOffset);
    PositionPortraitFrames(IsValid(FrameTexture));
}

void USFStageIntroWidget::ApplySpeakerHighlight(
    ESFStageIntroSpeakerSide InSpeakerSide, bool bInInstant)
{
    // 끄면 둘 다 원래 크기·색으로 되돌린다.
    if (!bUseSpeakerHighlight)
    {
        StopSpeakerBlendTicker();

        LeftHighlightAlpha = LeftHighlightTarget = 1.f;
        RightHighlightAlpha = RightHighlightTarget = 1.f;

        ApplyHighlightToImage(LeftCharacterImage, 1.f);
        ApplyHighlightToImage(RightCharacterImage, 1.f);
        return;
    }

    // SpeakerSide 가 None 이면 둘 다 말하는 쪽으로 본다.
    //
    // 기본값이 None 이라, 여기서 한쪽을 어둡게 하면 아직 화자를 지정하지
    // 않은 기존 행이 전부 어두워진다. 지정한 행만 반응하는 편이 안전하다.
    LeftHighlightTarget =
        (InSpeakerSide == ESFStageIntroSpeakerSide::Right) ? 0.f : 1.f;
    RightHighlightTarget =
        (InSpeakerSide == ESFStageIntroSpeakerSide::Left) ? 0.f : 1.f;

    if (bInInstant || SpeakerBlendSpeed <= 0.f)
    {
        StopSpeakerBlendTicker();

        LeftHighlightAlpha = LeftHighlightTarget;
        RightHighlightAlpha = RightHighlightTarget;

        ApplyHighlightToImage(LeftCharacterImage, LeftHighlightAlpha);
        ApplyHighlightToImage(RightCharacterImage, RightHighlightAlpha);
        return;
    }

    // 이미 목표에 있으면 티커를 새로 걸지 않는다.
    if (FMath::IsNearlyEqual(LeftHighlightAlpha, LeftHighlightTarget) &&
        FMath::IsNearlyEqual(RightHighlightAlpha, RightHighlightTarget))
    {
        return;
    }

    if (!SpeakerBlendTickerHandle.IsValid())
    {
        SpeakerBlendTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
            FTickerDelegate::CreateUObject(this, &USFStageIntroWidget::HandleSpeakerBlendTicker));
    }
}

void USFStageIntroWidget::ApplyHighlightToImage(UImage* InImage, float InAlpha) const
{
    if (!IsValid(InImage))
    {
        return;
    }

    // 색을 어둡게 한다. 투명도가 아니다 — 알파를 낮추면 배경이 비쳐서
    // "물러났다" 가 아니라 "사라지는 중" 으로 보인다.
    InImage->SetColorAndOpacity(
        FMath::Lerp(ListenerTint, SpeakerTint, InAlpha));

    // 예전 코드가 남긴 값이 있을 수 있으니 되돌려 둔다.
    InImage->SetRenderOpacity(1.f);

    // 초상화 프레임은 텍스처를 직접 그린다. UImage의 RenderScale을 바꾸면
    // "멀어지는" 대신 그림 자체가 작아져 하반신이 잘린 것처럼 보인다.
    // 화자 구분은 명암만으로 하고, 두 인물의 전신 구도는 항상 고정한다.
    InImage->SetRenderTransformPivot(SpeakerScalePivot);
    InImage->SetRenderScale(FVector2D(1.f, 1.f));
}

bool USFStageIntroWidget::HandleSpeakerBlendTicker(float InDeltaTime)
{
    LeftHighlightAlpha =
        FMath::FInterpTo(LeftHighlightAlpha, LeftHighlightTarget, InDeltaTime, SpeakerBlendSpeed);
    RightHighlightAlpha =
        FMath::FInterpTo(RightHighlightAlpha, RightHighlightTarget, InDeltaTime, SpeakerBlendSpeed);

    ApplyHighlightToImage(LeftCharacterImage, LeftHighlightAlpha);
    ApplyHighlightToImage(RightCharacterImage, RightHighlightAlpha);

    const bool bDone =
        FMath::IsNearlyEqual(LeftHighlightAlpha, LeftHighlightTarget, 0.005f) &&
        FMath::IsNearlyEqual(RightHighlightAlpha, RightHighlightTarget, 0.005f);

    if (bDone)
    {
        // 마지막에 정확한 값으로 맞춘다. 보간은 목표에 무한히 가까워질 뿐
        // 정확히 닿지는 않아서, 놔두면 미세하게 어긋난 채로 남는다.
        LeftHighlightAlpha = LeftHighlightTarget;
        RightHighlightAlpha = RightHighlightTarget;

        ApplyHighlightToImage(LeftCharacterImage, LeftHighlightAlpha);
        ApplyHighlightToImage(RightCharacterImage, RightHighlightAlpha);

        SpeakerBlendTickerHandle.Reset();
        return false;
    }

    return true;
}

void USFStageIntroWidget::StopSpeakerBlendTicker()
{
    if (SpeakerBlendTickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(SpeakerBlendTickerHandle);
        SpeakerBlendTickerHandle.Reset();
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
    // 개별 임무 완료와 전체 튜토리얼 완료는 다르다. 마지막 대화 뒤의 명시적인
    // End 노드에 도달했을 때만 완료 신호를 보낸다 (잘못된 인덱스로 닫힐 때 제외).
    const FSFStageIntroNode* EndNode = GetCurrentNode();
    const FGameplayTag CompletionTag = EndNode && EndNode->NodeType == ESFStageIntroNodeType::End
        ? EndNode->ObjectiveTag : FGameplayTag();
    UWorld* const IntroWorld = GetWorld();

    // 튜토리얼이 끝났으니 스테이지 잠금도 푼다.
    //
    // SuspendStageIntro 는 이 함수를 부르지 않는다. 목표 대기로 창이 닫혀
    // 있는 동안에는 잠금이 유지되어야 하기 때문이다.
    USFTutorialSubsystem::SetStageTutorialActive(this, false);
    USFTutorialSubsystem::SetStageFeatureLocks(this, FGameplayTagContainer());
    USFTutorialSubsystem::SetStageObjectiveSuccessSound(this, TSoftObjectPtr<USoundBase>());

    if (IsValid(IntroWorld))
    {
        if (ASFGameState* GameState = IntroWorld->GetGameState<ASFGameState>())
        {
            GameState->ReleaseBuildingRestrictionsAfterTutorial();
        }
    }

    ActiveNodes.Empty();
    ActiveDataTable = nullptr;
    ActiveRowName = NAME_None;
    CurrentNodeIndex = INDEX_NONE;
    DeactivateWidget();

    // HUD가 복구되고 대화로 인한 일시정지가 풀린 다음에 스폰 타이머를 시작한다.
    if (IsValid(IntroWorld) && CompletionTag.IsValid())
    {
        USFTutorialSubsystem::NotifyObjective(IntroWorld, CompletionTag, true);
    }
}
