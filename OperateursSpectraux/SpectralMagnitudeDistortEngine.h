#pragma once

#include <vector>
#include <complex>
#include <cmath>
#include <algorithm>

// ============================================================================
// SpectralMagnitudeDistortEngine
//
// Distorsion en magnitude, par bande : nouvelle_magnitude = magnitude ^
// exposant. Contrairement a l'Anti-Comp, aucun seuil, aucune branche -
// une fonction continue, donc structurellement beaucoup moins susceptible
// de creer des clics/craquements.
//
// Exposant pilote par la courbe partagee : 0 (centre) = neutre (exposant
// 1, rien ne change), vers un sens = compresse les harmoniques faibles
// (exposant > 1, "assainit"), vers l'autre = les exagere (exposant < 1,
// "fait ressortir" le detail faible, plus bruyant/riche).
//
// Compensation de gain (meme principe que MagniPhase/Magnitude1) : la
// distorsion en puissance peut deplacer enormement le niveau global selon
// l'exposant - on recale l'energie globale pour eviter les sauts de
// volume, le limiteur final gere le reste.
// ============================================================================

class SpectralMagnitudeDistortEngine
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
    mMagBuf.assign(mFFTSize, 0.f);
    mPhaseBuf.assign(mFFTSize, 0.f);

    for (int i = 0; i < mFFTSize; i++)
      mWindow[i] = 0.5f - 0.5f * std::cos(2.f * kPi * i / (mFFTSize - 1));

    mWritePos = 0;
    mReadPos = 0;
    mSamplesUntilHop = mHopSize;
  }

  void SetCurve(const float* curve, int curveSize) { mCurve = curve; mCurveSize = curveSize; }

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
  // un exposant : 0 -> 1 (neutre), +1 -> kMaxExponent, -1 -> 1/kMaxExponent.
  float GetExponentForBin(int binIdx) const
  {
    if (!mCurve || mCurveSize < 2) return 1.f;

    float freq = (float)binIdx * (float)mSampleRate / (float)mFFTSize;
    freq = std::clamp(freq, 20.f, 20000.f);
    float logPos = std::log(freq / 20.f) / std::log(20000.f / 20.f);

    float pos = logPos * (float)(mCurveSize - 1);
    int idx0 = (int)pos;
    int idx1 = std::min(idx0 + 1, mCurveSize - 1);
    float frac = pos - (float)idx0;
    float curveVal = mCurve[idx0] * (1.f - frac) + mCurve[idx1] * frac; // -1..1

    return std::pow(kMaxExponent, curveVal);
  }

  void ProcessHop()
  {
    ReadRingIntoLinear(mRing, mTime);

    for (int i = 0; i < mFFTSize; i++)
      mCplx[i] = cplx(mTime[i] * mWindow[i], 0.f);

    FFT(mCplx, false);

    int numBins = mFFTSize / 2;

    // Passe 1 : applique l'exposant par bande, accumule l'energie avant/apres.
    float energyBefore = 0.f, energyAfter = 0.f;
    for (int k = 0; k <= numBins; k++)
    {
      float mag = std::abs(mCplx[k]);
      float exponent = GetExponentForBin(k);
      float newMag = std::pow(std::max(mag, 1e-9f), exponent);

      mMagBuf[k] = newMag;
      mPhaseBuf[k] = std::arg(mCplx[k]);

      energyBefore += mag * mag;
      energyAfter += newMag * newMag;
    }

    float gain = std::sqrt(energyBefore / std::max(energyAfter, 1e-9f));
    gain = std::clamp(gain, 0.1f, 10.f);

    // Passe 2 : applique le gain de compensation, reconstruit.
    for (int k = 0; k <= numBins; k++)
    {
      float outMag = mMagBuf[k] * gain;
      float outPhase = mPhaseBuf[k];

      cplx val(outMag * std::cos(outPhase), outMag * std::sin(outPhase));
      mCplx[k] = val;
      if (k > 0 && k < numBins)
        mCplx[mFFTSize - k] = std::conj(val);
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
  static constexpr float kMaxExponent = 4.f;

  int mFFTSize = 1024;
  int mOverlap = 4;
  int mHopSize = 256;
  int mSamplesUntilHop = 256;
  int mWritePos = 0;
  int mReadPos = 0;
  double mSampleRate = 44100.0;

  const float* mCurve = nullptr;
  int mCurveSize = 0;

  std::vector<float> mRing, mRingOut, mWindow, mTime;
  std::vector<cplx> mCplx;
  std::vector<float> mMagBuf, mPhaseBuf;
};
