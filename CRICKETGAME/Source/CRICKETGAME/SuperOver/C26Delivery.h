#pragma once
#include "C26Types.h"

/** Shared by human and AI bowling. Selecting a variation never changes the target. */
namespace C26Delivery
{
    inline void Shape(FC26DeliveryPlan& P)
    {
        P.Speed=P.Type==EC26Delivery::Slower?2470.f:3670.f;
        P.Swing=P.Type==EC26Delivery::Outswing?165.f:P.Type==EC26Delivery::Inswing?-165.f:12.f;
        P.Bounce=P.Length<180.f?.69f:P.Length>730.f?.47f:.55f;
        P.Seam=P.Type==EC26Delivery::Outswing?16.f:P.Type==EC26Delivery::Inswing?-16.f:5.f;
    }
    inline FC26DeliveryPlan Execute(const FC26DeliveryPlan& Locked,float Error)
    {
        FC26DeliveryPlan Actual=Locked;
        const float E=FMath::Clamp(Error,-2.f,2.f),Fault=FMath::Abs(E);
        Actual.Line+=E*38.f;
        Actual.Length=FMath::Clamp(Actual.Length+E*45.f,0.f,850.f);
        Actual.Speed*=1.f-FMath::Min(.18f,Fault*.07f);
        Actual.Swing*=1.f-FMath::Min(.6f,Fault*.24f);
        Actual.Seam+=E*13.f;
        Actual.NoBall=Locked.NoBall||Error>1.35f;
        return Actual;
    }
    inline const TCHAR* Name(EC26Delivery Type)
    {
        switch(Type){case EC26Delivery::Outswing:return TEXT("OUTSWING");case EC26Delivery::Inswing:return TEXT("INSWING");case EC26Delivery::Slower:return TEXT("SLOWER BALL");default:return TEXT("STOCK PACE");}
    }
    inline const TCHAR* LengthName(float Length)
    {return Length>730.f?TEXT("YORKER"):Length>550.f?TEXT("FULL"):Length>180.f?TEXT("GOOD LENGTH"):TEXT("SHORT");}
    inline const TCHAR* ReleaseName(float Error)
    {return FMath::Abs(Error)<.22f?TEXT("PERFECT RELEASE"):FMath::Abs(Error)<.65f?TEXT("GOOD RELEASE"):FMath::Abs(Error)>1.2f?TEXT("POOR RELEASE"):Error<0?TEXT("EARLY RELEASE"):TEXT("LATE RELEASE");}
}
