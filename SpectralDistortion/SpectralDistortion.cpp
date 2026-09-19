#include "SpectralDistortion.h"
#include "IPlug_include_in_plug_src.h"
#include "IControls.h"
#include <cmath>
#include <string>

SpectralDistortion::SpectralDistortion(const InstanceInfo& info)
: iplug::Plugin(info, MakeConfig(kNumParams, 1))
{
  GetParam(kParamFFTSize)->InitEnum("FFT Size", 2, 5, "", IParam::kFlagsNone, "", "512", "1024", "2048", "4096", "8192");
  GetParam(kParamOverlap)->InitEnum("Overlap", 1, 2, "", IParam::kFlagsNone, "", "2x (50%)", "4x (75%)");
  GetParam(kParamCycles)->InitDouble("Cycles", 1., 0., 24., 0.01);
  GetParam(kParamQ)->InitPercentage("Q", 50.);
  GetParam(kParamBallade)->InitPercentage("Ballade", 0.);
  GetParam(kParamHorizon)->InitPercentage("Horizon", 50.);
  GetParam(kParamSkew)->InitDouble("Skew", 1., 0.1, 6., 0.01);
  GetParam(kParamShapeMode)->InitEnum("Forme", 0, 2, "", IParam::kFlagsNone, "", "Type", "Dessin");
  GetParam(kParamHarmonicInjection)->InitDouble("Injection", 0., 0., 100., 0.1, "%");
  GetParam(kParamDecayExponent)->InitDouble("Decroiss.", 1., 0.2, 1., 0.001);
  GetParam(kParamTempDrive)->InitDouble("Drive", 0., 0., 100., 0.1, "%");
  GetParam(kParamLimiterThreshold)->InitDouble("Limiteur", 0., -24., 0., 0.1, "dB");

  mDrawnShapeStorage.assign(128, 0.f);

#if IPLUG_EDITOR
  mMakeGraphicsFunc = [&]() {
    return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS,
                         GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT));
  };

  mLayoutFunc = [&](IGraphics* pGraphics) {
    pGraphics->AttachCornerResizer(EUIResizerMode::Scale, false);
    pGraphics->AttachPanelBackground(COLOR_GRAY);
    pGraphics->LoadFont("Roboto-Regular", ROBOTO_FN);

    const IVStyle knobStyle = DEFAULT_STYLE.WithLabelText(IText(11.f, COLOR_WHITE));
    const IVStyle bonusStyle = DEFAULT_STYLE.WithLabelText(IText(11.f, COLOR_WHITE))
                                             .WithColor(EVColor::kFG, IColor(255, 220, 160, 60))
                                             .WithColor(EVColor::kPR, IColor(255, 240, 190, 90));

    const IRECT bounds = pGraphics->GetBounds();

    // --- Rangee du haut : reglages generaux ---
    IRECT topRow = bounds.GetFromTop(100.f).GetPadded(-10.f);
    mParamControls[kParamFFTSize] = new IVMenuButtonControl(topRow.GetGridCell(0, 0, 1, 6).GetCentredInside(110.f, 44.f), kParamFFTSize, "FFT Size");
    pGraphics->AttachControl(mParamControls[kParamFFTSize]);
    mParamControls[kParamOverlap] = new IVMenuButtonControl(topRow.GetGridCell(0, 1, 1, 6).GetCentredInside(110.f, 44.f), kParamOverlap, "Overlap");
    pGraphics->AttachControl(mParamControls[kParamOverlap]);
    mParamControls[kParamHarmonicInjection] = new IVKnobControl(topRow.GetGridCell(0, 2, 1, 6).GetCentredInside(90.f), kParamHarmonicInjection, "Injection", bonusStyle);
    pGraphics->AttachControl(mParamControls[kParamHarmonicInjection]);
    mParamControls[kParamDecayExponent] = new IVKnobControl(topRow.GetGridCell(0, 3, 1, 6).GetCentredInside(90.f), kParamDecayExponent, "Decroiss.", bonusStyle);
    pGraphics->AttachControl(mParamControls[kParamDecayExponent]);
    mParamControls[kParamTempDrive] = new IVKnobControl(topRow.GetGridCell(0, 4, 1, 6).GetCentredInside(90.f), kParamTempDrive, "Drive", bonusStyle);
    pGraphics->AttachControl(mParamControls[kParamTempDrive]);

    IVStyle limiterStyle = DEFAULT_STYLE.WithLabelText(IText(12.f, COLOR_WHITE))
                                         .WithColor(EVColor::kFG, IColor(255, 200, 30, 30))
                                         .WithColor(EVColor::kPR, IColor(255, 230, 50, 50));
    mParamControls[kParamLimiterThreshold] = new IVKnobControl(topRow.GetGridCell(0, 5, 1, 6).GetCentredInside(90.f), kParamLimiterThreshold, "LIMITEUR", limiterStyle);
    pGraphics->AttachControl(mParamControls[kParamLimiterThreshold]);

    // --- Rangee des 6 parametres de courbe ---
    IRECT controlsRow = IRECT(bounds.L, bounds.T + 100.f, bounds.R, bounds.T + 230.f).GetPadded(-15.f);
    mParamControls[kParamCycles] = new IVKnobControl(controlsRow.GetGridCell(0, 0, 1, 6).GetCentredInside(90.f), kParamCycles, "Cycles", knobStyle);
    pGraphics->AttachControl(mParamControls[kParamCycles]);
    mParamControls[kParamQ] = new IVKnobControl(controlsRow.GetGridCell(0, 1, 1, 6).GetCentredInside(90.f), kParamQ, "Q", knobStyle);
    pGraphics->AttachControl(mParamControls[kParamQ]);
    mParamControls[kParamBallade] = new IVKnobControl(controlsRow.GetGridCell(0, 2, 1, 6).GetCentredInside(90.f), kParamBallade, "Ballade", knobStyle);
    pGraphics->AttachControl(mParamControls[kParamBallade]);
    mParamControls[kParamHorizon] = new IVKnobControl(controlsRow.GetGridCell(0, 3, 1, 6).GetCentredInside(90.f), kParamHorizon, "Horizon", knobStyle);
    pGraphics->AttachControl(mParamControls[kParamHorizon]);
    mParamControls[kParamSkew] = new IVKnobControl(controlsRow.GetGridCell(0, 4, 1, 6).GetCentredInside(90.f), kParamSkew, "Skew", knobStyle);
    pGraphics->AttachControl(mParamControls[kParamSkew]);
    mParamControls[kParamShapeMode] = new IVMenuButtonControl(controlsRow.GetGridCell(0, 5, 1, 6).GetCentredInside(120.f, 48.f), kParamShapeMode, "Forme");
    pGraphics->AttachControl(mParamControls[kParamShapeMode]);

    // --- Courbe : le reste de l'espace, aussi grand que possible ---
    IRECT curveArea = IRECT(bounds.L, bounds.T + 230.f, bounds.R, bounds.B).GetPadded(-20.f);
    mCurveView = new SpectralCurvePreviewControl(curveArea, [this](const float* data, int size) {
      mDrawnShapeStorage.assign(data, data + size);
      mEngine.SetDrawnShape(data, size);
      UpdateEngine();
    });
    pGraphics->AttachControl(mCurveView);
  };
