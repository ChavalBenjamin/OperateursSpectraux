#include "OperateursSpectraux.h"
#include "IPlug_include_in_plug_src.h"
#include "IControls.h"
#include <cmath>
#include <string>

OperateursSpectraux::OperateursSpectraux(const InstanceInfo& info)
: iplug::Plugin(info, MakeConfig(kNumParams, 1))
{
  GetParam(kParamFFTSize)->InitEnum("FFT Size", 2, 5, "", IParam::kFlagsNone, "", "512", "1024", "2048", "4096", "8192");
  GetParam(kParamOverlap)->InitEnum("Overlap", 1, 2, "", IParam::kFlagsNone, "", "2x (50%)", "4x (75%)");
  GetParam(kParamRouting)->InitEnum("Routage", 0, 7, "", IParam::kFlagsNone, "",
                                     "Filtre>Delay>Disto", "Filtre>Disto>Delay",
                                     "Delay>Filtre>Disto", "Delay>Disto>Filtre",
                                     "Disto>Filtre>Delay", "Disto>Delay>Filtre",
                                     "Parallele");

  GetParam(kParamFilterEnable)->InitBool("Filtre On", true);
  GetParam(kParamFilterCycles)->InitDouble("F.Cycles", 1., 0., 24., 0.01);
  GetParam(kParamFilterQ)->InitPercentage("F.Q", 50.);
  GetParam(kParamFilterBallade)->InitPercentage("F.Ballade", 0.);
  GetParam(kParamFilterHorizon)->InitPercentage("F.Horizon", 50.);
  GetParam(kParamFilterSkew)->InitDouble("F.Skew", 1., 0.1, 6., 0.01);
  GetParam(kParamFilterShapeMode)->InitEnum("F.Forme", 0, 2, "", IParam::kFlagsNone, "", "Type", "Dessin");

  GetParam(kParamDelayEnable)->InitBool("Delay On", false);
  GetParam(kParamDelayCycles)->InitDouble("D.Cycles", 1., 0., 24., 0.01);
  GetParam(kParamDelayQ)->InitPercentage("D.Q", 50.);
  GetParam(kParamDelayBallade)->InitPercentage("D.Ballade", 0.);
  GetParam(kParamDelayHorizon)->InitPercentage("D.Horizon", 50.);
  GetParam(kParamDelaySkew)->InitDouble("D.Skew", 1., 0.1, 6., 0.01);
  GetParam(kParamDelayShapeMode)->InitEnum("D.Forme", 0, 2, "", IParam::kFlagsNone, "", "Type", "Dessin");
  GetParam(kParamDelayFeedback)->InitDouble("D.Feedback", 0., 0., 95., 0.1, "%");
  GetParam(kParamDelaySyncMode)->InitBool("D.Sync BPM", false);

  GetParam(kParamDistoEnable)->InitBool("Disto On", false);
  GetParam(kParamDistoCycles)->InitDouble("X.Cycles", 1., 0., 24., 0.01);
  GetParam(kParamDistoQ)->InitPercentage("X.Q", 50.);
  GetParam(kParamDistoBallade)->InitPercentage("X.Ballade", 0.);
  GetParam(kParamDistoHorizon)->InitPercentage("X.Horizon", 50.);
  GetParam(kParamDistoSkew)->InitDouble("X.Skew", 1., 0.1, 6., 0.01);
  GetParam(kParamDistoShapeMode)->InitEnum("X.Forme", 0, 2, "", IParam::kFlagsNone, "", "Type", "Dessin");
  GetParam(kParamDistoInjection)->InitDouble("X.Injection", 0., 0., 100., 0.1, "%");
  GetParam(kParamDistoDecay)->InitDouble("X.Decroiss.", 1., 0.2, 1., 0.001);
  GetParam(kParamDistoDrive)->InitDouble("X.Drive", 0., 0., 100., 0.1, "%");

  GetParam(kParamLimiterThreshold)->InitDouble("Limiteur", 0., -24., 0., 0.1, "dB");

  mFilterDrawnShape.assign(128, 0.f);
  mDelayDrawnShape.assign(128, 0.f);
  mDistoDrawnShape.assign(128, 0.f);

#if IPLUG_EDITOR
  mMakeGraphicsFunc = [&]() {
    return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS,
                         GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT));
  };

  mLayoutFunc = [&](IGraphics* pGraphics) {
    pGraphics->AttachCornerResizer(EUIResizerMode::Scale, false);
    pGraphics->AttachPanelBackground(COLOR_GRAY);
    pGraphics->LoadFont("Roboto-Regular", ROBOTO_FN);

    const IVStyle knobStyle = DEFAULT_STYLE.WithLabelText(IText(9.f, COLOR_WHITE));
    const IColor kFilterBg(255, 60, 30, 40);   // rose terne
    const IColor kDelayBg(255, 25, 55, 55);    // vert-bleu actuel
    const IColor kDistoBg(255, 60, 25, 25);    // rouge terne

    const IRECT bounds = pGraphics->GetBounds();

    // --- Zone generale ---
    IRECT generalRow = bounds.GetFromTop(70.f).GetPadded(-8.f);
    mParamControls[kParamFFTSize] = new IVMenuButtonControl(generalRow.GetGridCell(0, 0, 1, 4).GetCentredInside(100.f, 36.f), kParamFFTSize, "FFT Size");
    pGraphics->AttachControl(mParamControls[kParamFFTSize]);
    mParamControls[kParamOverlap] = new IVMenuButtonControl(generalRow.GetGridCell(0, 1, 1, 4).GetCentredInside(100.f, 36.f), kParamOverlap, "Overlap");
    pGraphics->AttachControl(mParamControls[kParamOverlap]);
    mParamControls[kParamRouting] = new IVMenuButtonControl(generalRow.GetGridCell(0, 2, 1, 4).GetCentredInside(160.f, 36.f), kParamRouting, "Routage");
    pGraphics->AttachControl(mParamControls[kParamRouting]);

    // Limiteur : gros bouton ROUGE, en bout de chaine, bien visible.
    IVStyle limiterStyle = DEFAULT_STYLE.WithLabelText(IText(11.f, COLOR_WHITE))
                                         .WithColor(EVColor::kFG, IColor(255, 200, 30, 30))
                                         .WithColor(EVColor::kPR, IColor(255, 230, 50, 50));
    mParamControls[kParamLimiterThreshold] = new IVKnobControl(generalRow.GetGridCell(0, 3, 1, 4).GetCentredInside(60.f), kParamLimiterThreshold, "LIMITEUR", limiterStyle);
    pGraphics->AttachControl(mParamControls[kParamLimiterThreshold]);

    float zoneH = (bounds.H() - 70.f) / 3.f;
    float zoneTop = bounds.T + 70.f;

    // --- Zone Filtre (rose terne) ---
    IRECT filterZone(bounds.L, zoneTop, bounds.R, zoneTop + zoneH);
    pGraphics->AttachControl(new IPanelControl(filterZone, kFilterBg));
    {
      IRECT row1 = filterZone.GetFromTop(36.f).GetPadded(-6.f);
      mParamControls[kParamFilterEnable] = new IVToggleControl(row1.GetFromLeft(90.f), kParamFilterEnable, "Filtre On/Off");
      pGraphics->AttachControl(mParamControls[kParamFilterEnable]);

      IRECT row2 = IRECT(filterZone.L, row1.B, filterZone.R, row1.B + 70.f).GetPadded(-6.f);
      const char* labels[6] = { "Cycles", "Q", "Ballade", "Horizon", "Skew", "Forme" };
      int ids[6] = { kParamFilterCycles, kParamFilterQ, kParamFilterBallade, kParamFilterHorizon, kParamFilterSkew, kParamFilterShapeMode };
      for (int i = 0; i < 6; i++)
      {
        if (i == 5)
          mParamControls[ids[i]] = new IVMenuButtonControl(row2.GetGridCell(0, i, 1, 6).GetCentredInside(70.f, 30.f), ids[i], labels[i]);
        else
          mParamControls[ids[i]] = new IVKnobControl(row2.GetGridCell(0, i, 1, 6).GetCentredInside(46.f), ids[i], labels[i], knobStyle);
        pGraphics->AttachControl(mParamControls[ids[i]]);
      }

      IRECT curveArea = IRECT(filterZone.L, row2.B, filterZone.R, filterZone.B).GetPadded(-10.f);
      mFilterCurveView = new SpectralCurvePreviewControl(curveArea, [this](const float* data, int size) {
        mFilterDrawnShape.assign(data, data + size);
        mFilterEngine.SetDrawnShape(data, size);
        UpdateFilterCurve();
      });
      pGraphics->AttachControl(mFilterCurveView);
    }

    // --- Zone Delay (vert-bleu) ---
    IRECT delayZone(bounds.L, zoneTop + zoneH, bounds.R, zoneTop + 2.f * zoneH);
    pGraphics->AttachControl(new IPanelControl(delayZone, kDelayBg));
    {
      IRECT row1 = delayZone.GetFromTop(36.f).GetPadded(-6.f);
      mParamControls[kParamDelayEnable] = new IVToggleControl(row1.GetFromLeft(90.f), kParamDelayEnable, "Delay On/Off");
      pGraphics->AttachControl(mParamControls[kParamDelayEnable]);
      mParamControls[kParamDelayFeedback] = new IVKnobControl(row1.GetFromRight(180.f).GetFromLeft(70.f), kParamDelayFeedback, "Feedback", knobStyle);
      pGraphics->AttachControl(mParamControls[kParamDelayFeedback]);
      mParamControls[kParamDelaySyncMode] = new IVToggleControl(row1.GetFromRight(90.f), kParamDelaySyncMode, "Sync BPM");
      pGraphics->AttachControl(mParamControls[kParamDelaySyncMode]);

      IRECT row2 = IRECT(delayZone.L, row1.B, delayZone.R, row1.B + 70.f).GetPadded(-6.f);
      const char* labels[6] = { "Cycles", "Q", "Ballade", "Horizon", "Skew", "Forme" };
      int ids[6] = { kParamDelayCycles, kParamDelayQ, kParamDelayBallade, kParamDelayHorizon, kParamDelaySkew, kParamDelayShapeMode };
      for (int i = 0; i < 6; i++)
      {
        if (i == 5)
          mParamControls[ids[i]] = new IVMenuButtonControl(row2.GetGridCell(0, i, 1, 6).GetCentredInside(70.f, 30.f), ids[i], labels[i]);
        else
          mParamControls[ids[i]] = new IVKnobControl(row2.GetGridCell(0, i, 1, 6).GetCentredInside(46.f), ids[i], labels[i], knobStyle);
        pGraphics->AttachControl(mParamControls[ids[i]]);
      }

      IRECT curveArea = IRECT(delayZone.L, row2.B, delayZone.R, delayZone.B).GetPadded(-10.f);
      mDelayCurveView = new SpectralCurvePreviewControl(curveArea, [this](const float* data, int size) {
        mDelayDrawnShape.assign(data, data + size);
        mDelayEngine.SetDrawnShape(data, size);
        UpdateDelayCurve();
      });
      pGraphics->AttachControl(mDelayCurveView);
    }

    // --- Zone Distorsion (rouge terne) ---
    IRECT distoZone(bounds.L, zoneTop + 2.f * zoneH, bounds.R, bounds.B);
    pGraphics->AttachControl(new IPanelControl(distoZone, kDistoBg));
    {
      IRECT row1 = distoZone.GetFromTop(36.f).GetPadded(-6.f);
      mParamControls[kParamDistoEnable] = new IVToggleControl(row1.GetFromLeft(90.f), kParamDistoEnable, "Disto On/Off");
      pGraphics->AttachControl(mParamControls[kParamDistoEnable]);
      mParamControls[kParamDistoInjection] = new IVKnobControl(row1.GetFromRight(270.f).GetFromLeft(70.f), kParamDistoInjection, "Injection", knobStyle);
      pGraphics->AttachControl(mParamControls[kParamDistoInjection]);
      mParamControls[kParamDistoDecay] = new IVKnobControl(row1.GetFromRight(180.f).GetFromLeft(70.f), kParamDistoDecay, "Decroiss.", knobStyle);
      pGraphics->AttachControl(mParamControls[kParamDistoDecay]);
      mParamControls[kParamDistoDrive] = new IVKnobControl(row1.GetFromRight(90.f), kParamDistoDrive, "Drive", knobStyle);
      pGraphics->AttachControl(mParamControls[kParamDistoDrive]);

      IRECT row2 = IRECT(distoZone.L, row1.B, distoZone.R, row1.B + 70.f).GetPadded(-6.f);
      const char* labels[6] = { "Cycles", "Q", "Ballade", "Horizon", "Skew", "Forme" };
      int ids[6] = { kParamDistoCycles, kParamDistoQ, kParamDistoBallade, kParamDistoHorizon, kParamDistoSkew, kParamDistoShapeMode };
      for (int i = 0; i < 6; i++)
      {
        if (i == 5)
          mParamControls[ids[i]] = new IVMenuButtonControl(row2.GetGridCell(0, i, 1, 6).GetCentredInside(70.f, 30.f), ids[i], labels[i]);
        else
          mParamControls[ids[i]] = new IVKnobControl(row2.GetGridCell(0, i, 1, 6).GetCentredInside(46.f), ids[i], labels[i], knobStyle);
        pGraphics->AttachControl(mParamControls[ids[i]]);
      }

      IRECT curveArea = IRECT(distoZone.L, row2.B, distoZone.R, distoZone.B).GetPadded(-10.f);
      mDistoCurveView = new SpectralCurvePreviewControl(curveArea, [this](const float* data, int size) {
        mDistoDrawnShape.assign(data, data + size);
        mDistoEngine.SetDrawnShape(data, size);
        UpdateDistoCurve();
      });
      pGraphics->AttachControl(mDistoCurveView);
    }
  };
