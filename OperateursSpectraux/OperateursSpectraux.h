#pragma once

#include "IPlug_include_in_plug_hdr.h"
#include "SpectralCurveEngine.h"
#include "SpectralCurvePreviewControl.h"
#include "SpectralFilterEngine.h"
#include "BrickwallLimiter.h"
#include <atomic>
#include <mutex>

// ============================================================================
// Etape 3 : premier module branche sur du vrai son - Filtre spectral, pilote
// par la courbe partagee (Cycles/Q/Ballade/Horizon/Skew ou Dessin libre).
// Stereo (2 instances de SpectralFilterEngine, une par canal).
// ============================================================================

enum EParams
{
  kParamFFTSize = 0,   // 0=512, 1=1024, 2=2048, 3=4096, 4=8192
  kParamOverlap,       // 0=2x, 1=4x
  kParamCycles,
  kParamQ,
  kParamBallade,
  kParamHorizon,
  kParamSkew,
  kParamShapeMode, // 0 = Type (sinus), 1 = Dessin libre
  kParamLimiterThreshold, // dB - seuil du limiteur Brickwall final (securite)
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
  void UpdateFFTConfig();
  void UpdateEngine();

  SpectralCurveEngine mEngine;
  SpectralFilterEngine mFilterL, mFilterR;
  BrickwallLimiter mLimiter;

  // La courbe (calculee sur le thread interface/parametres) est copiee ici
  // sous mutex, puis lue par le thread audio a chaque bloc - section
  // critique tres courte (une simple copie), impact RT negligeable en
  // pratique pour ce cas d'usage.
  std::mutex mCurveMutex;
  std::vector<float> mSharedCurve;

  std::atomic<bool> mCurveUIUpdated { false };
  float mCurveUIBuf[512] = { 0.f };
  int mCurveUISize = 0;
#endif
};
