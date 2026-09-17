#pragma once

#include "IPlug_include_in_plug_hdr.h"
#include "SpectralCurveEngine.h"
#include "SpectralCurvePreviewControl.h"
#include "SpectralAntiCompEngine.h"
#include "BrickwallLimiter.h"
#include "SpectrumAnalyzer.h"
#include <atomic>
#include <mutex>

// ============================================================================
// Etape 5 : Anti-Comp (compresseur inverse, ratio < 1) - chaque bande FFT a
// son propre seuil (-60dB a 0dB), pilote par la courbe partagee. Ratio
// global. Stereo (2 instances, une par canal).
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
  kParamRatio,            // 0.02-1.0 : ratio du compresseur inverse (plus bas = plus extreme)
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
  void OnUIOpen() override { SyncUIToState(); }
  void OnUIClose() override { mCurveView = nullptr; for (auto& c : mParamControls) c = nullptr; }

  bool SerializeState(IByteChunk& chunk) const override;
  int UnserializeState(const IByteChunk& chunk, int startPos) override;

#if IPLUG_DSP
  void ProcessBlock(sample** inputs, sample** outputs, int nFrames) override;
  void OnParamChange(int paramIdx) override;
  void OnReset() override;
#endif

private:
  SpectralCurvePreviewControl* mCurveView = nullptr;
  IControl* mParamControls[kNumParams] = { nullptr };

  std::vector<float> mDrawnShapeStorage;

  void ApplyAllState();
  void SyncUIToState();

#if IPLUG_DSP
  void UpdateFFTConfig();
  void UpdateEngine();
  void UpdateYAxisMarks();

  SpectralCurveEngine mEngine;
  SpectralAntiCompEngine mCompL, mCompR;
  BrickwallLimiter mLimiter;
  SpectrumAnalyzer mAnalyzer;

  std::mutex mCurveMutex;
  std::vector<float> mSharedCurve;

  std::mutex mSpectrumMutex;
  std::atomic<bool> mSpectrumUIUpdated { false };
  float mSpectrumUIBuf[1100] = { -80.f };
  int mSpectrumUISize = 0;

  std::atomic<bool> mCurveUIUpdated { false };
  float mCurveUIBuf[512] = { 0.f };
  int mCurveUISize = 0;
#endif
};
