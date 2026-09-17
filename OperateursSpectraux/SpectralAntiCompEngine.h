#pragma once

#include <vector>
#include <complex>
#include <cmath>
#include <algorithm>

// ============================================================================
// SpectralAntiCompEngine
//
// Compresseur "inverse" (ratio < 1, façon ReaFir) par bande de frequence :
// au lieu de resserrer la dynamique autour du seuil, un ratio < 1 l'ECARTE
// - les parties fortes deviennent plus fortes, les parties faibles plus
// faibles, par rapport au seuil de CETTE bande precise. Le seuil est
// pilote par la courbe partagee (dB), le ratio est un reglage global.
//
// Meme formule qu'un compresseur classique (outputDb = seuil +
// (inputDb-seuil)/ratio), juste avec ratio < 1 au lieu de > 1 - c'est ce
// qui inverse compression en expansion. D'ou le besoin imperatif du
// limiteur Brickwall final : ce module peut ecarter la dynamique de
// facon tres agressive.
// ============================================================================

class SpectralAntiCompEngine
{
public:
  using cplx = std::complex<float>;

  void Init(int fftSize, int overlapFactor, double sampleRate)
  {
    mFFTSize = fftSize;
    mOverlap = overlapFactor;
    mHopSize = mFFTSize / mOverlap;
    mSampleRate = sampleRate;

    mRing.assign(mFFTSize, 0.f);
    mRingOut.assign(mFFTSize, 0.f);
    mWindow.resize(mFFTSize);
    mTime.resize(mFFTSize);
    mCplx.assign(mFFTSize, cplx(0.f, 0.f));

    for (int i = 0; i < mFFTSize; i++)
      mWindow[i] = 0.5f - 0.5f * std::cos(2.f * kPi * i / (mFFTSize - 1));

    int numBins = mFFTSize / 2 + 1;
    mEnvelopeDb.assign(numBins, kFloorDb);
    mGainSmoothDb.assign(numBins, 0.f);

    // Coefficients d'attaque/relachement de l'enveloppe par bande,
    // recalcules ici puisqu'ils dependent de la duree d'un hop.
    mHopDurationMs = (float)mHopSize / (float)mSampleRate * 1000.f;
    mAttackCoeff = 1.f - std::exp(-mHopDurationMs / kAttackMs);
    RecomputeReleaseCoeff();

    mWritePos = 0;
    mReadPos = 0;
    mSamplesUntilHop = mHopSize;
  }

  void SetCurve(const float* curve, int curveSize) { mCurve = curve; mCurveSize = curveSize; }
  void SetRatio(float ratio) { mRatio = std::clamp(ratio, 0.02f, 1.f); }
  void SetReleaseMs(float ms) { mReleaseMs = std::clamp(ms, 5.f, 2000.f); RecomputeReleaseCoeff(); }

  void Process(const float* in, float* out, int nFrames)
  {
    for (int i = 0; i < nFrames; i++)
    {
      mRing[mWritePos] = in[i];

      out[i] = mRingOut[mReadPos];
      mRingOut[mReadPos] = 0.f;

      mWritePos = (mWritePos + 1) % mFFTSize;
      mReadPos = (mReadPos + 1) % mFFTSize;

      if (--mSamplesUntilHop == 0)
      {
        mSamplesUntilHop = mHopSize;
        ProcessHop();
      }
    }
  }

private:
  void RecomputeReleaseCoeff()
  {
    mReleaseCoeff = 1.f - std::exp(-mHopDurationMs / mReleaseMs);
  }

  void ReadRingIntoLinear(const std::vector<float>& ring, std::vector<float>& dst)
  {
    int start = mWritePos;
    for (int i = 0; i < mFFTSize; i++)
      dst[i] = ring[(start + i) % mFFTSize];
  }

  static void FFT(std::vector<cplx>& a, bool invert)
  {
    int n = (int)a.size();
    for (int i = 1, j = 0; i < n; i++)
    {
      int bit = n >> 1;
      for (; j & bit; bit >>= 1)
        j ^= bit;
      j ^= bit;
      if (i < j) std::swap(a[i], a[j]);
    }

    for (int len = 2; len <= n; len <<= 1)
    {
      float ang = 2.f * kPi / (float)len * (invert ? 1.f : -1.f);
      cplx wlen(std::cos(ang), std::sin(ang));
      for (int i = 0; i < n; i += len)
      {
        cplx w(1.f, 0.f);
        for (int k = 0; k < len / 2; k++)
        {
          cplx u = a[i + k];
          cplx v = a[i + k + len / 2] * w;
          a[i + k] = u + v;
          a[i + k + len / 2] = u - v;
          w *= wlen;
        }
      }
    }
    if (invert)
      for (auto& x : a) x /= (float)n;
  }

