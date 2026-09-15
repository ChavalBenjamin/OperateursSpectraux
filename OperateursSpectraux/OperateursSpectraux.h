#pragma once

#include "IPlug_include_in_plug_hdr.h"
#include "SpectralCurveEngine.h"
#include "SpectralCurvePreviewControl.h"
#include <atomic>

// ============================================================================
// Etape 1 : moteur de courbe partage seul (Cycles/Q/Ballade/Horizon/Skew),
// avec apercu visuel - PAS de traitement audio pour l'instant, juste pour
// valider le comportement mathematique avant de le brancher sur un vrai
// module (Filtre/Delay/Inverse Comp).
// ============================================================================

enum EParams
{
  kParamCycles = 0,
  kParamQ,
  kParamBallade,
  kParamHorizon,
  kParamSkew,
  kNumParams
};

using namespace iplug;
using namespace igraphics;

class OperateursSpectraux final : public iplug::Plugin
{
public:
  OperateursSpectraux(const InstanceInfo& info);

  void OnIdle() override;
  void OnUIClose() override { mCurveView = nullptr; }

#if IPLUG_DSP
  void ProcessBlock(sample** inputs, sample** outputs, int nFrames) override;
  void OnParamChange(int paramIdx) override;
  void OnReset() override;
#endif

private:
  SpectralCurvePreviewControl* mCurveView = nullptr;

#if IPLUG_DSP
  void UpdateEngine();

  SpectralCurveEngine mEngine;
  std::atomic<bool> mCurveUIUpdated { false };
  float mCurveUIBuf[512] = { 0.f };
  int mCurveUISize = 0;
#endif
};
