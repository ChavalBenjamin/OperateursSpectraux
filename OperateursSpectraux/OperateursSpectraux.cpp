#include "OperateursSpectraux.h"
#include "IPlug_include_in_plug_src.h"
#include "IControls.h"
#include <cmath>
#include <string>

OperateursSpectraux::OperateursSpectraux(const InstanceInfo& info)
: iplug::Plugin(info, MakeConfig(kNumParams, 1))
{
  GetParam(kParamFFTSize)->InitEnum("FFT Size", 2 /*default = 2048*/, 5, "",
                                     IParam::kFlagsNone, "", "512", "1024", "2048", "4096", "8192");
  GetParam(kParamOverlap)->InitEnum("Overlap", 1 /*default = 4x*/, 2, "",
                                     IParam::kFlagsNone, "", "2x (50%)", "4x (75%)");
  GetParam(kParamCycles)->InitDouble("Cycles", 1., 0., 24., 0.01);
  GetParam(kParamQ)->InitPercentage("Q", 50.);
  GetParam(kParamBallade)->InitPercentage("Ballade", 0.);
  GetParam(kParamHorizon)->InitPercentage("Horizon", 50.);
  GetParam(kParamSkew)->InitDouble("Skew", 1., 0.1, 6., 0.01);
  GetParam(kParamShapeMode)->InitEnum("Forme", 0, 2, "", IParam::kFlagsNone, "", "Type", "Dessin");
  GetParam(kParamRatio)->InitDouble("Ratio", 0.5, 0.02, 1., 0.01);
  GetParam(kParamRelease)->InitDouble("Release", 80., 5., 2000., 1., "ms");
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

    const IVStyle knobStyle = DEFAULT_STYLE.WithLabelText(IText(10.f, COLOR_WHITE));

    const IRECT bounds = pGraphics->GetBounds();
    IRECT topRow = bounds.GetFromTop(60.f).GetPadded(-10.f);
    mParamControls[kParamFFTSize] = new IVMenuButtonControl(topRow.GetGridCell(0, 0, 1, 5).GetCentredInside(130.f, 40.f), kParamFFTSize, "FFT Size");
    pGraphics->AttachControl(mParamControls[kParamFFTSize]);
    mParamControls[kParamOverlap] = new IVMenuButtonControl(topRow.GetGridCell(0, 1, 1, 5).GetCentredInside(130.f, 40.f), kParamOverlap, "Overlap");
    pGraphics->AttachControl(mParamControls[kParamOverlap]);
    mParamControls[kParamRatio] = new IVKnobControl(topRow.GetGridCell(0, 2, 1, 5).GetCentredInside(50.f), kParamRatio, "Ratio", knobStyle);
    pGraphics->AttachControl(mParamControls[kParamRatio]);
    mParamControls[kParamRelease] = new IVKnobControl(topRow.GetGridCell(0, 3, 1, 5).GetCentredInside(50.f), kParamRelease, "Release", knobStyle);
    pGraphics->AttachControl(mParamControls[kParamRelease]);
    mParamControls[kParamLimiterThreshold] = new IVKnobControl(topRow.GetGridCell(0, 4, 1, 5).GetCentredInside(50.f), kParamLimiterThreshold, "Limiteur", knobStyle);
    pGraphics->AttachControl(mParamControls[kParamLimiterThreshold]);

    IRECT controlsRow = IRECT(bounds.L, bounds.T + 60.f, bounds.R, bounds.T + 180.f).GetPadded(-15.f);
    mParamControls[kParamCycles] = new IVKnobControl(controlsRow.GetGridCell(0, 0, 1, 6).GetCentredInside(80.f), kParamCycles, "Cycles", knobStyle);
    pGraphics->AttachControl(mParamControls[kParamCycles]);
    mParamControls[kParamQ] = new IVKnobControl(controlsRow.GetGridCell(0, 1, 1, 6).GetCentredInside(80.f), kParamQ, "Q", knobStyle);
    pGraphics->AttachControl(mParamControls[kParamQ]);
    mParamControls[kParamBallade] = new IVKnobControl(controlsRow.GetGridCell(0, 2, 1, 6).GetCentredInside(80.f), kParamBallade, "Ballade", knobStyle);
    pGraphics->AttachControl(mParamControls[kParamBallade]);
    mParamControls[kParamHorizon] = new IVKnobControl(controlsRow.GetGridCell(0, 3, 1, 6).GetCentredInside(80.f), kParamHorizon, "Horizon", knobStyle);
    pGraphics->AttachControl(mParamControls[kParamHorizon]);
    mParamControls[kParamSkew] = new IVKnobControl(controlsRow.GetGridCell(0, 4, 1, 6).GetCentredInside(80.f), kParamSkew, "Skew", knobStyle);
    pGraphics->AttachControl(mParamControls[kParamSkew]);
    mParamControls[kParamShapeMode] = new IVMenuButtonControl(controlsRow.GetGridCell(0, 5, 1, 6).GetCentredInside(120.f, 40.f), kParamShapeMode, "Forme");
    pGraphics->AttachControl(mParamControls[kParamShapeMode]);

    IRECT curveArea = IRECT(bounds.L, bounds.T + 180.f, bounds.R, bounds.B).GetPadded(-20.f);
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

void OperateursSpectraux::OnIdle()
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

void OperateursSpectraux::SyncUIToState()
{
  for (int i = 0; i < kNumParams; i++)
  {
    if (mParamControls[i])
      mParamControls[i]->SetValueFromDelegate(GetParam(i)->GetNormalized());
  }

  if (!mCurveView) return;

  bool drawMode = (int)GetParam(kParamShapeMode)->Value() != 0;
  mCurveView->SetDrawMode(drawMode);

  if (!mDrawnShapeStorage.empty())
    mCurveView->SetDrawnShapeExternal(mDrawnShapeStorage.data(), (int)mDrawnShapeStorage.size());

#if IPLUG_DSP
  UpdateYAxisMarks();
#endif
}

void OperateursSpectraux::ApplyAllState()
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

void OperateursSpectraux::UpdateFFTConfig()
{
  int fftSizeIdx = (int)GetParam(kParamFFTSize)->Value();
  int fftSize = 512 << fftSizeIdx;

  int overlapIdx = (int)GetParam(kParamOverlap)->Value();
  int overlap = (overlapIdx == 0) ? 2 : 4;

  mCompL.Init(fftSize, overlap, GetSampleRate());
  mCompR.Init(fftSize, overlap, GetSampleRate());
}

void OperateursSpectraux::UpdateEngine()
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
  for (int i = 0; i < size; i++)
    mCurveUIBuf[i] = curve[i];
  mCurveUIUpdated.store(true);
}

