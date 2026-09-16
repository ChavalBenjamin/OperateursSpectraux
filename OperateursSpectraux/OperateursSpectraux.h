#pragma once

#include "IPlug_include_in_plug_hdr.h"
#include "SpectralCurveEngine.h"
#include "SpectralCurvePreviewControl.h"
#include "SpectralDelayEngine.h"
#include "BrickwallLimiter.h"
#include <atomic>
#include <mutex>

// ============================================================================
// Etape 4 : Delay spectral - chaque bande FFT a son propre temps de retard
// (5ms a 2.5s), pilote par la courbe partagee, avec feedback et sync BPM.
// Stereo (2 instances de SpectralDelayEngine, une par canal).
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
  kParamShapeMode,        // 0 = Type (sinus), 1 = Dessin libre
  kParamFeedback,         // 0-150% (peut depasser 100%, le limiteur protege)
  kParamSyncMode,         // Off/On : la grille bascule ms <-> divisions rythmiques
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
  void UpdateYAxisMarks();

  SpectralCurveEngine mEngine;
  SpectralDelayEngine mDelayL, mDelayR;
  BrickwallLimiter mLimiter;

  // La courbe (calculee sur le thread interface/parametres) est copiee ici
  // sous mutex, puis lue par le thread audio a chaque bloc.
  std::mutex mCurveMutex;
  std::vector<float> mSharedCurve;

  std::atomic<bool> mCurveUIUpdated { false };
  float mCurveUIBuf[512] = { 0.f };
  int mCurveUISize = 0;
#endif
};