#endif

#if IPLUG_DSP
  OnReset();
#endif
}

void OperateursSpectraux::OnIdle()
{
#if IPLUG_DSP
  if (mFilterCurveView && mFilterCurveUIUpdated.exchange(false))
  {
    mFilterCurveView->SetCurve(mFilterCurveUIBuf, mFilterCurveUISize);
    mFilterCurveView->SetDirty(false);
  }
  if (mDelayCurveView && mDelayCurveUIUpdated.exchange(false))
  {
    mDelayCurveView->SetCurve(mDelayCurveUIBuf, mDelayCurveUISize);
    mDelayCurveView->SetDirty(false);
  }
  if (mDistoCurveView && mDistoCurveUIUpdated.exchange(false))
  {
    mDistoCurveView->SetCurve(mDistoCurveUIBuf, mDistoCurveUISize);
    mDistoCurveView->SetDirty(false);
  }
  if (mFilterCurveView && mSpectrumUIUpdated.exchange(false))
  {
    mFilterCurveView->SetSpectrumData(mSpectrumUIBuf, mSpectrumUISize, mAnalyzer.GetSampleRate(), mAnalyzer.GetFFTSize());
    if (mDelayCurveView) mDelayCurveView->SetSpectrumData(mSpectrumUIBuf, mSpectrumUISize, mAnalyzer.GetSampleRate(), mAnalyzer.GetFFTSize());
    if (mDistoCurveView) mDistoCurveView->SetSpectrumData(mSpectrumUIBuf, mSpectrumUISize, mAnalyzer.GetSampleRate(), mAnalyzer.GetFFTSize());
  }
#endif
}

