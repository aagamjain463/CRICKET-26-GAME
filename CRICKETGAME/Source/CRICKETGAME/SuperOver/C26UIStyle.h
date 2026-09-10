#pragma once
#include "CoreMinimal.h"

namespace C26UIStyle
{
inline const FLinearColor Void = FLinearColor::FromSRGBColor(FColor(10, 18, 26));
inline const FLinearColor Paper = FLinearColor::FromSRGBColor(FColor(239, 245, 246));
inline const FLinearColor MintCyan = FLinearColor::FromSRGBColor(FColor(65, 219, 195));
inline const FLinearColor Coral = FLinearColor::FromSRGBColor(FColor(246, 113, 106));
inline const FLinearColor Gold = FLinearColor::FromSRGBColor(FColor(225, 190, 121));
inline const FLinearColor PaperDim = FMath::Lerp(Void, Paper, .68f);
inline const FLinearColor Muted = FMath::Lerp(Void, Paper, .44f);
inline const FLinearColor Hairline = FMath::Lerp(Void, Paper, .10f);
inline const FLinearColor PillBg(Void.R, Void.G, Void.B, .94f);
inline const FLinearColor CardBg(Void.R, Void.G, Void.B, .78f);
inline constexpr float TopH = 84.f, NavW = 248.f;
inline constexpr float ContentX = 312.f, ContentR = 1536.f;
inline constexpr float ContentW = ContentR - ContentX, ContentTop = 124.f;
}
