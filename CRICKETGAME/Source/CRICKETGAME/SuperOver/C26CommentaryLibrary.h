#pragma once

#include "CoreMinimal.h"
#include "C26CommentaryTypes.h"

/**
 * Authoritative data repository for CRICKET 26 broadcast commentary lines.
 * Provides filtered lookup by Category, Role, Emotion, Pressure, and Match Climax.
 */
class CRICKETGAME_API FC26CommentaryLibrary
{
public:
    static const TArray<FC26CommentaryLineDef>& GetAllLines();
    static void Initialize();

    /** Queries matching lines based on event context, filtering by emotion, role, and pressure range. */
    static TArray<const FC26CommentaryLineDef*> FindMatchingLines(
        FName Category,
        ECommentatorRole Role,
        ECommentaryEmotion Emotion,
        float Pressure,
        float Intensity,
        bool bIsFinalBall,
        bool bIsMatchWinning,
        bool bFollowUpOnly
    );

    static const FC26CommentaryLineDef* FindLineById(FName LineId);

private:
    static TArray<FC26CommentaryLineDef> Lines;
    static bool bInitialized;
};