#endif

#if IPLUG_DSP
  OnReset();
#endif
}

void SpectralDistortion::OnIdle()
{
#if IPLUG_DSP
  if (mCurveView && mCurveUIUpdated.exchange(false))
  {
    mCurveView->SetCurve(mCurveUIBuf, mCurveUISize);
    mCurveView->SetDirty(false);
  }
  if (mCurveView && mSpectrumUIUpdated.exchange(false))
  {
    mCurveView->SetSpectrumData(mSpectrumUIBuf, mSpectrumUISize, mAnalyzer.GetSampleRate(), mAnalyzer.GetFFTSize());
  }
#endif
}

void SpectralDistortion::SyncUIToState()
{
  for (int i = 0; i < kNumParams; i++)
    if (mParamControls[i])
      mParamControls[i]->SetValueFromDelegate(GetParam(i)->GetNormalized());

  if (!mCurveView) return;

  bool drawMode = (int)GetParam(kParamShapeMode)->Value() != 0;
  mCurveView->SetDrawMode(drawMode);

  if (!mDrawnShapeStorage.empty())
    mCurveView->SetDrawnShapeExternal(mDrawnShapeStorage.data(), (int)mDrawnShapeStorage.size());

#if IPLUG_DSP
  UpdateYAxisMarks();
#endif
}

void SpectralDistortion::ApplyAllState()
{
#if IPLUG_DSP
  UpdateFFTConfig();
  UpdateEngine();
  UpdateYAxisMarks();
  mLimiter.Init(GetSampleRate());
  mLimiter.SetThresholdDb((float)GetParam(kParamLimiterThreshold)->Value());
  mAnalyzer.Init(GetSampleRate());
#endif
}

#if IPLUG_DSP

void SpectralDistortion::UpdateFFTConfig()
{
  int fftSizeIdx = (int)GetParam(kParamFFTSize)->Value();
  int fftSize = 512 << fftSizeIdx;
  int overlapIdx = (int)GetParam(kParamOverlap)->Value();
  int overlap = (overlapIdx == 0) ? 2 : 4;

  std::lock_guard<std::mutex> lock(mEngineMutex);
  mDistortL.Init(fftSize, overlap, GetSampleRate());
  mDistortR.Init(fftSize, overlap, GetSampleRate());
}

