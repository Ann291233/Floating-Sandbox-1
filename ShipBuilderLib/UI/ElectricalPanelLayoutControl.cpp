/***************************************************************************************
 * Original Author:     Gabriele Giuseppini
 * Created:             2022-04-06
 * Copyright:           Gabriele Giuseppini  (https://github.com/GabrieleGiuseppini)
 ***************************************************************************************/
#include "ElectricalPanelLayoutControl.h"

#include <wx/dcclient.h>

#include <cassert>

namespace ShipBuilder {

int constexpr ElementHeight = 100;

ElectricalPanelLayoutControl::ElectricalPanelLayoutControl(
    wxWindow * parent,
    ResourceLocator const & resourceLocator)
{
    // Calculate size
    wxSize const size = wxSize(-1, 20 + ElementHeight * 2 + 20);

    // Create panel
    Create(
        parent,
        wxID_ANY,
        wxDefaultPosition,
        size,
        wxBORDER_SIMPLE);

    SetMinSize(size);

    // Bind paint
    Bind(wxEVT_PAINT, &ElectricalPanelLayoutControl::OnPaint, this);

    // Initialize rendering
#ifdef __WXMSW__
    SetDoubleBuffered(true);
#endif

    // Create drawing tools
    // TODOHERE
    mGuidePen = wxPen(wxColor(0, 0, 0), 1, wxPENSTYLE_SHORT_DASH);
    mWaterlinePen = wxPen(wxColor(57, 127, 189), 1, wxPENSTYLE_SOLID);
    mWaterPen = wxPen(wxColor(77, 172, 255), 1, wxPENSTYLE_SOLID);
    mWaterBrush = wxBrush(mWaterPen.GetColour(), wxBRUSHSTYLE_SOLID);
}

////void ElectricalPanelLayoutControl::SetValue(
////    float trimCW,
////    bool floats)
////{
////    mOutcome.emplace(
////        trimCW,
////        floats);
////
////    // Render
////    Refresh(false);
////}

void ElectricalPanelLayoutControl::Clear()
{
    mOutcome.reset();

    // Render
    Refresh(false);
}

void ElectricalPanelLayoutControl::OnPaint(wxPaintEvent & /*event*/)
{
    wxPaintDC dc(this);
    Render(dc);
}

void ElectricalPanelLayoutControl::Render(wxDC & dc)
{
    wxSize const size = GetSize();

    dc.Clear();

    ////if (mOutcome.has_value())
    ////{
    ////    // 0 is at top
    ////    float const waterlineY = mOutcome->Floats ? size.GetHeight() / 2 : 2;

    ////    //
    ////    // Draw water
    ////    //

    ////    dc.SetPen(mWaterPen);
    ////    dc.SetBrush(mWaterBrush);
    ////    dc.DrawRectangle(
    ////        // Top-left, also origin
    ////        0,
    ////        waterlineY,
    ////        size.GetWidth() - 1,
    ////        size.GetHeight() - 1 - waterlineY);

    ////    //
    ////    // Draw waterline
    ////    //

    ////    dc.SetPen(mWaterlinePen);
    ////    dc.DrawLine(
    ////        0,
    ////        waterlineY,
    ////        size.GetWidth() - 1,
    ////        waterlineY);

    ////    //
    ////    // Dra vertical guide
    ////    //

    ////    dc.SetPen(mGuidePen);
    ////    dc.DrawLine(
    ////        size.GetWidth() / 2,
    ////        0,
    ////        size.GetWidth() / 2,
    ////        size.GetHeight() - 1);

    ////    //
    ////    // Draw ship
    ////    //

    ////    // Rotate bitmap
    ////    wxImage rotatedShip = mShipImage.Rotate(
    ////        -mOutcome->TrimCW,
    ////        wxPoint(
    ////            mShipImage.GetWidth() / 2,
    ////            mShipImage.GetHeight() / 2));

    ////    // Make bitmap
    ////    auto shipBitmap = wxBitmap(rotatedShip);

    ////    // Draw bitmap
    ////    dc.DrawBitmap(
    ////        shipBitmap,
    ////        wxPoint(
    ////            size.GetWidth() / 2 - rotatedShip.GetWidth() / 2,
    ////            size.GetHeight() / 2 - rotatedShip.GetHeight() / 2));
    ////}
}

}