void OperateursSpectraux::SyncUIToState()
{
  for (int i = 0; i < kNumParams; i++)
    if (mParamControls[i])
      mParamControls[i]->SetValueFromDelegate(GetParam(i)->GetNormalized());

  if (mFilterCurveView)
  {
    mFilterCurveView->SetDrawMode((int)GetParam(kParamFilterShapeMode)->Value() != 0);
    if (!mFilterDrawnShape.empty())
      mFilterCurveView->SetDrawnShapeExternal(mFilterDrawnShape.data(), (int)mFilterDrawnShape.size());
  }
  if (mDelayCurveView)
  {
    mDelayCurveView->SetDrawMode((int)GetParam(kParamDelayShapeMode)->Value() != 0);
    if (!mDelayDrawnShape.empty())
      mDelayCurveView->SetDrawnShapeExternal(mDelayDrawnShape.data(), (int)mDelayDrawnShape.size());
  }
  if (mDistoCurveView)
  {
    mDistoCurveView->SetDrawMode((int)GetParam(kParamDistoShapeMode)->Value() != 0);
    if (!mDistoDrawnShape.empty())
      mDistoCurveView->SetDrawnShapeExternal(mDistoDrawnShape.data(), (int)mDistoDrawnShape.size());
  }

#if IPLUG_DSP
  UpdateDelayYAxisMarks();
  UpdateDistoYAxisMarks();
#endif
}

