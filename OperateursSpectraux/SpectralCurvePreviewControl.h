#pragma once

#include "IControl.h"
#include <algorithm>

// ============================================================================
// SpectralCurvePreviewControl
//
// Affiche la courbe generee par SpectralCurveEngine, bipolaire (-1 a 1,
// centree verticalement). Mise a jour depuis le thread interface (OnIdle),
// jamais depuis l'audio.
// ============================================================================

class SpectralCurvePreviewControl : public iplug::igraphics::IControl
{
public:
  SpectralCurvePreviewControl(const iplug::igraphics::IRECT& bounds)
  : IControl(bounds)
  {
  }

  void SetCurve(const float* buf, int size)
  {
    mSize = std::min(size, kMaxPoints);
    for (int i = 0; i < mSize; i++)
    {
      int srcIdx = (int)((float)i / (float)mSize * (float)size);
      srcIdx = std::min(srcIdx, size - 1);
      mBuffer[i] = buf[srcIdx];
    }
  }

  void Draw(iplug::igraphics::IGraphics& g) override
  {
    using namespace iplug::igraphics;

    g.FillRect(IColor(255, 15, 15, 20), mRECT);
    if (mSize < 2) return;

    float w = mRECT.W();
    float h = mRECT.H() * 0.42f; // demi-hauteur utile (courbe -1..1 centree)
    float midY = mRECT.MH();

    // Ligne de reference au centre (Y=0)
    g.DrawLine(IColor(255, 60, 60, 65), mRECT.L, midY, mRECT.R, midY, nullptr, 1.f);

    for (int i = 0; i < mSize - 1; i++)
    {
      float x0 = mRECT.L + w * (float)i / (float)(mSize - 1);
      float x1 = mRECT.L + w * (float)(i + 1) / (float)(mSize - 1);
      float y0 = midY - mBuffer[i] * h;
      float y1 = midY - mBuffer[i + 1] * h;
      g.DrawLine(IColor(255, 130, 200, 160), x0, y0, x1, y1, nullptr, 1.5f);
    }
  }

private:
  static constexpr int kMaxPoints = 512;
  float mBuffer[kMaxPoints] = { 0.f };
  int mSize = 0;
};
