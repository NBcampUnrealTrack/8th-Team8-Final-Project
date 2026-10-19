#include "SFPVPLobbyWidget.h"
#include "Game/Multiplayer/SFPVPSessionSubsystem.h"
#include "Game/Multiplayer/SFPVPLobby.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "UObject/ConstructorHelpers.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"

namespace SFPVPUI
{
    const FLinearColor Gold(.65f,.43f,.20f), Cream(.96f,.89f,.74f), Teal(.19f,.81f,.75f), Muted(.53f,.62f,.61f);
}
USFPVPLobbyWidget::USFPVPLobbyWidget(const FObjectInitializer& Initializer) : Super(Initializer)
{
    static ConstructorHelpers::FObjectFinder<UFont> F(TEXT("/Game/Game/IN_UI/Font/Jua-Regular_Font.Jua-Regular_Font")); Font = F.Object;
    static ConstructorHelpers::FObjectFinder<UTexture2D> E(TEXT("/Game/Game/IN_UI/Image/Intro/StoryUse/Etherial/Commander/T_EtherCommander_Normal.T_EtherCommander_Normal")); EtherPortrait = E.Object;
    static ConstructorHelpers::FObjectFinder<UTexture2D> B(TEXT("/Game/Game/IN_UI/Image/Intro/StoryUse/Beast/Commander/Beast_Commander_Smile.Beast_Commander_Smile")); BeastPortrait = B.Object;
    SetIsFocusable(true);
}
USFPVPSessionSubsystem* USFPVPLobbyWidget::Sessions() const { return GetGameInstance()->GetSubsystem<USFPVPSessionSubsystem>(); }
bool USFPVPLobbyWidget::InRoom() const { return GetWorld() && GetWorld()->GetNetMode() != NM_Standalone; }
TSharedRef<SWidget> USFPVPLobbyWidget::Label(const FString& Text, int32 Size, FLinearColor Color) const
{
    return SNew(STextBlock).Text(FText::FromString(Text)).Font(FSlateFontInfo(Font,Size)).ColorAndOpacity(Color).Justification(ETextJustify::Center);
}
TSharedRef<SWidget> USFPVPLobbyWidget::PlayerCard(int32 Seat)
{
    using namespace SFPVPUI;
    const auto Player = [this, Seat]() -> ASFPVPLobbyPlayerState*
    { auto* S = GetWorld()->GetGameState<ASFPVPLobbyState>(); return S ? S->GetSeat(Seat) : nullptr; };
    return SNew(SBorder).BorderImage(&PanelBrush).Padding(24)
    [SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()[Label(Seat == 0 ? TEXT("01  /  방장") : TEXT("02  /  도전자"),22,Gold)]
        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0,12)
        [SNew(SBox).WidthOverride(190).HeightOverride(190)
            [SNew(SImage).Image_Lambda([this,Player]() -> const FSlateBrush* { auto* P=Player(); return P && P->Race==ESFRace::Beast ? &BeastBrush : &EtherBrush; })
                .ColorAndOpacity_Lambda([Player] { return Player() ? FLinearColor::White : FLinearColor(.15f,.2f,.2f,.35f); })]]
        + SVerticalBox::Slot().AutoHeight()
        [SNew(STextBlock).Font(FSlateFontInfo(Font,28)).ColorAndOpacity(Cream).Justification(ETextJustify::Center)
            .Text_Lambda([Player] { auto* P=Player(); return FText::FromString(!P ? TEXT("상대를 기다리는 중") : P->Race==ESFRace::Beast ? TEXT("비스트") : TEXT("에테리안")); })]
        + SVerticalBox::Slot().AutoHeight().Padding(0,10)
        [SNew(STextBlock).Font(FSlateFontInfo(Font,20)).ColorAndOpacity(Teal).Justification(ETextJustify::Center)
            .Text_Lambda([Player] { auto* P=Player(); return FText::FromString(!P ? TEXT("빈 자리") : P->bReady ? TEXT("준비 완료") : TEXT("종족 선택 중")); })]];
}
TSharedRef<SWidget> USFPVPLobbyWidget::RebuildWidget()
{
    using namespace SFPVPUI;
    EtherBrush.SetResourceObject(EtherPortrait); BeastBrush.SetResourceObject(BeastPortrait);
    EtherBrush.ImageSize=BeastBrush.ImageSize=FVector2D(190);
    PanelBrush=FSlateRoundedBoxBrush(FLinearColor(.025f,.048f,.048f),8.f,Gold,1.f);
    ButtonStyle=FButtonStyle().SetNormal(FSlateRoundedBoxBrush(FLinearColor(.04f,.09f,.09f),4.f,Gold,1.f))
        .SetHovered(FSlateRoundedBoxBrush(FLinearColor(.07f,.20f,.19f),4.f,Teal,2.f))
        .SetPressed(FSlateRoundedBoxBrush(FLinearColor(.02f,.05f,.05f),4.f,Teal,1.f))
        .SetDisabled(FSlateRoundedBoxBrush(FLinearColor(.035f,.04f,.04f),4.f,Muted*.4f,1.f))
        .SetNormalPadding(FMargin(20,12)).SetPressedPadding(FMargin(20,13,20,11));
    InputStyle=FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox");
    InputStyle.SetFont(FSlateFontInfo(Font,20));
    const auto Button=[this](const TCHAR* Text,TFunction<void()> Action,bool bNeedsLogin=false) -> TSharedRef<SWidget>
    { return SNew(SButton).ButtonStyle(&ButtonStyle).IsEnabled_Lambda([this,bNeedsLogin]{return !Sessions()->IsBusy()&&(!bNeedsLogin||!Sessions()->IsInternetMode()||Sessions()->IsInternetLoggedIn());})
        .OnClicked_Lambda([Action]{Action();return FReply::Handled();})[Label(Text,21,SFPVPUI::Cream)]; };
    return SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
    .BorderBackgroundColor(FLinearColor(.008f,.019f,.023f)).Padding(28)
    [SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly)
    [SNew(SBox).WidthOverride(1080).HeightOverride(860)
    [SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()[Label(TEXT("S T R I K E F O R G E"),20,Gold)]
        + SVerticalBox::Slot().AutoHeight().Padding(0,6)[Label(TEXT("PVP  |  결투의 전당"),42,Cream)]
        + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,18)
            [Label(TEXT("1 대 1     |     스타트맵 제외 / 무작위 전장"),18,Muted)]
        + SVerticalBox::Slot().FillHeight(1)
        [SNew(SVerticalBox).Visibility_Lambda([this]{return InRoom()?EVisibility::Collapsed:EVisibility::Visible;})
            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0,0,0,12)
            [SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth()
                [SNew(SButton).ButtonStyle(&ButtonStyle).IsEnabled_Lambda([this]{return !Sessions()->IsBusy();})
                    .OnClicked_Lambda([this]{Sessions()->SetInternetMode(true);return FReply::Handled();})
                    [SNew(STextBlock).Font(FSlateFontInfo(Font,21)).Text(FText::FromString(TEXT("인터넷")))
                        .ColorAndOpacity_Lambda([this]{return Sessions()->IsInternetMode()?SFPVPUI::Teal:SFPVPUI::Muted;})]]
                + SHorizontalBox::Slot().AutoWidth().Padding(12,0)
                [SNew(SButton).ButtonStyle(&ButtonStyle).IsEnabled_Lambda([this]{return !Sessions()->IsBusy();})
                    .OnClicked_Lambda([this]{Sessions()->SetInternetMode(false);return FReply::Handled();})
                    [SNew(STextBlock).Font(FSlateFontInfo(Font,21)).Text(FText::FromString(TEXT("같은 네트워크")))
                        .ColorAndOpacity_Lambda([this]{return !Sessions()->IsInternetMode()?SFPVPUI::Teal:SFPVPUI::Muted;})]]
                + SHorizontalBox::Slot().AutoWidth()
                [SNew(SButton).ButtonStyle(&ButtonStyle)
                    .Visibility_Lambda([this]{return Sessions()->IsInternetMode()?EVisibility::Visible:EVisibility::Collapsed;})
                    .IsEnabled_Lambda([this]{return !Sessions()->IsBusy()&&Sessions()->IsInternetConfigured()&&!Sessions()->IsInternetLoggedIn();})
                    .OnClicked_Lambda([this]{Sessions()->LoginInternet();return FReply::Handled();})
                    [SNew(STextBlock).Font(FSlateFontInfo(Font,21)).ColorAndOpacity(Cream)
                        .Text_Lambda([this]{return FText::FromString(Sessions()->GetIdentityLabel());})]]]
            + SVerticalBox::Slot().AutoHeight()
            [SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(1).Padding(0,0,12,0)
                [SAssignNew(RoomInput,SEditableTextBox).Style(&InputStyle).Text(FText::FromString(TEXT("StrikeForge 1대1")))
                    .HintText(FText::FromString(TEXT("방 이름"))).IsEnabled_Lambda([this]{return !Sessions()->IsBusy();})]
                + SHorizontalBox::Slot().AutoWidth()[Button(TEXT("방 만들기"),[this]{Sessions()->Host(RoomInput->GetText().ToString());},true)]
                + SHorizontalBox::Slot().AutoWidth().Padding(12,0,0,0)[Button(TEXT("새로고침"),[this]{Sessions()->FindRooms();},true)]]
            + SVerticalBox::Slot().FillHeight(1).Padding(0,18)
            [SNew(SBorder).BorderImage(&PanelBrush).Padding(22)
                [SNew(SScrollBox)+SScrollBox::Slot()[SAssignNew(RoomList,SVerticalBox)]]]
            + SVerticalBox::Slot().AutoHeight()
            [SNew(SHorizontalBox).Visibility_Lambda([this]{return Sessions()->IsInternetMode()?EVisibility::Collapsed:EVisibility::Visible;})
                + SHorizontalBox::Slot().FillWidth(1).Padding(0,0,12,0)
                [SAssignNew(AddressInput,SEditableTextBox).Style(&InputStyle).HintText(FText::FromString(TEXT("방장 IP 주소  예: 192.168.0.10:7777")))
                    .IsEnabled_Lambda([this]{return !Sessions()->IsBusy();})]
                + SHorizontalBox::Slot().AutoWidth()[Button(TEXT("주소로 참가"),[this]{Sessions()->ConnectAddress(AddressInput->GetText().ToString());})]]
            + SVerticalBox::Slot().AutoHeight().Padding(0,12)
                [SNew(STextBlock).Font(FSlateFontInfo(Font,16)).ColorAndOpacity(Muted).Justification(ETextJustify::Center)
                    .Text_Lambda([this]{return FText::FromString(Sessions()->GetConnectionDescription());})]]
        + SVerticalBox::Slot().AutoHeight()
        [SNew(SVerticalBox).Visibility_Lambda([this]{return InRoom()?EVisibility::Visible:EVisibility::Collapsed;})
            + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,12)
            [SNew(STextBlock).Font(FSlateFontInfo(Font,24)).ColorAndOpacity(Teal).Justification(ETextJustify::Center)
                .Text_Lambda([this]{auto* S=GetWorld()->GetGameState<ASFPVPLobbyState>();return FText::FromString(S?S->RoomName:TEXT("접속 중"));})]
            + SVerticalBox::Slot().AutoHeight()
            [SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(1)[PlayerCard(0)]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(28,0)[Label(TEXT("VS"),32,Gold)]
                + SHorizontalBox::Slot().FillWidth(1)[PlayerCard(1)]]
            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0,18)
            [SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth()[Button(TEXT("에테리안"),[this]{if(auto* PC=Cast<ASFPVPLobbyController>(GetOwningPlayer())) PC->ServerSetRace(ESFRace::Human);})]
                + SHorizontalBox::Slot().AutoWidth().Padding(12,0)[Button(TEXT("비스트"),[this]{if(auto* PC=Cast<ASFPVPLobbyController>(GetOwningPlayer())) PC->ServerSetRace(ESFRace::Beast);})]
                + SHorizontalBox::Slot().AutoWidth()
                [SNew(SButton).ButtonStyle(&ButtonStyle).OnClicked_Lambda([this]{auto* PC=Cast<ASFPVPLobbyController>(GetOwningPlayer());auto* P=PC?PC->GetPlayerState<ASFPVPLobbyPlayerState>():nullptr;if(P)PC->ServerSetReady(!P->bReady);return FReply::Handled();})
                [SNew(STextBlock).Font(FSlateFontInfo(Font,21)).ColorAndOpacity(Cream).Text_Lambda([this]{auto* P=GetOwningPlayer()->GetPlayerState<ASFPVPLobbyPlayerState>();return FText::FromString(P&&P->bReady?TEXT("준비 취소"):TEXT("준비"));})]]]
            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0,0,0,10)
            [SNew(SBox).Visibility_Lambda([this]{return Sessions()->IsInternetMode()?EVisibility::Visible:EVisibility::Collapsed;})
                [Button(TEXT("Steam 친구 초대"),[this]{Sessions()->ShowSteamInvite();})]]
            + SVerticalBox::Slot().AutoHeight()
            [SNew(STextBlock).Font(FSlateFontInfo(Font,18)).ColorAndOpacity(Muted).Justification(ETextJustify::Center)
                .Text_Lambda([this]{
                    const auto* S=GetWorld()->GetGameState<ASFPVPLobbyState>();
                    if(S&&!S->StartError.IsEmpty())return FText::FromString(S->StartError);
                    if(S&&S->bStarting)return FText::FromString(TEXT("무작위 전장으로 함께 이동하고 있습니다."));
                    return FText::FromString(S&&S->AreBothReady()?TEXT("두 플레이어 준비 완료 / 방장이 시작을 누르면 무작위 전장으로 이동합니다."):TEXT("상대와 종족을 선택하고 준비하세요."));})]]
        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0,12)
        [SNew(SButton).ButtonStyle(&ButtonStyle)
            .Visibility_Lambda([this]{auto* P=GetOwningPlayer()->GetPlayerState<ASFPVPLobbyPlayerState>();return InRoom()&&P&&P->Seat==0?EVisibility::Visible:EVisibility::Collapsed;})
            .IsEnabled_Lambda([this]{auto* S=GetWorld()->GetGameState<ASFPVPLobbyState>();return S&&S->AreBothReady()&&!S->bStarting;})
            .OnClicked_Lambda([this]{if(auto* PC=Cast<ASFPVPLobbyController>(GetOwningPlayer()))PC->ServerStartBattle();return FReply::Handled();})
            [Label(TEXT("전투 시작"),24,Gold)]]
        + SVerticalBox::Slot().AutoHeight().Padding(0,18,0,10)
        [SNew(STextBlock).Font(FSlateFontInfo(Font,18)).ColorAndOpacity(Teal).Justification(ETextJustify::Center).AutoWrapText(true)
            .Text_Lambda([this]{return FText::FromString(Sessions()->GetStatus());})]
        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
            [Button(TEXT("돌아가기"),[this]{Sessions()->Leave(!InRoom());})]
    ]]];
}
void USFPVPLobbyWidget::RefreshRooms()
{
    if (!RoomList) return;
    const auto* S=Sessions(); FString Signature=S->GetStatus();
    for(int32 I=0;I<S->GetRooms().Num();++I) Signature+=S->DescribeRoom(I);
    if(Signature==RoomSignature)return;
    RoomSignature=Signature;RoomList->ClearChildren();
    if(S->GetRooms().IsEmpty()) RoomList->AddSlot().AutoHeight().Padding(16)[Label(TEXT("방을 만들거나 새로고침으로 상대를 찾아보세요."),22,SFPVPUI::Muted)];
    for(int32 I=0;I<S->GetRooms().Num();++I)
        RoomList->AddSlot().AutoHeight().Padding(0,4)
        [SNew(SButton).ButtonStyle(&ButtonStyle).HAlign(HAlign_Left)
            .IsEnabled_Lambda([this,I]{const auto* Sub=Sessions();return !Sub->IsBusy()&&Sub->GetRooms().IsValidIndex(I)&&Sub->GetRooms()[I].Session.NumOpenPublicConnections>0;})
            .OnClicked_Lambda([this,I]{Sessions()->JoinRoom(I);return FReply::Handled();})[Label(S->DescribeRoom(I),23,SFPVPUI::Cream)]];
}
void USFPVPLobbyWidget::NativeTick(const FGeometry& Geometry,float DeltaTime)
{Super::NativeTick(Geometry,DeltaTime);if(!InRoom())RefreshRooms();}
void USFPVPLobbyWidget::ReleaseSlateResources(bool bReleaseChildren)
{Super::ReleaseSlateResources(bReleaseChildren);RoomInput.Reset();AddressInput.Reset();RoomList.Reset();RoomSignature.Empty();}
