#pragma once

#include "IPlug_include_in_plug_hdr.h"
#include "SpectralCurveEngine.h"
#include "SpectralCurvePreviewControl.h"
#include "SpectralMagnitudeDistortEngine.h"
#include "BrickwallLimiter.h"
#include "SpectrumAnalyzer.h"
#include <atomic>
#include <mutex>

// ============================================================================
// SpectralDistortion - module mono-effet, issu du projet fusionne "Les
// Operateurs Spectraux" (desormais abandonne en tant que projet combine,
// separe en 3 plugins distincts). Distorsion en magnitude par bande +
// injection harmonique + distorsion temporelle, pilotees par une courbe
// spectrale partagee. Limiteur Brickwall en securite finale. Stereo.
// ============================================================================

enum EParams
{
  kParamFFTSize = 0,   // 0=512..4=8192
  kParamOverlap,       // 0=2x, 1=4x
  kParamCycles,
  kParamQ,
  kParamBallade,
  kParamHorizon,
  kParamSkew,
  kParamShapeMode,        // 0 = Type, 1 = Dessin libre
  kParamHarmonicInjection, // 0-100% : injection harmonique (x2/x3/x4...)
  kParamDecayExponent,    // 0.2-1.0 : decroissance + nombre d'harmoniques injectees
  kParamTempDrive,        // 0-100% : distorsion temporelle (waveshaping)
  kParamDryWet,           // 0-100% : melange signal sec (retarde, compense) / traite, courbe exp
  kParamLimiterThreshold, // dB - securite finale
  kNumParams
};

using namespace iplug;
using namespace igraphics;

class SpectralDistortion final : public iplug::Plugin
{
public:
  SpectralDistortion(const InstanceInfo& info);

  void OnIdle() override;
  void OnUIOpen() override { SyncUIToState(); }
  void OnUIClose() override { mCurveView = nullptr; for (auto& c : mParamControls) c = nullptr; }

  // Dessin libre non sauvegarde entre sessions Reaper (SerializeState
  // retire par le passe suite a un crash grave - voir historique du
  // projet "Les Operateurs Spectraux").

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
  SpectralMagnitudeDistortEngine mDistortL, mDistortR;
  BrickwallLimiter mLimiter;
  SpectrumAnalyzer mAnalyzer;

  // Protege le moteur : Init() (thread principal, au changement de FFT
  // Size) ne doit jamais s'executer en meme temps que Process() (thread
  // audio) - lecon tiree d'un crash reproductible a grande taille FFT.
  std::mutex mEngineMutex;

  // Ligne a retard du signal sec, alignee EXACTEMENT sur la latence du
  // traitement (une fenetre FFT), pour que Dry/Wet ne cree pas de
  // decalage temporel - le Dry/Wet de l'hote ne compense pas cette
  // latence, d'ou la necessite de le faire nous-memes, en interne.
  std::mutex mDryDelayMutex;
  std::vector<float> mDryDelayL, mDryDelayR;
  int mDryDelayPos = 0;
  int mDryDelaySize = 1;

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