void OperateursSpectraux::ApplyAllState()
{
#if IPLUG_DSP
  UpdateFFTConfig();
  UpdateFilterCurve();
  UpdateDelayCurve();
  UpdateDistoCurve();
  UpdateDelayYAxisMarks();
  UpdateDistoYAxisMarks();
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

  std::lock_guard<std::mutex> lock(mEngineMutex);
  mFilterL.Init(fftSize, overlap); mFilterL.SetSampleRate(GetSampleRate());
  mFilterR.Init(fftSize, overlap); mFilterR.SetSampleRate(GetSampleRate());
  mDelayL.Init(fftSize, overlap, GetSampleRate());
  mDelayR.Init(fftSize, overlap, GetSampleRate());
  mDistortL.Init(fftSize, overlap, GetSampleRate());
  mDistortR.Init(fftSize, overlap, GetSampleRate());
}

static void UpdateOneCurve(SpectralCurveEngine& engine, int paramCycles, int paramQ, int paramBallade,
                            int paramHorizon, int paramSkew, int paramShapeMode,
                            iplug::Plugin& plug, std::mutex& mtx, std::vector<float>& shared,
                            float* uiBuf, int& uiSize, std::atomic<bool>& uiFlag)
{
  engine.SetSize(512);
  engine.SetShapeMode((int)plug.GetParam(paramShapeMode)->Value() == 0
                         ? SpectralCurveEngine::ShapeMode::Type
                         : SpectralCurveEngine::ShapeMode::Draw);
  engine.SetCycles((float)plug.GetParam(paramCycles)->Value());
  engine.SetQ((float)(plug.GetParam(paramQ)->Value() / 100.0));
  engine.SetBallade((float)(plug.GetParam(paramBallade)->Value() / 100.0));
  engine.SetHorizon((float)(plug.GetParam(paramHorizon)->Value() / 100.0));
  engine.SetSkew((float)plug.GetParam(paramSkew)->Value());
  engine.RebuildIfNeeded();

  const float* curve = engine.GetCurve();
  int size = engine.GetSize();

  {
    std::lock_guard<std::mutex> lock(mtx);
    shared.assign(curve, curve + size);
  }
  uiSize = size;
  for (int i = 0; i < size; i++) uiBuf[i] = curve[i];
  uiFlag.store(true);
}