  // Interpole la courbe (-1..1) sur l'echelle log 20Hz-20kHz, mappee sur
  // -60dB..0dB pour le seuil de cette bande.
  float GetThresholdDbForBin(int binIdx) const
  {
    if (!mCurve || mCurveSize < 2) return -30.f;

    float freq = (float)binIdx * (float)mSampleRate / (float)mFFTSize;
    freq = std::clamp(freq, 20.f, 20000.f);
    float logPos = std::log(freq / 20.f) / std::log(20000.f / 20.f);

    float pos = logPos * (float)(mCurveSize - 1);
    int idx0 = (int)pos;
    int idx1 = std::min(idx0 + 1, mCurveSize - 1);
    float frac = pos - (float)idx0;
    float curveVal = mCurve[idx0] * (1.f - frac) + mCurve[idx1] * frac; // -1..1

    return curveVal * 30.f - 30.f; // -1..1 -> -60dB..0dB
  }

  void ProcessHop()
  {
    ReadRingIntoLinear(mRing, mTime);

    for (int i = 0; i < mFFTSize; i++)
      mCplx[i] = cplx(mTime[i] * mWindow[i], 0.f);

    FFT(mCplx, false);

    int numBins = mFFTSize / 2;
    for (int k = 0; k <= numBins; k++)
    {
      float magnitude = std::abs(mCplx[k]);
      float levelDb = 20.f * std::log10(std::max(magnitude, 1e-9f));
      levelDb = std::max(levelDb, kFloorDb);

      // Enveloppe attaque/relachement par bande - lisse le niveau brut du
      // hop courant, evite un comportement trop nerveux.
      if (levelDb > mEnvelopeDb[k])
        mEnvelopeDb[k] += (levelDb - mEnvelopeDb[k]) * mAttackCoeff;
      else
        mEnvelopeDb[k] += (levelDb - mEnvelopeDb[k]) * mReleaseCoeff;

      float thresholdDb = GetThresholdDbForBin(k);

      // Meme formule qu'un compresseur classique, mais ratio < 1 -> ecarte
      // la dynamique au lieu de la resserrer (applique uniformement,
      // au-dessus ET en dessous du seuil - pas de branche if/else,
      // "l'explosion des dynamiques" vient naturellement des deux cotes).
      float outputLevelDb = thresholdDb + (mEnvelopeDb[k] - thresholdDb) / mRatio;
      float gainDb = outputLevelDb - mEnvelopeDb[k];
      gainDb = std::clamp(gainDb, -60.f, 24.f); // securite locale, en plus du limiteur final

      // Lissage SEPARE, applique au GAIN lui-meme (pas juste au niveau
      // detecte) - a Ratio bas, la formule ci-dessus amplifie enormement
      // les petites fluctuations du niveau (diviser par un ratio proche
      // de 0 peut transformer 1dB de variation en 50dB de variation de
      // gain) - sans ce second lissage, ca cree des craquements/zipper
      // audibles a chaque saut de hop.
      if (gainDb > mGainSmoothDb[k])
        mGainSmoothDb[k] += (gainDb - mGainSmoothDb[k]) * mAttackCoeff;
      else
        mGainSmoothDb[k] += (gainDb - mGainSmoothDb[k]) * mReleaseCoeff;

      float gainLin = std::pow(10.f, mGainSmoothDb[k] / 20.f);

      mCplx[k] *= gainLin;
      if (k > 0 && k < numBins)
        mCplx[mFFTSize - k] = std::conj(mCplx[k]);
    }

    FFT(mCplx, true);

    float normOverlap = 1.f / (float)mOverlap * 2.f;
    int start = mWritePos;
    for (int i = 0; i < mFFTSize; i++)
    {
      int idx = (start + i) % mFFTSize;
      mRingOut[idx] += mCplx[i].real() * mWindow[i] * normOverlap;
    }
  }

  static constexpr float kPi = 3.14159265358979323846f;
  static constexpr float kFloorDb = -80.f;
  static constexpr float kAttackMs = 5.f;

  int mFFTSize = 1024;
  int mOverlap = 4;
  int mHopSize = 256;
  int mSamplesUntilHop = 256;
  int mWritePos = 0;
  int mReadPos = 0;
  double mSampleRate = 44100.0;

  const float* mCurve = nullptr;
  int mCurveSize = 0;
  float mRatio = 1.f;

  float mReleaseMs = 80.f;
  float mHopDurationMs = 10.f;
  float mAttackCoeff = 0.5f;
  float mReleaseCoeff = 0.05f;
  std::vector<float> mEnvelopeDb;
  std::vector<float> mGainSmoothDb;

  std::vector<float> mRing, mRingOut, mWindow, mTime;
  std::vector<cplx> mCplx;
};