void OperateursSpectraux::UpdateYAxisMarks()
{
  if (!mCurveView) return;

  std::vector<SpectralCurvePreviewControl::AxisMark> marks;
  // Seuil (-60dB..0dB, mappe lineairement sur -1..1 - meme formule que le moteur).
  const float dbMarks[] = { -60.f, -45.f, -30.f, -15.f, 0.f };
  for (int i = 0; i < 5; i++)
  {
    float value = (dbMarks[i] + 30.f) / 30.f; // inverse de curveVal*30-30
    char buf[16];
    snprintf(buf, sizeof(buf), "%.0fdB", dbMarks[i]);
    marks.push_back({ value, buf, i == 2 });
  }

  mCurveView->SetYAxisMarks(marks);
}

void OperateursSpectraux::OnReset()
{
  ApplyAllState();
}

void OperateursSpectraux::OnParamChange(int paramIdx)
{
  switch (paramIdx)
  {
    case kParamFFTSize:
    case kParamOverlap:
      UpdateFFTConfig();
      break;

    case kParamShapeMode:
      if (mCurveView)
        mCurveView->SetDrawMode((int)GetParam(kParamShapeMode)->Value() != 0);
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

void OperateursSpectraux::ProcessBlock(sample** inputs, sample** outputs, int nFrames)
{
  static float bufL[8192], bufR[8192], outL[8192], outR[8192];
  int n = std::min(nFrames, 8192);

  for (int i = 0; i < n; i++)
  {
    bufL[i] = (float)inputs[0][i];
    bufR[i] = (float)inputs[1][i];
  }

  static float bufMix[8192];
  for (int i = 0; i < n; i++)
    bufMix[i] = (bufL[i] + bufR[i]) * 0.5f;
  mAnalyzer.Process(bufMix, n);
  {
    std::lock_guard<std::mutex> lock(mSpectrumMutex);
    int numBins = std::min(mAnalyzer.GetNumBins(), 1100);
    for (int i = 0; i < numBins; i++)
      mSpectrumUIBuf[i] = mAnalyzer.GetMagnitudeDb()[i];
    mSpectrumUISize = numBins;
  }
  mSpectrumUIUpdated.store(true);

  {
    std::lock_guard<std::mutex> lock(mCurveMutex);
    mCompL.SetCurve(mSharedCurve.data(), (int)mSharedCurve.size());
    mCompR.SetCurve(mSharedCurve.data(), (int)mSharedCurve.size());
  }

  float ratio = (float)GetParam(kParamRatio)->Value();
  float releaseMs = (float)GetParam(kParamRelease)->Value();
  mCompL.SetRatio(ratio);
  mCompR.SetRatio(ratio);
  mCompL.SetReleaseMs(releaseMs);
  mCompR.SetReleaseMs(releaseMs);

  mCompL.Process(bufL, outL, n);
  mCompR.Process(bufR, outR, n);

  // Limiteur Brickwall - indispensable ici, l'Anti-Comp peut ecarter la
  // dynamique de facon tres agressive.
  mLimiter.ProcessStereo(outL, outR, n);

  for (int i = 0; i < n; i++)
  {
    outputs[0][i] = outL[i];
    outputs[1][i] = outR[i];
  }
}

#endif
