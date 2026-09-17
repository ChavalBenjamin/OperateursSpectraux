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
  GetParam(kParamFeedback)->InitDouble("Feedback", 0., 0., 150., 0.1, "%");
  GetParam(kParamSyncMode)->InitEnum("Sync", 0, 2, "", IParam::kFlagsNone, "", "Off", "On");
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
    mParamControls[kParamFeedback] = new IVKnobControl(topRow.GetGridCell(0, 2, 1, 5).GetCentredInside(50.f), kParamFeedback, "Feedback", knobStyle);
    pGraphics->AttachControl(mParamControls[kParamFeedback]);
    mParamControls[kParamSyncMode] = new IVMenuButtonControl(topRow.GetGridCell(0, 3, 1, 5).GetCentredInside(100.f, 40.f), kParamSyncMode, "Sync");
    pGraphics->AttachControl(mParamControls[kParamSyncMode]);
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

// A chaque (re)ouverture de la fenetre : les CONTROLES sont neufs (recrees
// par mLayoutFunc), meme si le moteur/parametres eux ont deja leur bon
// etat - il faut donc explicitement repousser mode dessin + courbe
// dessinee + grille Y vers cette nouvelle fenetre.
void OperateursSpectraux::SyncUIToState()
{
  // Force chaque potard/bouton a se resynchroniser visuellement depuis sa
  // vraie valeur - sans ca, apres restauration d'un projet, le son et le
  // texte affiche sont corrects mais la ROTATION visuelle reste a zero.
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

bool OperateursSpectraux::SerializeState(IByteChunk& chunk) const
{
  bool success = SerializeParams(chunk);

  int drawnShapeSize = (int)mDrawnShapeStorage.size();
  chunk.Put(&drawnShapeSize);
  for (float v : mDrawnShapeStorage)
    chunk.Put(&v);

  return success;
}

int OperateursSpectraux::UnserializeState(const IByteChunk& chunk, int startPos)
{
  int pos = UnserializeParams(chunk, startPos);

  int size = 0;
  pos = chunk.Get(&size, pos);
  mDrawnShapeStorage.resize(std::max(0, size));
  for (int i = 0; i < size; i++)
    pos = chunk.Get(&mDrawnShapeStorage[i], pos);

#if IPLUG_DSP
  if (!mDrawnShapeStorage.empty())
    mEngine.SetDrawnShape(mDrawnShapeStorage.data(), (int)mDrawnShapeStorage.size());

  // Les parametres restaures ne redeclenchent pas OnParamChange -
  // reconfigure donc TOUT explicitement (FFT/Overlap compris), sinon le
  // moteur reste sur son ancien etat par defaut malgre les boutons
  // affichant les bonnes valeurs.
  ApplyAllState();
#endif

  SyncUIToState();

  return pos;
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

  mDelayL.Init(fftSize, overlap, GetSampleRate());
  mDelayR.Init(fftSize, overlap, GetSampleRate());
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

  auto msToValue = [](float ms) {
    float t = std::pow(std::max(0.f, ms) / 2500.f, 1.f / 3.f);
    return 2.f * t - 1.f;
  };

  std::vector<SpectralCurvePreviewControl::AxisMark> marks;
  bool sync = (int)GetParam(kParamSyncMode)->Value() != 0;

  if (!sync)
  {
    const float msMarks[] = { 0.f, 20.f, 100.f, 500.f, 2500.f };
    const char* labels[] = { "0ms", "20ms", "100ms", "500ms", "2.5s" };
    for (int i = 0; i < 5; i++)
      marks.push_back({ msToValue(msMarks[i]), labels[i], i == 2 });
  }
  else
  {
    double bpm = GetTempo();
    if (bpm <= 0.0) bpm = 120.0;
    double quarterMs = 60000.0 / bpm;
    double wholeMs = quarterMs * 4.0;
    static const float divisors[] = { 1.f, 2.f, 4.f, 8.f, 16.f, 32.f, 64.f };
    static const char* names[] = { "1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/64" };
    for (int i = 0; i < 7; i++)
    {
      float straightMs = (float)(wholeMs / divisors[i]);
      if (straightMs >= 0.f && straightMs <= 2500.f)
        marks.push_back({ msToValue(straightMs), names[i], i == 2 });

      float tripletMs = straightMs * (2.f / 3.f);
      if (tripletMs >= 0.f && tripletMs <= 2500.f)
      {
        std::string tName = std::string(names[i]) + "T";
        marks.push_back({ msToValue(tripletMs), tName, false });
      }
    }
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

    case kParamSyncMode:
      UpdateYAxisMarks();
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

  // Analyseur de spectre (purement visuel) - alimente avec le signal
  // ENTRANT (avant traitement), mixe L+R, pour servir de reference pendant
  // qu'on sculpte la courbe.
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
    mDelayL.SetCurve(mSharedCurve.data(), (int)mSharedCurve.size());
    mDelayR.SetCurve(mSharedCurve.data(), (int)mSharedCurve.size());
  }

  float feedback = (float)(GetParam(kParamFeedback)->Value() / 100.0);
  bool sync = (int)GetParam(kParamSyncMode)->Value() != 0;
  double bpm = GetTempo();
  if (bpm <= 0.0) bpm = 120.0;

  mDelayL.SetFeedback(feedback);
  mDelayR.SetFeedback(feedback);
  mDelayL.SetSyncMode(sync);
  mDelayR.SetSyncMode(sync);
  mDelayL.SetBPM(bpm);
  mDelayR.SetBPM(bpm);

  mDelayL.Process(bufL, outL, n);
  mDelayR.Process(bufR, outR, n);

  mLimiter.ProcessStereo(outL, outR, n);

  for (int i = 0; i < n; i++)
  {
    outputs[0][i] = outL[i];
    outputs[1][i] = outR[i];
  }
}

#endif
