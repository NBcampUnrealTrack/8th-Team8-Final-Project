#include "SFEtherVictoryPreview.h"
#include "Layout/Geometry.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Textures/SlateShaderResource.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Styling/CoreStyle.h"
#include "Engine/Font.h"

int32 PaintEtherVictoryPreview(const FGeometry& G,FSlateWindowElementList& Out,int32 Layer,
    float T,UFont* Font,const FLinearColor& Tint,bool bVictory)
{
    auto Palette=[&](FLinearColor Color)
    {
        if(bVictory)return Color;
        return FLinearColor(FMath::Max(Color.R,Color.B*.92f),Color.G*.29f,Color.B*.23f,Color.A);
    };
    const FVector2D S=G.GetLocalSize(), C(S.X*.5,S.Y*.43);
    const float Fit=FMath::Min(S.X/1920.,S.Y/1080.);
    constexpr float Impact=1.8f;
    const float Charge=FMath::Clamp(T/Impact,0.f,1.f);
    const float After=FMath::Max(0.f,T-Impact);
    const float Reveal=FMath::Clamp((After-.12f)/.62f,0.f,1.f);
    const float Ease=1.f-FMath::Pow(1.f-Reveal,3.f);
    const FLinearColor Cyan(.12f,.78f,1.f), Gold(1.f,.69f,.24f);
    const FSlateBrush* White=FCoreStyle::Get().GetBrush("WhiteBrush");
    const auto Resource=FSlateApplication::Get().GetRenderer()->GetResourceHandle(*White);
    const auto* Proxy=Resource.GetResourceProxy();
    const FVector2f UV=Proxy?Proxy->StartUV+Proxy->SizeUV*.5f:FVector2f(.5f,.5f);
    auto Vertex=[&](FVector2D P,FLinearColor Color)
    { return FSlateVertex::Make<ESlateVertexRounding::Disabled>(G.GetAccumulatedRenderTransform(),FVector2f(P),UV,(Palette(Color)*Tint).ToFColor(true)); };
    auto Line=[&](TArray<FVector2D> P,FLinearColor Color,float Width=1.f)
    { FSlateDrawElement::MakeLines(Out,Layer,G.ToPaintGeometry(),P,ESlateDrawEffect::None,Palette(Color)*Tint,true,Width*Fit); };
    auto Glow=[&](FVector2D P,float Radius,FLinearColor Color,float Aspect=1.f)
    {
        TArray<FSlateVertex> V;TArray<SlateIndex> I;V.Add(Vertex(P,Color));
        for(int N=0;N<=64;++N)
        {
            const float A=2*PI*N/64;
            V.Add(Vertex(P+FVector2D(FMath::Cos(A),FMath::Sin(A)*Aspect)*Radius*Fit,FLinearColor::Transparent));
            if(N)I.Append({0,static_cast<SlateIndex>(N),static_cast<SlateIndex>(N+1)});
        }
        FSlateDrawElement::MakeCustomVerts(Out,Layer,Resource,V,I,nullptr,0,0);
    };
    auto Beam=[&](float Angle,float Radius,float Spread,FLinearColor Color)
    {
        constexpr int Radial=9,Angular=6;
        TArray<FSlateVertex> V;TArray<SlateIndex> I;
        for(int R=0;R<=Radial;++R)for(int A=0;A<=Angular;++A)
        {
            const float Along=R/static_cast<float>(Radial),Across=A/static_cast<float>(Angular);
            const float Theta=Angle+(Across-.5f)*Spread;
            FLinearColor K=Color;K.A*=FMath::Square(FMath::Sin(PI*Across))*FMath::Pow(1-Along,1.5f);
            V.Add(Vertex(C+FVector2D(FMath::Cos(Theta),FMath::Sin(Theta))*(70+Along*Radius)*Fit,K));
            if(R<Radial && A<Angular)
            {
                const SlateIndex N=R*(Angular+1)+A,J=N+Angular+1;
                I.Append({N,static_cast<SlateIndex>(N+1),J,static_cast<SlateIndex>(N+1),static_cast<SlateIndex>(J+1),J});
            }
        }
        FSlateDrawElement::MakeCustomVerts(Out,Layer,Resource,V,I,nullptr,0,0);
    };
    auto Shard=[&](FVector2D P,float Radius,float Angle,FLinearColor Color)
    {
        const FVector2D U(FMath::Cos(Angle),FMath::Sin(Angle)), V(-U.Y,U.X);
        const FVector2D Points[]={P-U*Radius*Fit,P+V*Radius*.36f*Fit,P+U*Radius*Fit,P-V*Radius*.36f*Fit};
        TArray<FSlateVertex> Vertices={Vertex(Points[0],Color),Vertex(Points[1],Color*.5f),Vertex(Points[2],Color),Vertex(Points[3],FLinearColor(Color.R*.3f,Color.G*.5f,Color.B,Color.A))};
        TArray<SlateIndex> Indices={0,1,2,0,2,3};
        FSlateDrawElement::MakeCustomVerts(Out,Layer,Resource,Vertices,Indices,nullptr,0,0);
        Line({Points[0],Points[1],Points[2]},FLinearColor(.65f,.95f,1,Color.A*.8f),1.2f);
    };
    auto Arc=[&](float Radius,float Start,float Length,FLinearColor Color,float Width,float Aspect=1.f)
    {
        TArray<FVector2D> P;
        for(int N=0;N<=48;++N)
        { const float A=Start+Length*N/48;P.Add(C+FVector2D(FMath::Cos(A),FMath::Sin(A)*Aspect)*Radius*Fit); }
        Line(P,Color,Width);
    };
    auto Text=[&](const FString& Value,FVector2D Center,int Size,FLinearColor Color,float Outline)
    {
        FSlateFontInfo Info=Font?FSlateFontInfo(Font,FMath::Max(1,FMath::RoundToInt(Size*Fit))):FCoreStyle::GetDefaultFontStyle("Bold",FMath::Max(1,FMath::RoundToInt(Size*Fit)));
        Info.OutlineSettings.OutlineSize=FMath::RoundToInt(Outline*Fit);
        Info.OutlineSettings.OutlineColor=FLinearColor(.02f,.045f,.07f,Color.A);
        const FVector2D Extent=FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Value,Info);
        FSlateDrawElement::MakeText(Out,Layer,G.ToPaintGeometry(Extent,FSlateLayoutTransform(Center-Extent*.5)),Value,Info,ESlateDrawEffect::None,Palette(Color)*Tint);
    };

    // The battlefield remains visible beneath the imploding ether core.
    const float Shade=.2f+.63f*FMath::Clamp(T/.9f,0.f,1.f);
    FSlateDrawElement::MakeBox(Out,Layer++,G.ToPaintGeometry(),White,ESlateDrawEffect::None,FLinearColor(.004f,.012f,.024f,Shade)*Tint);
    Glow(C,900,FLinearColor(.015f,.12f,.22f,.5f));
    ++Layer;
    const float BorderFade=FMath::Clamp(T/.8f,0.f,1.f);
    for(double Y:{0.,S.Y-46*Fit})
        FSlateDrawElement::MakeBox(Out,Layer,G.ToPaintGeometry(FVector2D(S.X,46*Fit),FSlateLayoutTransform(FVector2D(0,Y))),White,ESlateDrawEffect::None,FLinearColor(0,0,0,.5f*BorderFade)*Tint);
    ++Layer;
    if(T<Impact)
    {
        for(int I=0;I<14;++I)Beam(I*2*PI/14-T*.22f,640,.16f,FLinearColor(.1f,.7f,1,.07f+.16f*Charge));
        ++Layer;
        const float Radius=FMath::Lerp(300.f,36.f,FMath::Pow(Charge,.85f));
        Glow(C,180+190*Charge,FLinearColor(.06f,.55f,1,.2f+.6f*Charge));
        for(int R=0;R<3;++R)
            Arc(Radius+R*46, T*(R%2?-1.f:1.f)*(1.1f+Charge*3)+R,PI*1.56f,FLinearColor(.12f,.72f,1,.24f+Charge*.6f),1.3f+Charge*2,R==1?.54f:1.f);
        for(int I=0;I<46;++I)
        {
            const float Seed=FMath::Frac(I*.61803399f), A=I*2.39996f+T*.35f;
            const float Travel=FMath::Frac(Seed+T*(.38f+Charge*.55f));
            const FVector2D D(FMath::Cos(A),FMath::Sin(A)*.7f);
            const float R=FMath::Lerp(650.f,20.f,Travel);
            Line({C+D*R*Fit,C+D*(R+18+Charge*28)*Fit},FLinearColor(.18f,.8f,1,Travel*.65f),1.4f);
        }
        for(int I=0;I<24;++I)
        {
            const float A=I*2*PI/24+T*.16f;const FVector2D D(FMath::Cos(A),FMath::Sin(A));
            const float R=Radius+114;
            Line({C+D*R*Fit,C+D*(R+9+Charge*12)*Fit},FLinearColor(.45f,.85f,1,.18f+.42f*Charge),1.3f);
            if(I%3==0)Shard(C+D*(R+26)*Fit,6.f+Charge*3,A,FLinearColor(.4f,.8f,1,.3f+.6f*Charge));
        }
        Shard(C,56.f-30.f*Charge,-PI*.5f+FMath::Sin(T*18)*Charge*.05f,FLinearColor(.35f,.84f,1,1));
        Glow(C,18+Charge*32,FLinearColor(.8f,.97f,1,Charge));
    }
    else
    {
        for(int I=0;I<26;++I)
        {
            const float Alpha=(.28f*FMath::Exp(-After*.85f)+.055f*Ease)*(I%3?.65f:1.f);
            Beam(I*2*PI/26+After*.065f,950+(I%4)*100,.1f+(I%3)*.08f,FLinearColor(I%3?Cyan:Gold).CopyWithNewOpacity(Alpha));
        }
        ++Layer;
        const float Flash=FMath::Exp(-After*8.f);
        Glow(C,1150,FLinearColor(.65f,.91f,1,.87f*Flash));
        Glow(C,1200,FLinearColor(.45f,.85f,1,.55f*FMath::Exp(-After*2.2f)),.028f);
        Glow(C,680,FLinearColor(1,.79f,.35f,.35f*FMath::Exp(-After*1.4f)),.12f);
        ++Layer;
        for(int Ring=0;Ring<5;++Ring)
        {
            const float Age=After-Ring*.1f;
            if(Age<0 || Age>1.4f)continue;
            const float P=Age/1.4f;
            Arc(40+1150*(1-FMath::Pow(1-P,2.f)),0,2*PI,FLinearColor(.42f,.86f,1,(1-P)*.75f),3.2f*(1-P)+.6f,.75f);
        }
        for(int I=0;I<132;++I)
        {
            const float Seed=FMath::Frac(I*.61803399f), A=I*2.39996f;
            const FVector2D D(FMath::Cos(A),FMath::Sin(A)*.65f);
            const float Dist=40+(350+Seed*700)*(1-FMath::Exp(-After*1.8f));
            const FVector2D P=C+D*Dist*Fit+FVector2D(0,After*After*18)*Fit;
            const float Alpha=FMath::Clamp(1-After/(2.2f+Seed*1.6f),0.f,1.f);
            if(Alpha>0)
            {
                Shard(P,5+Seed*13,A+After*(2+Seed*5),FLinearColor(I%3?Cyan:Gold).CopyWithNewOpacity(Alpha));
                Line({P-D*(18+Seed*26)*Fit,P},FLinearColor(.3f,.8f,1,Alpha*.6f),1.1f);
            }
        }
        ++Layer;
        // Segmented rune halo and a forged laurel give both outcomes a shared crest.
        for(int I=0;I<32;++I)
        {
            const float A=I*2*PI/32+After*(bVictory?.025f:-.035f);
            if(!bVictory && I%5==0)continue;
            Arc(310,A,.125f,FLinearColor(.2f,.66f,.82f,Ease*.25f),1.2f);
            const FVector2D D(FMath::Cos(A),FMath::Sin(A));
            Line({C+D*318*Fit,C+D*(I%4==0?337:327)*Fit},FLinearColor(.65f,.8f,.9f,Ease*(I%4==0?.5f:.22f)),1.2f);
            if(I%4==0)Shard(C+D*347*Fit,4.5f,A,FLinearColor(.45f,.85f,1,Ease*.48f));
        }
        for(int Side:{-1,1})
        {
            TArray<FVector2D> Stem;
            for(int I=0;I<11;++I)
            {
                const float P=I/10.f;
                const FVector2D Pos=C+FVector2D(Side*(218+112*FMath::Sin(P*PI*.83f)),184-306*P)*Fit;
                Stem.Add(Pos);
                if(bVictory)
                {
                    Shard(Pos+FVector2D(Side*17,-11)*Fit,20-5*P,Side*.62f-PI*.5f,FLinearColor(1,.69f,.25f,Ease*.72f));
                    Shard(Pos+FVector2D(-Side*13,3)*Fit,13-3*P,-Side*.45f-PI*.5f,FLinearColor(.5f,.72f,.7f,Ease*.53f));
                }
                else if(I%3!=1)
                {
                    Shard(Pos+FVector2D(Side*12,After<2?(1-Ease)*80:0)*Fit,16-4*P,Side*1.1f-P,FLinearColor(.37f,.5f,.62f,Ease*.64f));
                }
            }
            if(bVictory)Line(Stem,FLinearColor(.95f,.64f,.25f,Ease*.72f),2.f);
            else for(int I=0;I+1<Stem.Num();I+=3)Line({Stem[I],Stem[I+1]},FLinearColor(.65f,.45f,.25f,Ease*.6f),2.f);
        }
        Glow(C+FVector2D(0,182)*Fit,510,FLinearColor(.15f,.6f,1,Ease*.22f),.022f);
        ++Layer;
        // A new geometric sigil forms from fragments; no victory sprite is used.
        const FVector2D Sigil=C+FVector2D(0,-190-(1-Ease)*65)*Fit;
        Glow(Sigil,210,FLinearColor(.08f,.6f,1,.35f*Ease));
        if(bVictory)
        {
        Shard(Sigil,78,-PI*.5f,FLinearColor(.12f,.65f,1,Ease));
        Shard(Sigil+FVector2D(-62,14)*Fit,38,-PI*.7f,FLinearColor(.18f,.78f,1,Ease*.85f));
        Shard(Sigil+FVector2D(62,14)*Fit,38,-PI*.3f,FLinearColor(.18f,.78f,1,Ease*.85f));
        Line({Sigil+FVector2D(-112,20)*Fit,Sigil+FVector2D(-78,57)*Fit,Sigil+FVector2D(0,82)*Fit,Sigil+FVector2D(78,57)*Fit,Sigil+FVector2D(112,20)*Fit},FLinearColor(1,.72f,.32f,Ease),2.4f);
        }
        else
        {
            // Broken pieces settle apart instead of forming the victory crown.
            Shard(Sigil+FVector2D(-15,-16)*Fit,43,-PI*.58f,FLinearColor(.19f,.62f,.9f,Ease));
            Shard(Sigil+FVector2D(26,41)*Fit,35,-PI*.4f,FLinearColor(.12f,.4f,.7f,Ease));
            Shard(Sigil+FVector2D(-70,50)*Fit,24,-PI*.82f,FLinearColor(.15f,.5f,.8f,Ease*.75f));
            Shard(Sigil+FVector2D(73,65)*Fit,20,-PI*.15f,FLinearColor(.15f,.5f,.8f,Ease*.7f));
            Line({Sigil+FVector2D(-100,55)*Fit,Sigil+FVector2D(-65,85)*Fit,Sigil+FVector2D(-19,99)*Fit},FLinearColor(.65f,.4f,.28f,Ease),2.f);
            Line({Sigil+FVector2D(19,99)*Fit,Sigil+FVector2D(65,85)*Fit,Sigil+FVector2D(100,55)*Fit},FLinearColor(.65f,.4f,.28f,Ease),2.f);
        }
        ++Layer;
        const float Settle=FMath::Sin(After*9.f)*FMath::Exp(-After*3.f)*9.f;
        const FVector2D Title=C+FVector2D(0,Settle)*Fit+FVector2D(0,8+(1-Ease)*48)*Fit;
        Text(bVictory?TEXT("승리"):TEXT("패배"),Title+FVector2D(0,7)*Fit,174,FLinearColor(.18f,.07f,.015f,Ease),5);
        ++Layer;
        Text(bVictory?TEXT("승리"):TEXT("패배"),Title,174,FLinearColor(1,.83f,.48f,Ease),3);
        ++Layer;
        Text(bVictory?TEXT("V I C T O R Y"):TEXT("D E F E A T"),C+FVector2D(0,136)*Fit,30,FLinearColor(.6f,.83f,.9f,Ease*.9f),0);
        for(int Side:{-1,1})
        {
            const FVector2D P=C+FVector2D(Side*330,22)*Fit;
            Line({P+FVector2D(Side*125,-22)*Fit,P,P+FVector2D(Side*125,22)*Fit},FLinearColor(1,.69f,.25f,Ease*.75f),2);
            Line({P+FVector2D(Side*25,0)*Fit,P+FVector2D(Side*270,0)*Fit},FLinearColor(.2f,.75f,1,Ease*.4f),1);
        }
        for(int I=0;I<100;++I)
        {
            const float Seed=FMath::Frac(I*.61803399f), X=FMath::Frac(I*.75487766f)*S.X;
            const float Y=(bVictory?1-FMath::Frac(Seed+After*(.014f+Seed*.013f)):FMath::Frac(Seed+After*(.02f+Seed*.018f)))*S.Y;
            const float Alpha=Ease*(.15f+.4f*FMath::Square(FMath::Sin(T+I)))*FMath::Sin(PI*Y/S.Y);
            const FVector2D P(X,Y);
            Shard(P,1.4f+Seed*2.2f,T*.15f+I,FLinearColor(I%4?Cyan:Gold).CopyWithNewOpacity(Alpha));
        }
    }
    return Layer;
}
