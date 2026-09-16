#pragma once

#include <vector>
#include <cmath>
#include <algorithm>

// ============================================================================
// SpectralCurveEngine
//
// Moteur de courbe partage, reutilise par les 3 modules (Filtre, Delay,
// Inverse Comp) des "Operateurs Spectraux". Genere une courbe 2D
// (spectre en X, valeur en Y, -1 a 1) a partir de 5 parametres :
//
//  - Cycles  : nombre d'oscillations a travers le spectre (jusqu'a 24)
//  - Q       : 0 = plat, milieu = sinus plein, max = notchs etroits
//  - Ballade : phase (0..1 = un tour complet) - boucle proprement pour un LFO
//  - Horizon : asymetrie - 0.5 = symetrique, vers 0 = ne garde que les
//              bosses positives, vers 1 = ne garde que les creux negatifs
//  - Skew    : redistribution non-lineaire de Y (type gamma) - 1 = neutre,
//              >1 = ecarte les extremes, <1 = resserre vers les extremes
// ============================================================================

class SpectralCurveEngine
{
public:
  void SetSize(int numPoints)
  {
    numPoints = std::max(2, numPoints);
    // "|| mCurve.empty()" est essentiel : sans ca, le tout premier appel
    // (quand numPoints correspond deja a la valeur par defaut du membre)
    // ne redimensionne jamais le vecteur, qui reste vide alors que le
    // reste du code croit qu'il contient mNumPoints elements - ecriture
    // hors limites garantie au premier RebuildIfNeeded().
    if (numPoints != mNumPoints || mCurve.empty())
    {
      mNumPoints = numPoints;
      mCurve.assign(mNumPoints, 0.f);
      mDirty = true;
    }
  }

  void SetCycles(float cycles) { SetIfChanged(mCycles, std::clamp(cycles, 0.f, 24.f)); }
  void SetQ(float q) { SetIfChanged(mQ, std::clamp(q, 0.f, 1.f)); }
  void SetBallade(float ballade) { SetIfChanged(mBallade, std::clamp(ballade, 0.f, 1.f)); }
  void SetHorizon(float horizon) { SetIfChanged(mHorizon, std::clamp(horizon, 0.f, 1.f)); }
  void SetSkew(float skew) { SetIfChanged(mSkew, std::clamp(skew, 0.1f, 6.f)); }

  const float* GetCurve() const { return mCurve.data(); }
  int GetSize() const { return mNumPoints; }

  void RebuildIfNeeded()
  {
    if (!mDirty) return;
    mDirty = false;

    for (int i = 0; i < mNumPoints; i++)
    {
      float x = (float)i / (float)(mNumPoints - 1); // 0..1

      // 0. Skew : redistribue la POSITION des cycles sur l'axe X (pas leur
      // hauteur) - resserre les cycles d'un cote du spectre, les etire de
      // l'autre. Skew=1 = lineaire (neutre), <1 et >1 divergent dans des
      // sens opposes. Applique AVANT le calcul de phase, sur x directement.
      float xWarped = std::pow(x, mSkew);

      // 1. Oscillation de base - phase pilotee par Cycles et Ballade,
      // boucle proprement (Ballade parcourt exactement un tour, 0 a 2*Pi).
      float phase = 2.f * kPi * mCycles * xWarped + mBallade * 2.f * kPi;
      float s = std::sin(phase);

      // 2. Q : deux regimes - 0..milieu monte l'amplitude (plat -> sinus
      // plein), milieu..max resserre les pics (sinus plein -> notchs).
      float y;
      if (mQ <= 0.5f)
      {
        float amt = mQ / 0.5f;
        y = s * amt;
      }
      else
      {
        float amt = (mQ - 0.5f) / 0.5f;
        float power = 1.f + amt * 11.f; // durete croissante des notchs
        float signS = (s >= 0.f) ? 1.f : -1.f;
        y = signS * std::pow(std::abs(s), power); // pointe (pas 1/power, qui aplatissait)
      }

      // 3. Horizon : asymetrie - 0.5 = symetrique (rien ne change).
      float posGain, negGain;
      if (mHorizon <= 0.5f)
      {
        posGain = 1.f;
        negGain = mHorizon / 0.5f;
      }
      else
      {
        negGain = 1.f;
        posGain = (1.f - mHorizon) / 0.5f;
      }
      y *= (y >= 0.f) ? posGain : negGain;

      // (Skew deplace au debut : redistribue la position X des cycles,
      // voir plus haut - il n'agit plus sur la hauteur Y ici.)

      mCurve[i] = y;
    }
  }

private:
  void SetIfChanged(float& member, float value)
  {
    if (value != member) { member = value; mDirty = true; }
  }

  static constexpr float kPi = 3.14159265358979323846f;

  int mNumPoints = 512;
  float mCycles = 1.f;
  float mQ = 0.5f;       // sinus plein par defaut
  float mBallade = 0.f;
  float mHorizon = 0.5f; // symetrique par defaut
  float mSkew = 1.f;     // neutre par defaut

  bool mDirty = true;
  std::vector<float> mCurve;
};