void SpectralDistortion::UpdateEngine()
{
  mEngine.SetSize(512);
  mEngine.SetShapeMode((int)GetParam(kParamShapeMode)->Value() == 0
                          ? SpectralCurveEngine::ShapeMode::Type
                          : SpectralCurveEngine::ShapeMode::Draw);
  mEngine.SetCycles((float)GetParam(kParamCycles)->Value());
  mEngine.SetQ((float)(GetParam(kParamQ)->Value() / 100.0));
  mEngine.SetBallade((float)(GetParam(kParamBallade)->Value() / 100.0));
  mEngine.SetHorizon((float)(GetParam(kParamHorizon)->Value() / 100.0));
  mEngine.SetSkew((float)GetParam(kParamSkew)->Value());
  mEngine.RebuildIfNeeded();

  const float* curve = mEngine.GetCurve();
  int size = mEngine.GetSize();

  {
    std::lock_guard<std::mutex> lock(mCurveMutex);
    mSharedCurve.assign(curve, curve + size);
  }
  mCurveUISize = size;
  for (int i = 0; i < size; i++) mCurveUIBuf[i] = curve[i];
  mCurveUIUpdated.store(true);
}

void SpectralDistortion::UpdateYAxisMarks()
{
  if (!mCurveView) return;
  std::vector<SpectralCurvePreviewControl::AxisMark> marks;
  marks.push_back({ -1.f, "Exp 0.125", false });
  marks.push_back({ 0.f, "Exp 1 (neutre)", true });
  marks.push_back({ 1.f, "Exp 8", false });
  mCurveView->SetYAxisMarks(marks);
}

void SpectralDistortion::OnReset()
{
  ApplyAllState();
}

void SpectralDistortion::OnParamChange(int paramIdx)
{
  switch (paramIdx)
  {
    case kParamFFTSize:
    case kParamOverlap:
      UpdateFFTConfig();
      break;

    case kParamShapeMode:
      if (mCurveView) mCurveView->SetDrawMode((int)GetParam(kParamShapeMode)->Value() != 0);
      UpdateEngine();
      break;

    case kParamCycles:
    case kParamQ:
    case kParamBallade:
    case kParamHorizon:
    case kParamSkew:
      UpdateEngine();
      break;

    case kParamLimiterThreshold:
      mLimiter.SetThresholdDb((float)GetParam(kParamLimiterThreshold)->Value());
      break;

    default:
      break;
  }
}

void SpectralDistortion::ProcessBlock(sample** inputs, sample** outputs, int nFrames)
{
  static float bufL[8192], bufR[8192], outL[8192], outR[8192];
  int n = std::min(nFrames, 8192);

  for (int i = 0; i < n; i++) { bufL[i] = (float)inputs[0][i]; bufR[i] = (float)inputs[1][i]; }

  static float bufMix[8192];
  for (int i = 0; i < n; i++) bufMix[i] = (bufL[i] + bufR[i]) * 0.5f;
  mAnalyzer.Process(bufMix, n);
  {
    std::lock_guard<std::mutex> lock(mSpectrumMutex);
    int numBins = std::min(mAnalyzer.GetNumBins(), 1100);
    for (int i = 0; i < numBins; i++) mSpectrumUIBuf[i] = mAnalyzer.GetMagnitudeDb()[i];
    mSpectrumUISize = numBins;
  }
  mSpectrumUIUpdated.store(true);

  float injection = (float)(GetParam(kParamHarmonicInjection)->Value() / 100.0);
  float decayExponent = (float)GetParam(kParamDecayExponent)->Value();
  float driveRaw = (float)(GetParam(kParamTempDrive)->Value() / 100.0);
  float drive;
  {
    constexpr float kSplitKnob = 0.75f, kSplitValue = 0.15f, kExpPower = 2.5f;
    if (driveRaw <= kSplitKnob) drive = (driveRaw / kSplitKnob) * kSplitValue;
    else { float s = (driveRaw - kSplitKnob) / (1.f - kSplitKnob); drive = kSplitValue + (1.f - kSplitValue) * std::pow(s, kExpPower); }
  }

  {
    std::lock_guard<std::mutex> curveLock(mCurveMutex);
    std::lock_guard<std::mutex> engineLock(mEngineMutex);

    mDistortL.SetCurve(mSharedCurve.data(), (int)mSharedCurve.size());
    mDistortR.SetCurve(mSharedCurve.data(), (int)mSharedCurve.size());
    mDistortL.SetHarmonicInjection(injection); mDistortR.SetHarmonicInjection(injection);
    mDistortL.SetDecayExponent(decayExponent); mDistortR.SetDecayExponent(decayExponent);
    mDistortL.SetTempDrive(drive); mDistortR.SetTempDrive(drive);

    mDistortL.Process(bufL, outL, n);
    mDistortR.Process(bufR, outR, n);
  }

  mLimiter.ProcessStereo(outL, outR, n);

  for (int i = 0; i < n; i++) { outputs[0][i] = outL[i]; outputs[1][i] = outR[i]; }
}

#endif
