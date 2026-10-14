#include "Game/Audio/SFVoiceAssetLibrary.h"
#include "Sound/SoundCue.h"
#include "Sound/SoundNodeRandom.h"
#include "Sound/SoundNodeWavePlayer.h"
#include "Sound/SoundWave.h"
#if WITH_EDITOR
#include "EdGraph/EdGraphNode.h"
#endif

bool USFVoiceAssetLibrary::ConfigureResponseCue(USoundCue* Cue, const TArray<USoundWave*>& Waves)
{
#if WITH_EDITOR
    if (!Cue || Waves.IsEmpty() || Waves.Contains(nullptr)) return false;
    Cue->Modify();
    Cue->ResetGraph();
    auto* Random = Cue->ConstructSoundNode<USoundNodeRandom>(USoundNodeRandom::StaticClass(), false);
    Random->bRandomizeWithoutReplacement = true;
    Random->PreselectAtLevelLoad = 0;
    TArray<USoundNode*> Children;
    for (int32 Index = 0; Index < Waves.Num(); ++Index)
    {
        auto* Player = Cue->ConstructSoundNode<USoundNodeWavePlayer>(USoundNodeWavePlayer::StaticClass(), false);
        Player->SetSoundWave(Waves[Index]);
        Player->bLooping = false;
        Player->GraphNode->NodePosX = -500;
        Player->GraphNode->NodePosY = Index * 110;
        Children.Add(Player);
    }
    Random->SetChildNodes(Children);
    Random->Weights.Init(1.f, Waves.Num());
    Random->GraphNode->ReconstructNode();
    Random->GraphNode->NodePosX = -220;
    Random->GraphNode->NodePosY = (Waves.Num() - 1) * 55;
    Cue->FirstNode = Random;
    Cue->VolumeMultiplier = 1.f;
    Cue->PitchMultiplier = 1.f;
    Cue->LinkGraphNodesFromSoundNodes();
    Cue->PostEditChange();
    Cue->MarkPackageDirty();
    return true;
#else
    return false;
#endif
}
