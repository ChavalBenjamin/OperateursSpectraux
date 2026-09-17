#pragma once

#include <vector>
#include <complex>
#include <cmath>
#include <algorithm>

// ============================================================================
// SpectralMagnitudeDistortEngine
//
// 3 mecanismes combinables :
//
//  1. Distorsion en magnitude (existant) : nouvelle_magnitude = magnitude
//     ^ exposant, par bande - redistribue l'energie deja presente, ne
//     cree jamais de nouvelle frequence.
//
//  2. Injection harmonique (nouveau) : copie une partie de l'energie de
//     chaque bande vers ses multiples (x2, x3, x4) - cree une VRAIE
//     nouvelle richesse spectrale, contrairement a la distorsion en
//     magnitude seule.
//
//  3. Distorsion temporelle (nouveau) : waveshaping (tanh) applique sur
//     le signal RECONSTRUIT final (apres overlap-add) - vraie generation
//     d'harmoniques par pliage de la forme d'onde, comme une saturation
//     analogique classique.
//
// La latence de traitement (environ une fenetre FFT complete) est
// exposee via GetLatencySamples(), pour que le plugin puisse a la fois
// informer l'hote (PDC) et compenser son propre signal sec (Dry/Wet).
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
    mMagInjected.assign(mFFTSize, 0.f);
    mPhaseBuf.assign(mFFTSize, 0.f);

    for (int i = 0; i < mFFTSize; i++)
      mWindow[i] = 0.5f - 0.5f * std::cos(2.f * kPi * i / (mFFTSize - 1));

    mWritePos = 0;
    mReadPos = 0;
    mSamplesUntilHop = mHopSize;
  }

  void SetCurve(const float* curve, int curveSize) { mCurve = curve; mCurveSize = curveSize; }
  void SetHarmonicInjection(float amount) { mHarmonicInjection = std::clamp(amount, 0.f, 1.f); }
  void SetTempDrive(float drive) { mTempDrive = std::clamp(drive, 0.f, 1.f); }

  // Latence de traitement introduite (en echantillons) - environ une
  // fenetre FFT complete pour ce type d'architecture (ring buffer STFT).
  int GetLatencySamples() const { return mFFTSize; }

  void Process(const float* in, float* out, int nFrames)
  {
    for (int i = 0; i < nFrames; i++)
    {
      mRing[mWritePos] = in[i];

      float wetSample = mRingOut[mReadPos];
      mRingOut[mReadPos] = 0.f;

      // Distorsion temporelle (waveshaping) : appliquee ICI, sur le
      // signal RECONSTRUIT final (apres overlap-add) - jamais avant,
      // sinon la distorsion serait appliquee plusieurs fois dans les
      // zones de chevauchement entre hops.
      out[i] = Waveshape(wetSample, mTempDrive);

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
  static float Waveshape(float x, float drive)
  {
    if (drive <= 0.0001f) return x; // neutre exact
    float k = 1.f + drive * kMaxDrive;
    return std::tanh(x * k) / std::tanh(k);
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

    // Passe 1 : distorsion en magnitude par bande (existant).
    float energyBefore = 0.f;
    for (int k = 0; k <= numBins; k++)
    {
      float mag = std::abs(mCplx[k]);
      float exponent = GetExponentForBin(k);
      mMagBuf[k] = std::pow(std::max(mag, 1e-9f), exponent);
      mPhaseBuf[k] = std::arg(mCplx[k]);
      energyBefore += mag * mag;
    }

    // Passe 2 : injection harmonique - copie de l'energie de chaque bande
    // vers ses multiples (x2, x3, x4), decroissante avec l'ordre - cree
    // une vraie nouvelle richesse spectrale (pas juste un rescale).
    if (mHarmonicInjection > 0.001f)
    {
      std::copy(mMagBuf.begin(), mMagBuf.begin() + numBins + 1, mMagInjected.begin());
      for (int k = 1; k <= numBins; k++)
      {
        float srcMag = mMagBuf[k];
        if (srcMag < 1e-6f) continue;
        for (int h = 2; h <= 4; h++)
        {
          int targetBin = k * h;
          if (targetBin > numBins) break;
          mMagInjected[targetBin] += srcMag * (mHarmonicInjection / (float)h);
        }
      }
      std::copy(mMagInjected.begin(), mMagInjected.begin() + numBins + 1, mMagBuf.begin());
    }

    // Passe 3 : compensation de gain (apres distorsion ET injection, pour
    // recaler sur l'energie REELLEMENT ajoutee), puis reconstruction.
    float energyAfter = 0.f;
    for (int k = 0; k <= numBins; k++)
      energyAfter += mMagBuf[k] * mMagBuf[k];

    float gain = std::sqrt(energyBefore / std::max(energyAfter, 1e-9f));
    gain = std::clamp(gain, 0.1f, 10.f);

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
  static constexpr float kMaxExponent = 8.f;  // repousse (avant : 4)
  static constexpr float kMaxDrive = 30.f;    // intensite max de la distorsion temporelle

  int mFFTSize = 1024;
  int mOverlap = 4;
  int mHopSize = 256;
  int mSamplesUntilHop = 256;
  int mWritePos = 0;
  int mReadPos = 0;
  double mSampleRate = 44100.0;

  const float* mCurve = nullptr;
  int mCurveSize = 0;
  float mHarmonicInjection = 0.f;
  float mTempDrive = 0.f;

  std::vector<float> mRing, mRingOut, mWindow, mTime;
  std::vector<cplx> mCplx;
  std::vector<float> mMagBuf, mMagInjected, mPhaseBuf;
};
