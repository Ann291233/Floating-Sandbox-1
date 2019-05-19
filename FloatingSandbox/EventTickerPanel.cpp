/***************************************************************************************
* Original Author:		Gabriele Giuseppini
* Created:				2018-03-06
* Copyright:			Gabriele Giuseppini  (https://github.com/GabrieleGiuseppini)
***************************************************************************************/
#include "EventTickerPanel.h"

#include <wx/dcbuffer.h>

#include <cassert>
#include <sstream>

EventTickerPanel::EventTickerPanel(wxWindow* parent)
    : wxPanel(
        parent,
        wxID_ANY,
        wxDefaultPosition,
        wxDefaultSize,
        wxBORDER_SIMPLE)
    , mBufferedDCBitmap()
    , mCurrentTickerText(TickerTextSize, ' ')
    , mFutureTickerText()
    , mCurrentCharStep(TickerFontSize)
{
    SetMinSize(wxSize(-1, 1 + TickerFontSize + 1));
    SetMaxSize(wxSize(-1, 1 + TickerFontSize + 1));

    SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));

    SetDoubleBuffered(true);

    Connect(this->GetId(), wxEVT_PAINT, (wxObjectEventFunction)&EventTickerPanel::OnPaint);
    Connect(this->GetId(), wxEVT_ERASE_BACKGROUND, (wxObjectEventFunction)&EventTickerPanel::OnEraseBackground);

    //
    // Set font
    //

    mFont = wxFont(wxFontInfo(wxSize(TickerFontSize, TickerFontSize)).Family(wxFONTFAMILY_TELETYPE));
    SetFont(mFont);
}

EventTickerPanel::~EventTickerPanel()
{
}

void EventTickerPanel::Update()
{
    mCurrentCharStep += TickerCharStep;
    if (mCurrentCharStep >= TickerFontSize)
    {
        mCurrentCharStep = 0;

        // Pop first char
        assert(TickerTextSize == mCurrentTickerText.size());
        mCurrentTickerText.erase(mCurrentTickerText.begin());

        // Add last char
        if (!mFutureTickerText.empty())
        {
            mCurrentTickerText.push_back(mFutureTickerText.front());
            mFutureTickerText.erase(mFutureTickerText.begin());
        }
        else
        {
            mCurrentTickerText.push_back(' ');
        }
    }

    // Rendering costs ~2%, hence let's do it only when needed!
    if (this->IsShown())
    {
        Refresh();
    }
}

///////////////////////////////////////////////////////////////////////////////////////

void EventTickerPanel::OnGameReset()
{
    mCurrentTickerText = std::string(TickerTextSize, ' ');
    mFutureTickerText.clear();
}

void EventTickerPanel::OnShipLoaded(
    unsigned int /*id*/,
    std::string const & name,
    std::optional<std::string> const & author)
{
    std::stringstream ss;
    ss << "Loaded " << name;

    if (!!author)
        ss << " by " << *author;

    AppendFutureTickerText(ss.str());
}

void EventTickerPanel::OnSinkingBegin(ShipId shipId)
{
    std::stringstream ss;
    ss << "SHIP " << shipId << " IS SINKING!";

    AppendFutureTickerText(ss.str());
}

void EventTickerPanel::OnSinkingEnd(ShipId shipId)
{
    std::stringstream ss;
    ss << "SHIP " << shipId << " HAS STOPPED SINKING!";

    AppendFutureTickerText(ss.str());
}

void EventTickerPanel::OnStress(
    StructuralMaterial const & structuralMaterial,
    bool isUnderwater,
    unsigned int size)
{
    std::stringstream ss;
    ss << "Stressed " << size << "x" << structuralMaterial.Name << (isUnderwater ? " underwater" : "") << "!";

    AppendFutureTickerText(ss.str());
}

void EventTickerPanel::OnBreak(
    StructuralMaterial const & structuralMaterial,
    bool isUnderwater,
    unsigned int size)
{
    std::stringstream ss;
    ss << "Broken " << size << "x" << structuralMaterial.Name << (isUnderwater ? " underwater" : "") << "!";

    AppendFutureTickerText(ss.str());
}

