#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
// FUIInputConfig / ECommonInputMode 는 CommonActivatableWidget.h 가 끌어온다.
// 예전에 여기 있던 "Input/UIActionRouterTypes.h" 는 CommonUI 의 Private 헤더라
// 모듈 밖에서 열 수 없다 (C1083). 공개 위치는 "Input/UIActionBindingHandle.h" 인데,
// 엔진 버전에 따라 경로가 바뀐 적이 있어 부모 헤더에 맡긴다.
#include "Containers/Ticker.h"
#include "Components/CanvasPanelSlot.h"
#include "SFStageIntroData.h"
#include "SFStageIntroWidget.generated.h"

class UButton;
class UDataTable;
class UImage;
class UTextBlock;
class UTexture2D;
class USoundBase;
class USoundClass;
class USoundMix;
class UWidgetSwitcher;
class USFStageIntroPortraitFrame;

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
    /**
     * CommonUI 는 활성 위젯이 요청한 입력 설정을 뷰포트에 적용한다.
     * 아무도 요청하지 않으면 기본값(FUIInputConfig 기본 생성자)으로 되돌리는데,
     * 그 기본값이 CapturePermanently + 캡처 중 커서 숨김이라 RTS 조작이 막힌다.
     * 그래서 이 위젯이 쓸 설정을 명시한다.
     */
    virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

    virtual void NativeOnInitialized() override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    /**
     * 튜토리얼 창이 뜨는 동안 배경음을 줄인다.
     *
     * 창을 열고 닫는 것은 CommonUI 의 활성/비활성이므로 여기에 건다.
     * NativeConstruct 에 걸면 목표 대기로 창을 닫았다 여는 사이에
     * 원래 볼륨으로 돌아오지 않는다.
     */
    virtual void NativeOnActivated() override;
    virtual void NativeOnDeactivated() override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
    /**
     * 배경음 감쇠를 켜고 끈다.
     *
     * SoundMix 를 밀어 넣는 방식이라 재생 중인 소리를 끊지 않는다.
     * Push 와 Pop 은 반드시 짝이 맞아야 한다 — 한 번 더 밀면 그만큼
     * 더 눌려서 창을 여닫을수록 배경음이 점점 작아진다.
     */
    void ApplyMusicDuck(bool bInDuck);

    /** Push 와 Pop 의 짝을 맞추기 위한 현재 상태 */
    bool bMusicDucked = false;

    void ShowNode(int32 NodeIndex);
    void ShowDialogueNode(const FSFStageIntroNode& Node);
    void ShowChoiceNode(const FSFStageIntroNode& Node);
    void ShowControlGuideNode(const FSFStageIntroNode& Node);
    void ShowWaitForObjectiveNode(const FSFStageIntroNode& Node);
    void ShowInteractiveGuideNode(const FSFStageIntroNode& Node);
    void UpdateCharacterImages(const FSFStageIntroNode& Node);
    USFStageIntroPortraitFrame* WrapCharacterImage(UImage* Image);
    void UpdateCharacterFrames(const FSFStageIntroNode& Node);
    void CacheConversationLayout();
    void RestoreConversationLayout();
    void CompactDialogueLayout(const FText& FullText);
    void PositionPortraitFrames(bool bHasFrame);
    void SetImageTexture(UImage* TargetImage, const TSoftObjectPtr<UTexture2D>& Texture);
    void UpdateGuidePageText();
    void FinishStageIntro();

    /** 대기·안내로 넘어갈 때 진행 상태를 비우고 위젯을 닫는다 */
    void SuspendStageIntro();

    // ── 타자 효과 ───────────────────────────────────────

    /**
     * 글자를 하나씩 드러내기 시작한다.
     *
     * 효과가 꺼져 있거나 노드가 건너뛰기로 표시돼 있으면 전문을 즉시 넣는다.
     */
    void BeginTypewriter(UTextBlock* InTarget, const FText& InText, const FSFStageIntroNode& InNode);

    /** 한 글자(대리 문자쌍이면 두 칸)를 드러내고 다음 간격을 정한다 */
    void RevealNextCharacter();

    /** 남은 글자를 전부 드러내고 티커를 뗀다 */
    void CompleteTypewriter();

    /**
     * 엔진 티커가 매 프레임 부른다. false 를 돌려주면 티커가 떨어진다.
     *
     * 월드 타이머가 아니라 엔진 티커인 이유가 중요하다.
     * 튜토리얼이 뜨는 동안 게임은 일시정지 상태인데, 월드 타이머는 일시정지
     * 중에 전혀 진행되지 않는다. 타이머로 만들면 글자가 한 자도 안 나오고
     * 멈춰 있다가 클릭하는 순간 전문이 튀어나온다.
     *
     * UUserWidget 의 NativeTick 도 답이 아니다. TickFrequency 기본값(Auto)은
     * 블루프린트 Tick 이 없으면 틱을 아예 안 돌린다.
     */
    bool HandleTypewriterTicker(float InDeltaTime);

    // ── 화자 강조 ────────────────────────────────────────

    /**
     * 지금 말하는 쪽을 크고 밝게, 듣는 쪽을 작고 어둡게 만든다.
     *
     * @param bInInstant 참이면 보간 없이 즉시 적용한다. 노드가 아니라
     *                   창이 처음 열릴 때 쓴다 — 첫 화자가 작은 상태에서
     *                   커지면 등장 연출처럼 보인다.
     */
    void ApplySpeakerHighlight(ESFStageIntroSpeakerSide InSpeakerSide, bool bInInstant);

    /** 한쪽 초상화에 현재 강조도를 반영한다. 1 = 말하는 중, 0 = 듣는 중 */
    void ApplyHighlightToImage(UImage* InImage, float InAlpha) const;

    /**
     * 강조도를 목표값으로 보간한다.
     *
     * 월드 타이머가 아니라 FTSTicker 를 쓴다. 튜토리얼 창은 게임을
     * 일시정지시키는데 일시정지 중에는 월드 타이머가 전혀 진행되지 않는다.
     * 타자기 효과와 같은 이유다.
     */
    bool HandleSpeakerBlendTicker(float InDeltaTime);

    void StopSpeakerBlendTicker();

    /** 티커를 떼어낸다. 여러 곳에서 불리므로 한 곳에 모은다 */
    void StopTypewriterTicker();

    bool IsTypewriterRunning() const;

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

    /** 모든 유닛과 좌우 초상화에 공통으로 적용하는 휘장 크기(UMG 단위). */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Stage Intro|Portrait Frame",
        meta = (ClampMin = "1.0"))
    FVector2D PortraitFrameSize = FVector2D(320.f, 480.f);

    /** 휘장 크기에 대한 안쪽 여백 비율. 초상화는 이 사각형 안에 비율을 유지해서 들어간다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Stage Intro|Portrait Frame")
    FMargin PortraitFrameInset = FMargin(0.17f, 0.13f, 0.17f, 0.23f);

    /** 초상화만 확대한다. 노드별 Left/Right Portrait Zoom을 여기에 곱한다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Stage Intro|Portrait Frame", meta = (ClampMin = "0.5", ClampMax = "3.0"))
    float DefaultPortraitZoom = 1.6f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Stage Intro|Portrait Frame")
    FLinearColor PortraitBackdropColor = FLinearColor(0.025f, 0.04f, 0.065f, 1.f);

    /** 기존 대사창 높이에 대한 비율. 긴 대사는 전문을 측정해 자동으로 높인다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Stage Intro|Layout", meta = (ClampMin = "0.5", ClampMax = "1.0"))
    float DialogueHeightScale = 0.75f;

    /** 휘장을 대사창 양 끝에 코드로 붙인다. 끄면 WBP 에 잡아둔 Image 칸을 그대로 쓴다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Stage Intro|Layout")
    bool bAutoPositionPortraitFrames = false;

    /** 휘장의 중심을 대사창 양 끝에서 안쪽으로 넣는 거리(UMG 단위). */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Stage Intro|Layout")
    float PortraitInsetFromPanelEdge = 40.f;

    /** 휘장 하단이 대사창 상단과 겹치는 거리. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Stage Intro|Layout")
    float PortraitDialogueOverlap = 24.f;

    // ── 화자 강조 설정 ──────────────────────────────────
    //
    // 노드의 SpeakerSide 를 보고 말하는 쪽과 듣는 쪽을 다르게 그린다.
    // SpeakerSide 가 None 이면 둘 다 말하는 쪽으로 취급한다 — 기존에
    // 만들어 둔 행이 갑자기 어두워지지 않게 하기 위해서다.

    /** 화자 강조를 쓸지. 끄면 둘 다 원래 크기·색으로 나온다 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Stage Intro|Speaker")
    bool bUseSpeakerHighlight = true;

    /** 말하는 쪽 크기 배수 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Stage Intro|Speaker",
        meta = (ClampMin = "0.5", ClampMax = "2.0"))
    float SpeakerScale = 1.f;

    /**
     * 듣는 쪽 크기 배수.
     *
     * 0.9 아래로 내리면 뒤로 물러난 게 아니라 다른 사람처럼 보인다.
     * 크기 차이는 눈에 띄지 않아도 되고, 어느 쪽이 말하는지만 읽히면 된다.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Stage Intro|Speaker",
        meta = (ClampMin = "0.5", ClampMax = "2.0"))
    float ListenerScale = 0.93f;

    /** 말하는 쪽 색. 보통 흰색(원본 그대로) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Stage Intro|Speaker")
    FLinearColor SpeakerTint = FLinearColor::White;

    /**
     * 듣는 쪽 색.
     *
     * 알파가 아니라 색을 어둡게 한다. 투명도를 낮추면 배경이 비쳐서
     * "흐려졌다" 가 아니라 "사라지는 중" 으로 보인다.
     *
     * 파란기를 살짝 남기면 그늘로 읽힌다. 회색으로만 낮추면 빛바랜 종이처럼 보인다.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Stage Intro|Speaker")
    FLinearColor ListenerTint = FLinearColor(0.42f, 0.44f, 0.52f, 1.f);

    /**
     * 크기·색이 바뀌는 속도.
     *
     * 0 이면 즉시 바뀐다. 10 정도면 대사가 넘어갈 때 눈이 따라올 만큼
     * 부드럽고, 다음 대사를 빨리 넘겨도 늘어지지 않는다.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Stage Intro|Speaker",
        meta = (ClampMin = "0.0"))
    float SpeakerBlendSpeed = 10.f;

    /**
     * 크기를 키우고 줄이는 기준점. (0.5, 1) 이면 발밑 가운데.
     *
     * 기본값(0.5, 0.5)으로 두면 작아질 때 캐릭터가 공중으로 떠오른다.
     * 서 있는 그림은 발이 바닥에 붙어 있어야 한다.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Stage Intro|Speaker")
    FVector2D SpeakerScalePivot = FVector2D(0.5f, 1.f);

    // ── 배경음 감쇠 설정 ────────────────────────────────
    //
    // 튜토리얼 창은 게임을 일시정지시킨다. 배경음이 UI 사운드가 아니면
    // 일시정지와 함께 소리 자체가 멎으므로, 여기 값을 아무리 만져도
    // 소용이 없다. BGM 을 재생하는 쪽에서 먼저 UI 사운드로 만들어야 한다.

    /**
     * 창이 떠 있는 동안 밀어 넣을 SoundMix. 비우면 감쇠하지 않는다.
     *
     * 소리를 멈췄다 다시 트는 것이 아니라 볼륨만 눌러 두는 방식이라
     * 배경음의 재생 위치가 유지된다.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Stage Intro|Audio")
    TObjectPtr<USoundMix> DuckSoundMix = nullptr;

    /**
     * 볼륨을 누를 대상 SoundClass. 보통 배경음 클래스를 넣는다.
     *
     * 비워두면 SoundMix 에이셋에 저장된 조정값을 그대로 쓴다. 채워두면
     * 아래 DuckVolume 으로 런타임에 덮어쓰므로, 에셋을 열지 않고 값만
     * 바꿔가며 맞출 수 있다.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Stage Intro|Audio")
    TObjectPtr<USoundClass> DuckSoundClass = nullptr;

    /**
     * 창이 떠 있는 동안의 배경음 볼륨 배수.
     *
     * 0 으로 두지 마라. 완전히 죽이면 "멈췄다" 로 들려서 지금과 같아진다.
     * 0.3 안팎이 "뒤로 물러났다" 로 들린다.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Stage Intro|Audio",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float DuckVolume = 0.3f;

    /**
     * 볼륨이 내려가고 올라오는 데 걸리는 시간(초).
     *
     * 0 이면 뚝 끊기듯 들린다. 0.3~0.5 정도면 자연스럽게 물러났다 돌아온다.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF|Stage Intro|Audio",
        meta = (ClampMin = "0.0"))
    float DuckFadeSeconds = 0.4f;

    // ── 타자 효과 설정 ──────────────────────────────────

    /**
     * 타자 효과를 쓸지. 끄면 예전처럼 전문이 한 번에 나온다.
     *
     * 연출이 답답하다는 의견이 나오면 이 값 하나로 되돌릴 수 있어야 한다.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "SF|Stage Intro|Typewriter")
    bool bUseTypewriter = true;

    /**
     * 초당 글자 수.
     *
     * 한글은 한 글자에 담기는 정보가 많아 라틴 문자보다 느려도 읽힌다.
     * 30 언저리가 "읽어주는 속도" 로 자연스럽다.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "SF|Stage Intro|Typewriter",
        meta = (ClampMin = "1.0"))
    float CharactersPerSecond = 30.f;

    /**
     * 문장부호 뒤에 추가로 쉬는 시간(초).
     *
     * 균일한 속도로 흘리면 기계가 뱉는 느낌이 난다. 쉼표와 마침표에서
     * 살짝 쉬는 것만으로 사람이 읽어주는 것처럼 들린다.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "SF|Stage Intro|Typewriter",
        meta = (ClampMin = "0.0"))
    float PunctuationPauseSeconds = 0.16f;

    /**
     * 타자가 도는 동안 NextIndicatorImage 를 숨길지.
     *
     * 기본값이 꺼짐인 이유가 있다. 이 이름의 위젯에 화살표가 아니라 대사창
     * 배경 전체가 들어가 있는 경우가 있는데, 그러면 이 값을 켜는 순간 글자를
     * 찍는 동안 대사창이 통째로 사라진다.
     *
     * 진짜 "다음" 화살표를 따로 두었을 때만 켜라.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "SF|Stage Intro|Typewriter")
    bool bHideNextIndicatorWhileTyping = false;

    /** 글자가 나올 때 낼 소리. 비워두면 무음 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "SF|Stage Intro|Typewriter")
    TObjectPtr<USoundBase> TypeSound = nullptr;

    /**
     * 소리를 몇 글자마다 낼지.
     *
     * 매 글자마다 내면 소리가 겹쳐 뭉개진다.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "SF|Stage Intro|Typewriter",
        meta = (ClampMin = "1"))
    int32 SoundEveryNCharacters = 3;

private:
    UPROPERTY(Transient)
    TMap<TObjectPtr<UCanvasPanelSlot>, FAnchorData> OriginalConversationLayouts;

    UPROPERTY(Transient)
    TObjectPtr<USFStageIntroPortraitFrame> LeftPortraitFrame = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<USFStageIntroPortraitFrame> RightPortraitFrame = nullptr;

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

    // ── 타자 효과 상태 ──────────────────────────────────

    /**
     * 지금 글자를 채워 넣고 있는 텍스트 위젯.
     *
     * 대사와 조작 설명이 서로 다른 위젯을 쓰므로 대상을 들고 있어야 한다.
     */
    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> TypingTarget = nullptr;

    /** 다 드러났을 때의 전문 */
    FString FullTypingText;

    /** 지금까지 드러낸 글자 수(FString 인덱스) */
    int32 RevealedCharCount = 0;

    /** 이 노드에 적용 중인 한 글자 간격(초) */
    float ActiveTypeInterval = 0.03f;

    /** 다음 글자까지 남은 시간을 재는 누적값 */
    float TypeAccumulator = 0.f;

    /**
     * 다음 한 글자를 내보내기까지 기다릴 시간.
     *
     * 보통은 ActiveTypeInterval 과 같고, 문장부호 뒤에서만 길어진다.
     */
    float NextCharacterDelay = 0.f;

    FTSTicker::FDelegateHandle TypewriterTickerHandle;

    // ── 화자 강조 상태 ──────────────────────────────────
    //
    // 1 = 말하는 중, 0 = 듣는 중. 그 사이 값은 전환 중이다.

    float LeftHighlightAlpha = 1.f;
    float RightHighlightAlpha = 1.f;
    float LeftHighlightTarget = 1.f;
    float RightHighlightTarget = 1.f;

    FTSTicker::FDelegateHandle SpeakerBlendTickerHandle;

    /**
     * 마지막으로 세운 초상화. 그림이 바뀌었는지 보려고 들고 있다.
     *
     * 인물이 교체되는 노드는 화면 전환에 가까우므로 보간 없이 즉시 맞춘다.
     * 새 인물이 작은 상태에서 커지면 등장 연출처럼 읽힌다.
     */
    TSoftObjectPtr<UTexture2D> LastLeftCharacterImage;
    TSoftObjectPtr<UTexture2D> LastRightCharacterImage;

    static constexpr int32 DialoguePanelIndex = 0;
    static constexpr int32 ChoicePanelIndex = 1;
    static constexpr int32 ControlGuidePanelIndex = 2;
};
