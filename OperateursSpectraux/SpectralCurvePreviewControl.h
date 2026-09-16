#pragma once

#include "IControl.h"
#include <vector>
#include <algorithm>
#include <functional>
#include <cmath>
#include <cstdio>

// ============================================================================
// SpectralCurvePreviewControl
//
// Fenetre unique, bipolaire (-1 a 1) :
//  - En mode Dessin (SetDrawMode(true)) : interactive, on trace a la
//    souris (interpole entre les points pendant un glissement rapide) -
//    affiche le trace brut pendant qu'on dessine, prevenu au relachement.
//  - Sinon (ou une fois le trait relache) : affiche le resultat final
//    transforme (fourni via SetCurve, mis a jour depuis le thread
//    interface, jamais depuis l'audio).
// ============================================================================

class SpectralCurvePreviewControl : public iplug::igraphics::IControl
{
public:
  using ShapeChangedFunc = std::function<void(const float*, int)>;

  SpectralCurvePreviewControl(const iplug::igraphics::IRECT& bounds, ShapeChangedFunc onShapeChanged = nullptr)
  : IControl(bounds)
  , mOnShapeChanged(onShapeChanged)
  {
    mDrawnShape.assign(kDrawResolution, 0.f);
  }

  void SetDrawMode(bool drawMode) { mDrawMode = drawMode; SetDirty(false); }

  // Resultat final transforme (affiche quand on n'est pas en train de
  // dessiner activement).
  void SetCurve(const float* buf, int size)
  {
    mResultSize = std::min(size, kMaxResultPoints);
    for (int i = 0; i < mResultSize; i++)
    {
      int srcIdx = (int)((float)i / (float)mResultSize * (float)size);
      srcIdx = std::min(srcIdx, size - 1);
      mResultBuffer[i] = buf[srcIdx];
    }
  }

  void OnMouseDown(float x, float y, const iplug::igraphics::IMouseMod& mod) override
  {
    if (!mDrawMode) return;
    mDrawing = true;
    mLastIdx = -1;
    DrawPointAt(x, y);
  }

  void OnMouseDrag(float x, float y, float dX, float dY, const iplug::igraphics::IMouseMod& mod) override
  {
    if (mDrawMode && mDrawing) DrawPointAt(x, y);
  }

  void OnMouseUp(float x, float y, const iplug::igraphics::IMouseMod& mod) override
  {
    if (!mDrawMode) return;
    mDrawing = false;
    if (mOnShapeChanged) mOnShapeChanged(mDrawnShape.data(), (int)mDrawnShape.size());
  }

