#include "OperateursSpectraux.h"
#include "IPlug_include_in_plug_src.h"
#include "IControls.h"

OperateursSpectraux::OperateursSpectraux(const InstanceInfo& info)
: iplug::Plugin(info, MakeConfig(kNumParams, 1))
{
  GetParam(kParamCycles)->InitDouble("Cycles", 1., 0., 24., 0.01);
  GetParam(kParamQ)->InitPercentage("Q", 50.); // 50% = milieu = forme pleine par defaut
  GetParam(kParamBallade)->InitPercentage("Ballade", 0.);
  GetParam(kParamHorizon)->InitPercentage("Horizon", 50.); // 50% = symetrique par defaut
  GetParam(kParamSkew)->InitDouble("Skew", 1., 0.1, 6., 0.01);
  GetParam(kParamShapeMode)->InitEnum("Forme", 0, 2, "", IParam::kFlagsNone, "", "Type", "Dessin");

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
    IRECT controlsRow = bounds.GetFromTop(120.f).GetPadded(-15.f);

    pGraphics->AttachControl(new IVKnobControl(controlsRow.GetGridCell(0, 0, 1, 6).GetCentredInside(80.f), kParamCycles, "Cycles", knobStyle));
    pGraphics->AttachControl(new IVKnobControl(controlsRow.GetGridCell(0, 1, 1, 6).GetCentredInside(80.f), kParamQ, "Q", knobStyle));
    pGraphics->AttachControl(new IVKnobControl(controlsRow.GetGridCell(0, 2, 1, 6).GetCentredInside(80.f), kParamBallade, "Ballade", knobStyle));
    pGraphics->AttachControl(new IVKnobControl(controlsRow.GetGridCell(0, 3, 1, 6).GetCentredInside(80.f), kParamHorizon, "Horizon", knobStyle));
    pGraphics->AttachControl(new IVKnobControl(controlsRow.GetGridCell(0, 4, 1, 6).GetCentredInside(80.f), kParamSkew, "Skew", knobStyle));
    pGraphics->AttachControl(new IVMenuButtonControl(controlsRow.GetGridCell(0, 5, 1, 6).GetCentredInside(120.f, 40.f), kParamShapeMode, "Forme"));

    // Fenetre unique : dessin et resultat final partagent la meme zone.
    IRECT curveArea = IRECT(bounds.L, bounds.T + 120.f, bounds.R, bounds.B).GetPadded(-20.f);
    mCurveView = new SpectralCurvePreviewControl(curveArea, [this](const float* data, int size) {
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
#endif
}

#if IPLUG_DSP

void OperateursSpectraux::UpdateEngine()
{
  mEngine.SetSize(512); // resolution de l'apercu pour l'instant (pas encore lie a une taille FFT)
  mEngine.SetShapeMode((int)GetParam(kParamShapeMode)->Value() == 0
                          ? SpectralCurveEngine::ShapeMode::Type
                          : SpectralCurveEngine::ShapeMode::Draw);
  mEngine.SetCycles((float)GetParam(kParamCycles)->Value());
  mEngine.SetQ((float)(GetParam(kParamQ)->Value() / 100.0));
  mEngine.SetBallade((float)(GetParam(kParamBallade)->Value() / 100.0));
  mEngine.SetHorizon((float)(GetParam(kParamHorizon)->Value() / 100.0));
  mEngine.SetSkew((float)GetParam(kParamSkew)->Value());

  mEngine.RebuildIfNeeded();

  mCurveUISize = mEngine.GetSize();
  const float* curve = mEngine.GetCurve();
  for (int i = 0; i < mCurveUISize; i++)
    mCurveUIBuf[i] = curve[i];
  mCurveUIUpdated.store(true);
}

void OperateursSpectraux::OnReset()
{
  UpdateEngine();
}

void OperateursSpectraux::OnParamChange(int paramIdx)
{
  switch (paramIdx)
  {
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
    default:
      break;
  }
}

void OperateursSpectraux::ProcessBlock(sample** inputs, sample** outputs, int nFrames)
{
  // Etape 2 : passthrough pur, aucun traitement audio pour l'instant.
  for (int i = 0; i < nFrames; i++)
  {
    outputs[0][i] = inputs[0][i];
    outputs[1][i] = inputs[1][i];
  }
}

#endif