void EventTickerPanel::OnTsunami(float x)
{
    std::stringstream ss;
    ss << "WARNING: Tsunami at " << x;

    AppendFutureTickerText(ss.str());
}

void EventTickerPanel::OnDestroy(
    StructuralMaterial const & structuralMaterial,
    bool isUnderwater,
    unsigned int size)
{
    std::stringstream ss;
    ss << "Destroyed " << size << "x" << structuralMaterial.Name << (isUnderwater ? " underwater" : "") << "!";

    AppendFutureTickerText(ss.str());
}

void EventTickerPanel::OnSpringRepaired(
    StructuralMaterial const & structuralMaterial,
    bool isUnderwater,
    unsigned int size)
{
    std::stringstream ss;
    ss << "Repaired spring " << size << "x" << structuralMaterial.Name << (isUnderwater ? " underwater" : "") << "!";

    AppendFutureTickerText(ss.str());
}

void EventTickerPanel::OnTriangleRepaired(
    StructuralMaterial const & structuralMaterial,
    bool isUnderwater,
    unsigned int size)
{
    std::stringstream ss;
    ss << "Repaired triangle " << size << "x" << structuralMaterial.Name << (isUnderwater ? " underwater" : "") << "!";

    AppendFutureTickerText(ss.str());
}

void EventTickerPanel::OnBombPlaced(
    BombId /*bombId*/,
    BombType bombType,
    bool /*isUnderwater*/)
{
    std::stringstream ss;
    switch (bombType)
    {
        case BombType::AntiMatterBomb:
        {
            ss << "Anti-matter";
            break;
        }

        case BombType::ImpactBomb:
        {
            ss << "Impact";
            break;
        }

        case BombType::RCBomb:
        {
            ss << "Remote-controlled";
            break;
        }

        case BombType::TimerBomb:
        {
            ss << "Timer";
            break;
        }
    }

    ss << " bomb placed!";

    AppendFutureTickerText(ss.str());
}

void EventTickerPanel::OnBombRemoved(
    BombId /*bombId*/,
    BombType bombType,
    std::optional<bool> /*isUnderwater*/)
{
    std::stringstream ss;
    ss << (BombType::RCBomb == bombType ? "Remote-controlled" : "Timer") << " bomb removed";

    AppendFutureTickerText(ss.str());
}

void EventTickerPanel::OnBombExplosion(
    BombType /*bombType*/,
    bool /*isUnderwater*/,
    unsigned int size)
{
    std::stringstream ss;
    ss << "Bomb" << (size > 1 ? "s" : "") << " exploded!";

    AppendFutureTickerText(ss.str());
}

///////////////////////////////////////////////////////////////////////////////////////

void EventTickerPanel::OnPaint(wxPaintEvent & /*event*/)
{
    if (!mBufferedDCBitmap || mBufferedDCBitmap->GetSize() != this->GetSize())
    {
        mBufferedDCBitmap = std::make_unique<wxBitmap>(this->GetSize());
    }

    wxBufferedPaintDC bufDc(this, *mBufferedDCBitmap);

    Render(bufDc);
}

void EventTickerPanel::OnEraseBackground(wxPaintEvent & /*event*/)
{
    // Do nothing
}

void EventTickerPanel::AppendFutureTickerText(std::string const & text)
{
    mFutureTickerText.clear();

    assert(!mCurrentTickerText.empty());
    if (mCurrentTickerText.back() != ' '
        && mCurrentTickerText.back() != '>')
    {
        mFutureTickerText.append(">");
    }

    mFutureTickerText.append(text);
}

void EventTickerPanel::Render(wxDC & dc)
{
    wxSize tickerPanelSize = dc.GetSize();

    int leftX = tickerPanelSize.GetWidth() + TickerFontSize - mCurrentCharStep - (TickerTextSize * TickerFontSize);

    wxString tickerText(mCurrentTickerText, TickerTextSize);

    dc.Clear();
    dc.DrawText(tickerText, leftX, -2);
}