  void Draw(iplug::igraphics::IGraphics& g) override
  {
    using namespace iplug::igraphics;

    g.FillRect(IColor(255, 15, 15, 20), mRECT);

    float w = mRECT.W();
    float h = mRECT.H() * 0.42f;
    float midY = mRECT.MH();

    // --- Reperes Hz (echelle log, 20Hz a 20kHz - meme mapping que le filtre) ---
    const float freqMarks[] = { 20.f, 100.f, 1000.f, 10000.f, 20000.f };
    const char* freqLabels[] = { "20Hz", "100Hz", "1kHz", "10kHz", "20kHz" };
    IText freqText(9.f, IColor(255, 130, 130, 140), "Roboto-Regular", EAlign::Center, EVAlign::Top);
    for (int m = 0; m < 5; m++)
    {
      float logPos = std::log(freqMarks[m] / 20.f) / std::log(20000.f / 20.f);
      float x = mRECT.L + w * logPos;
      g.DrawLine(IColor(255, 40, 40, 45), x, mRECT.T, x, mRECT.B, nullptr, 1.f);
      g.DrawText(freqText, freqLabels[m], IRECT(x - 25.f, mRECT.B - 14.f, x + 25.f, mRECT.B));
    }

    // --- Reperes dB (echelle lineaire, -24 a +24, meme plage que le filtre) ---
    const float dbMarks[] = { -24.f, -12.f, 0.f, 12.f, 24.f };
    IText dbText(9.f, IColor(255, 130, 130, 140), "Roboto-Regular", EAlign::Near, EVAlign::Middle);
    for (int m = 0; m < 5; m++)
    {
      float y = midY - (dbMarks[m] / 24.f) * h;
      g.DrawLine(IColor(255, 40, 40, 45), mRECT.L, y, mRECT.R, y, nullptr, 1.f);
      char buf[16];
      snprintf(buf, sizeof(buf), "%+.0fdB", dbMarks[m]);
      g.DrawText(dbText, buf, IRECT(mRECT.L + 2.f, y - 7.f, mRECT.L + 50.f, y + 7.f));
    }

    g.DrawLine(IColor(255, 60, 60, 65), mRECT.L, midY, mRECT.R, midY, nullptr, 1.f);

    if (mDrawMode && mDrawing)
    {
      // Pendant le trace : affiche le dessin brut, tel quel.
      int n = (int)mDrawnShape.size();
      for (int i = 0; i < n - 1; i++)
      {
        float x0 = mRECT.L + w * (float)i / (float)(n - 1);
        float x1 = mRECT.L + w * (float)(i + 1) / (float)(n - 1);
        float y0 = midY - mDrawnShape[i] * h;
        float y1 = midY - mDrawnShape[i + 1] * h;
        g.DrawLine(IColor(255, 200, 160, 130), x0, y0, x1, y1, nullptr, 1.5f);
      }
    }
    else
    {
      // Sinon : affiche le resultat final transforme.
      if (mResultSize < 2) return;
      for (int i = 0; i < mResultSize - 1; i++)
      {
        float x0 = mRECT.L + w * (float)i / (float)(mResultSize - 1);
        float x1 = mRECT.L + w * (float)(i + 1) / (float)(mResultSize - 1);
        float y0 = midY - mResultBuffer[i] * h;
        float y1 = midY - mResultBuffer[i + 1] * h;
        g.DrawLine(IColor(255, 130, 200, 160), x0, y0, x1, y1, nullptr, 1.5f);
      }
    }
  }

private:
  void DrawPointAt(float mx, float my)
  {
    float xFrac = std::clamp((mx - mRECT.L) / mRECT.W(), 0.f, 1.f);
    float yVal = std::clamp(-(my - mRECT.MH()) / (mRECT.H() * 0.42f), -1.f, 1.f);
    int idx = std::clamp((int)(xFrac * (kDrawResolution - 1)), 0, kDrawResolution - 1);

    // Interpole entre le dernier point et celui-ci, pour eviter les trous
    // pendant un glissement rapide de la souris.
    if (mLastIdx >= 0 && mLastIdx != idx)
    {
      int lo = std::min(mLastIdx, idx);
      int hi = std::max(mLastIdx, idx);
      float loVal = (mLastIdx < idx) ? mLastY : yVal;
      float hiVal = (mLastIdx < idx) ? yVal : mLastY;
      for (int i = lo; i <= hi; i++)
      {
        float t = (hi > lo) ? (float)(i - lo) / (float)(hi - lo) : 0.f;
        mDrawnShape[i] = loVal + t * (hiVal - loVal);
      }
    }
    else
    {
      mDrawnShape[idx] = yVal;
    }

    mLastIdx = idx;
    mLastY = yVal;
    SetDirty(false);
  }

  static constexpr int kDrawResolution = 128;
  static constexpr int kMaxResultPoints = 512;

  bool mDrawMode = false;
  bool mDrawing = false;
  int mLastIdx = -1;
  float mLastY = 0.f;
  std::vector<float> mDrawnShape;

  float mResultBuffer[kMaxResultPoints] = { 0.f };
  int mResultSize = 0;

  ShapeChangedFunc mOnShapeChanged;
};
