/***************************************************************************************
* Original Author:      Gabriele Giuseppini
* Created:              2018-09-07
* Copyright:            Gabriele Giuseppini  (https://github.com/GabrieleGiuseppini)
***************************************************************************************/
#include "ProbePanel.h"

#include <wx/stattext.h>

#include <cassert>
#include <chrono>

static constexpr int TopPadding = 2;
static constexpr int ProbePadding = 10;

ProbePanel::ProbePanel(wxWindow* parent)
    : wxPanel(
        parent,
        wxID_ANY,
        wxDefaultPosition,
        wxDefaultSize,
        wxBORDER_SIMPLE | wxCLIP_CHILDREN)
{
#ifdef __WXMSW__
    SetDoubleBuffered(true);
#endif

    SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE));


    //
    // Create probes
    //

    mProbesSizer = new wxBoxSizer(wxHORIZONTAL);

    mFrameRateProbe = AddScalarTimeSeriesProbe(_("Frame Rate"), 200);
    mCurrentUpdateDurationProbe = AddScalarTimeSeriesProbe(_("Update Time"), 200);

    mWaterTakenProbe = AddScalarTimeSeriesProbe(_("Water Inflow"), 120);
    mWaterSplashProbe = AddScalarTimeSeriesProbe(_("Water Splash"), 200);

    mWindSpeedProbe = AddScalarTimeSeriesProbe(_("Wind Speed"), 200);

    //
    // Finalize
    //

    SetSizerAndFit(mProbesSizer);
}

ProbePanel::~ProbePanel()
{
}

void ProbePanel::UpdateSimulation()
{
    //
    // Update all probes
    //

    if (IsActive())
    {
        mFrameRateProbe->UpdateSimulation();
        mCurrentUpdateDurationProbe->UpdateSimulation();
        mWaterTakenProbe->UpdateSimulation();
        mWaterSplashProbe->UpdateSimulation();
        mWindSpeedProbe->UpdateSimulation();

        for (auto const & p : mCustomProbes)
        {
            p.second->UpdateSimulation();
        }
    }
}

std::unique_ptr<ScalarTimeSeriesProbeControl> ProbePanel::AddScalarTimeSeriesProbe(
    wxString const & name,
    int sampleCount)
{
    wxBoxSizer * sizer = new wxBoxSizer(wxVERTICAL);

    sizer->AddSpacer(TopPadding);

    auto probe = std::make_unique<ScalarTimeSeriesProbeControl>(this, sampleCount);
    sizer->Add(probe.get(), 1, wxALIGN_CENTRE, 0);

    wxStaticText * label = new wxStaticText(this, wxID_ANY, name, wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE_HORIZONTAL);
    sizer->Add(label, 0, wxALIGN_CENTRE, 0);

    mProbesSizer->Add(sizer, 1, wxLEFT | wxRIGHT, ProbePadding);

    return probe;
}

///////////////////////////////////////////////////////////////////////////////////////

void ProbePanel::OnGameReset()
{
    mFrameRateProbe->Reset();
    mCurrentUpdateDurationProbe->Reset();
    mWaterTakenProbe->Reset();
    mWaterSplashProbe->Reset();
    mWindSpeedProbe->Reset();

    for (auto const & p : mCustomProbes)
    {
        p.second->Reset();
    }
}

void ProbePanel::OnWaterTaken(float waterTaken)
{
    mWaterTakenProbe->RegisterSample(waterTaken);
}

void ProbePanel::OnWaterSplashed(float waterSplashed)
{
    mWaterSplashProbe->RegisterSample(waterSplashed);
}

void ProbePanel::OnWindSpeedUpdated(
    float const /*zeroSpeedMagnitude*/,
    float const /*baseSpeedMagnitude*/,
    float const /*baseAndStormSpeedMagnitude*/,
    float const /*preMaxSpeedMagnitude*/,
    float const /*maxSpeedMagnitude*/,
    vec2f const & windSpeed)
{
    mWindSpeedProbe->RegisterSample(windSpeed.length());
}

void ProbePanel::OnCustomProbe(
    std::string const & name,
    float value)
{
    auto & probe = mCustomProbes[name];
    if (!probe)
    {
        probe = AddScalarTimeSeriesProbe(name, 100);
        mProbesSizer->Layout();
    }

    probe->RegisterSample(value);
}

void ProbePanel::OnFrameRateUpdated(
    float immediateFps,
    float /*averageFps*/)
{
    mFrameRateProbe->RegisterSample(immediateFps);
}

void ProbePanel::OnCurrentUpdateDurationUpdated(float currentUpdateDuration)
{
    mCurrentUpdateDurationProbe->RegisterSample(currentUpdateDuration);
}