void OperateursSpectraux::UpdateFilterCurve()
{
  UpdateOneCurve(mFilterEngine, kParamFilterCycles, kParamFilterQ, kParamFilterBallade, kParamFilterHorizon,
                 kParamFilterSkew, kParamFilterShapeMode, *this, mFilterCurveMutex, mSharedFilterCurve,
                 mFilterCurveUIBuf, mFilterCurveUISize, mFilterCurveUIUpdated);
}

void OperateursSpectraux::UpdateDelayCurve()
{
  UpdateOneCurve(mDelayEngine, kParamDelayCycles, kParamDelayQ, kParamDelayBallade, kParamDelayHorizon,
                 kParamDelaySkew, kParamDelayShapeMode, *this, mDelayCurveMutex, mSharedDelayCurve,
                 mDelayCurveUIBuf, mDelayCurveUISize, mDelayCurveUIUpdated);
}

void OperateursSpectraux::UpdateDistoCurve()
{
  UpdateOneCurve(mDistoEngine, kParamDistoCycles, kParamDistoQ, kParamDistoBallade, kParamDistoHorizon,
                 kParamDistoSkew, kParamDistoShapeMode, *this, mDistoCurveMutex, mSharedDistoCurve,
                 mDistoCurveUIBuf, mDistoCurveUISize, mDistoCurveUIUpdated);
}

