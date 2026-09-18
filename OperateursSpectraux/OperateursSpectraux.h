#pragma once

#include "IPlug_include_in_plug_hdr.h"
#include "SpectralCurveEngine.h"
#include "SpectralCurvePreviewControl.h"
#include "SpectralFilterEngine.h"
#include "SpectralDelayEngine.h"
#include "SpectralMagnitudeDistortEngine.h"
#include "BrickwallLimiter.h"
#include "SpectrumAnalyzer.h"
#include <atomic>
#include <mutex>

// ============================================================================
// Etape 6 : Les Operateurs Spectraux - matrice complete. 3 modules
// (Filtre/Delay/Distorsion), chacun avec sa propre courbe et son propre
// On/Off, routables en serie (6 ordres possibles) ou en parallele.
// Limiteur Brickwall toujours en toute derniere position. Stereo.
// ============================================================================

enum EParams
{
  kParamFFTSize = 0,
  kParamOverlap,
  kParamRouting, // 0-5 = les 6 ordres en serie, 6 = Parallele

  kParamFilterEnable,
  kParamFilterCycles, kParamFilterQ, kParamFilterBallade, kParamFilterHorizon, kParamFilterSkew, kParamFilterShapeMode,

  kParamDelayEnable,
  kParamDelayCycles, kParamDelayQ, kParamDelayBallade, kParamDelayHorizon, kParamDelaySkew, kParamDelayShapeMode,
  kParamDelayFeedback, kParamDelaySyncMode,

  kParamDistoEnable,
  kParamDistoCycles, kParamDistoQ, kParamDistoBallade, kParamDistoHorizon, kParamDistoSkew, kParamDistoShapeMode,
  kParamDistoInjection, kParamDistoDecay, kParamDistoDrive,

  kParamLimiterThreshold,

  kNumParams
};

using namespace iplug;
using namespace igraphics;

class OperateursSpectraux final : public iplug::Plugin
{
public:
  OperateursSpectraux(const InstanceInfo& info);

  void OnIdle() override;
  void OnUIOpen() override { SyncUIToState(); }
  void OnUIClose() override
  {
    mFilterCurveView = mDelayCurveView = mDistoCurveView = nullptr;
    for (auto& c : mParamControls) c = nullptr;
  }

#if IPLUG_DSP
  void ProcessBlock(sample** inputs, sample** outputs, int nFrames) override;
  void OnParamChange(int paramIdx) override;
  void OnReset() override;
#endif

private:
  SpectralCurvePreviewControl* mFilterCurveView = nullptr;
  SpectralCurvePreviewControl* mDelayCurveView = nullptr;
  SpectralCurvePreviewControl* mDistoCurveView = nullptr;
  IControl* mParamControls[kNumParams] = { nullptr };

  std::vector<float> mFilterDrawnShape, mDelayDrawnShape, mDistoDrawnShape;

  void ApplyAllState();
  void SyncUIToState();

#if IPLUG_DSP
  void UpdateFFTConfig();
  void UpdateFilterCurve();
  void UpdateDelayCurve();
  void UpdateDistoCurve();
  void UpdateDelayYAxisMarks();
  void UpdateDistoYAxisMarks();

  SpectralCurveEngine mFilterEngine, mDelayEngine, mDistoEngine;
  SpectralFilterEngine mFilterL, mFilterR;
  SpectralDelayEngine mDelayL, mDelayR;
  SpectralMagnitudeDistortEngine mDistortL, mDistortR;
  BrickwallLimiter mLimiter;
  SpectrumAnalyzer mAnalyzer;

  // Protege l'ENSEMBLE des 3 moteurs : Init() (thread principal, au
  // changement de FFT Size) ne doit jamais s'executer en meme temps que
  // Process() (thread audio) - lecon tiree d'un crash reproductible a
  // grande taille FFT sur une version precedente.
  std::mutex mEngineMutex;

  std::mutex mFilterCurveMutex;
  std::vector<float> mSharedFilterCurve;
  std::mutex mDelayCurveMutex;
  std::vector<float> mSharedDelayCurve;
  std::mutex mDistoCurveMutex;
  std::vector<float> mSharedDistoCurve;

  std::mutex mDryDelayMutex; // reserve pour un usage futur (pas de Dry/Wet global pour l'instant)

  std::mutex mSpectrumMutex;
  std::atomic<bool> mSpectrumUIUpdated { false };
  float mSpectrumUIBuf[1100] = { -80.f };
  int mSpectrumUISize = 0;

  std::atomic<bool> mFilterCurveUIUpdated { false };
  float mFilterCurveUIBuf[512] = { 0.f };
  int mFilterCurveUISize = 0;

  std::atomic<bool> mDelayCurveUIUpdated { false };
  float mDelayCurveUIBuf[512] = { 0.f };
  int mDelayCurveUISize = 0;

  std::atomic<bool> mDistoCurveUIUpdated { false };
  float mDistoCurveUIBuf[512] = { 0.f };
  int mDistoCurveUISize = 0;
#endif
};
