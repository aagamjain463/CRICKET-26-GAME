#include "C26Settings.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/GameUserSettings.h"
#include "HAL/IConsoleManager.h"
UC26Settings* UC26Settings::Load()
{
    auto* S=Cast<UC26Settings>(UGameplayStatics::LoadGameFromSlot(TEXT("C26Preferences"),0));
    return S?S:NewObject<UC26Settings>();
}
void UC26Settings::Save(){UGameplayStatics::SaveGameToSlot(this,TEXT("C26Preferences"),0);}
void UC26Settings::Apply()
{
    if(auto* G=UGameUserSettings::GetGameUserSettings())
    {
        G->SetOverallScalabilityLevel(Quality);G->SetViewDistanceQuality(2);
        G->SetResolutionScaleValueEx(Quality==0?65:Quality==1?80:Quality==2?90:100);
        G->SetFrameRateLimit(Quality==0?30:60);G->ApplyNonResolutionSettings();
    }
    if(auto* V=IConsoleManager::Get().FindConsoleVariable(TEXT("r.MotionBlurQuality")))V->Set(0,ECVF_SetByGameSetting);
}