void OperateursSpectraux::UpdateDelayYAxisMarks()
{
  if (!mDelayCurveView) return;
  std::vector<SpectralCurvePreviewControl::AxisMark> marks;
  marks.push_back({ -1.f, "0 ms", false });
  marks.push_back({ 0.f, "1.25 s", true });
  marks.push_back({ 1.f, "2.5 s", false });
  mDelayCurveView->SetYAxisMarks(marks);
}

void OperateursSpectraux::UpdateDistoYAxisMarks()
{
  if (!mDistoCurveView) return;
  std::vector<SpectralCurvePreviewControl::AxisMark> marks;
  marks.push_back({ -1.f, "Exp 0.125", false });
  marks.push_back({ 0.f, "Exp 1 (neutre)", true });
  marks.push_back({ 1.f, "Exp 8", false });
  mDistoCurveView->SetYAxisMarks(marks);
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

    case kParamFilterShapeMode:
      if (mFilterCurveView) mFilterCurveView->SetDrawMode((int)GetParam(kParamFilterShapeMode)->Value() != 0);
      UpdateFilterCurve();
      break;
    case kParamFilterCycles: case kParamFilterQ: case kParamFilterBallade:
    case kParamFilterHorizon: case kParamFilterSkew:
      UpdateFilterCurve();
      break;

    case kParamDelayShapeMode:
      if (mDelayCurveView) mDelayCurveView->SetDrawMode((int)GetParam(kParamDelayShapeMode)->Value() != 0);
      UpdateDelayCurve();
      break;
    case kParamDelayCycles: case kParamDelayQ: case kParamDelayBallade:
    case kParamDelayHorizon: case kParamDelaySkew:
      UpdateDelayCurve();
      break;

    case kParamDistoShapeMode:
      if (mDistoCurveView) mDistoCurveView->SetDrawMode((int)GetParam(kParamDistoShapeMode)->Value() != 0);
      UpdateDistoCurve();
      break;
    case kParamDistoCycles: case kParamDistoQ: case kParamDistoBallade:
    case kParamDistoHorizon: case kParamDistoSkew:
      UpdateDistoCurve();
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
  static float bufL[8192], bufR[8192];
  static float stageL[8192], stageR[8192];
  static float outFL[8192], outFR[8192], outDL[8192], outDR[8192], outXL[8192], outXR[8192];
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

  bool filterOn = GetParam(kParamFilterEnable)->Value() != 0.;
  bool delayOn = GetParam(kParamDelayEnable)->Value() != 0.;
  bool distoOn = GetParam(kParamDistoEnable)->Value() != 0.;
  int routing = (int)GetParam(kParamRouting)->Value();

  {
    std::lock_guard<std::mutex> lockF(mFilterCurveMutex);
    std::lock_guard<std::mutex> lockD(mDelayCurveMutex);
    std::lock_guard<std::mutex> lockX(mDistoCurveMutex);
    std::lock_guard<std::mutex> lockE(mEngineMutex);

    mFilterL.SetCurve(mSharedFilterCurve.data(), (int)mSharedFilterCurve.size());
    mFilterR.SetCurve(mSharedFilterCurve.data(), (int)mSharedFilterCurve.size());

    mDelayL.SetCurve(mSharedDelayCurve.data(), (int)mSharedDelayCurve.size());
    mDelayR.SetCurve(mSharedDelayCurve.data(), (int)mSharedDelayCurve.size());
    float feedback = (float)(GetParam(kParamDelayFeedback)->Value() / 100.0);
    bool delaySync = GetParam(kParamDelaySyncMode)->Value() != 0.;
    mDelayL.SetFeedback(feedback); mDelayR.SetFeedback(feedback);
    mDelayL.SetSyncMode(delaySync); mDelayR.SetSyncMode(delaySync);
    double bpm = GetTempo(); // confirmee fonctionnelle (utilisee dans MagniPhase)
    mDelayL.SetBPM(bpm); mDelayR.SetBPM(bpm);

    mDistortL.SetCurve(mSharedDistoCurve.data(), (int)mSharedDistoCurve.size());
    mDistortR.SetCurve(mSharedDistoCurve.data(), (int)mSharedDistoCurve.size());
    float injection = (float)(GetParam(kParamDistoInjection)->Value() / 100.0);
    float decayExponent = (float)GetParam(kParamDistoDecay)->Value();
    float driveRaw = (float)(GetParam(kParamDistoDrive)->Value() / 100.0);
    float drive;
    {
      constexpr float kSplitKnob = 0.75f, kSplitValue = 0.15f, kExpPower = 2.5f;
      if (driveRaw <= kSplitKnob) drive = (driveRaw / kSplitKnob) * kSplitValue;
      else { float s = (driveRaw - kSplitKnob) / (1.f - kSplitKnob); drive = kSplitValue + (1.f - kSplitValue) * std::pow(s, kExpPower); }
    }
    mDistortL.SetHarmonicInjection(injection); mDistortR.SetHarmonicInjection(injection);
    mDistortL.SetDecayExponent(decayExponent); mDistortR.SetDecayExponent(decayExponent);
    mDistortL.SetTempDrive(drive); mDistortR.SetTempDrive(drive);

    if (routing == 6) // Parallele : les 3 traitent le MEME signal d'origine, sommes
    {
      if (filterOn) { mFilterL.Process(bufL, outFL, n); mFilterR.Process(bufR, outFR, n); }
      if (delayOn)  { mDelayL.Process(bufL, outDL, n); mDelayR.Process(bufR, outDR, n); }
      if (distoOn)  { mDistortL.Process(bufL, outXL, n); mDistortR.Process(bufR, outXR, n); }

      int activeCount = (filterOn ? 1 : 0) + (delayOn ? 1 : 0) + (distoOn ? 1 : 0);
      float norm = activeCount > 0 ? 1.f / std::sqrt((float)activeCount) : 1.f;

      for (int i = 0; i < n; i++)
      {
        float sL = 0.f, sR = 0.f;
        if (filterOn) { sL += outFL[i]; sR += outFR[i]; }
        if (delayOn)  { sL += outDL[i]; sR += outDR[i]; }
        if (distoOn)  { sL += outXL[i]; sR += outXR[i]; }
        stageL[i] = activeCount > 0 ? sL * norm : bufL[i];
        stageR[i] = activeCount > 0 ? sR * norm : bufR[i];
      }
    }
    else // Serie : un des 6 ordres, modules OFF simplement sautes (bypass)
    {
      static const int orders[6][3] = {
        {0,1,2}, {0,2,1}, {1,0,2}, {1,2,0}, {2,0,1}, {2,1,0}
      };
      std::copy(bufL, bufL + n, stageL);
      std::copy(bufR, bufR + n, stageR);

      for (int stage = 0; stage < 3; stage++)
      {
        int moduleIdx = orders[routing][stage];
        if (moduleIdx == 0 && filterOn) { mFilterL.Process(stageL, stageL, n); mFilterR.Process(stageR, stageR, n); }
        else if (moduleIdx == 1 && delayOn) { mDelayL.Process(stageL, stageL, n); mDelayR.Process(stageR, stageR, n); }
        else if (moduleIdx == 2 && distoOn) { mDistortL.Process(stageL, stageL, n); mDistortR.Process(stageR, stageR, n); }
      }
    }
  }

  mLimiter.ProcessStereo(stageL, stageR, n);

  for (int i = 0; i < n; i++) { outputs[0][i] = stageL[i]; outputs[1][i] = stageR[i]; }
}

#endif
