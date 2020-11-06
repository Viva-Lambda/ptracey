#pragma once
// implements spectrum type from pbrt-book
// mostly adapted from
// https://github.com/mmp/pbrt-v3/blob/master/src/core/spectrum.h
#include <specdata.hpp>
#include <utils.hpp>
// CONSTANTS

static const int NB_SPECTRAL_SAMPLES = 60;
static const int VISIBLE_LAMBDA_START = 360;
static const int VISIBLE_LAMBDA_END = 830;
static const auto CIE_Y_INTEGRAL = 106.856895;

// utility functions for spectrums
bool isSamplesSorted(const double *lambdas,
                     const double *vs, int nb_samples) {
  for (int i = 0; i < nb_samples - 1; i++) {
    if (lambdas[i] > lambdas[i + 1])
      return false;
  }
  return true;
}
void sortSpectrumSamples(double *lambdas, double *vs,
                         int nb_samples) {
  std::vector<std::pair<double, double>> sortedVec;
  sortedVec.reserve(nb_samples);

  for (int i = 0; i < nb_samples; i++) {
    sortedVec.push_back(make_pair(lambdas[i], vs[i]));
  }
  std::sort(sortedVec.begin(), sortedVec.end());
  for (int i = 0; i < nb_samples; i++) {
    lambdas[i] = sortedVec[i].first;
    vs[i] = sortedVec[i].second;
  }
}

double averageSpectrumSamples(const double *lambdas,
                              const double *vs,
                              int nb_sample,
                              double lambdaStart,
                              double lambdaEnd) {
  D_CHECK(isSamplesSorted(lambdas, vs, nb_sample));
  D_CHECK(lambdaStart < lambdaEnd);
  //
  if (lambdas[0] >= lambdaEnd) {
    return vs[0];
  }
  if (lambdas[nb_sample - 1] <= lambdaStart) {
    return vs[nb_sample - 1];
  }
  if (nb_sample == 1) {
    return vs[0];
  }
  //
  double sum = 0.0;
  // constant segment contribution
  if (lambdaStart < lambdas[0]) {
    sum += vs[0] * (lambdas[0] - lambdaStart);
  }
  if (lambdaEnd > lambdas[nb_sample - 1]) {
    sum += vs[nb_sample] * (lambdas[nb_sample] - lambdaEnd);
  }
  int start_index = 0;
  while (lambdaStart > lambdas[start_index + 1])
    start_index++;

  D_CHECK(start_index + 1 < nb_sample);

  auto interp_l = [lambdas, vs](double w, int i) {
    return interp(
        w - lambdas[i] / (lambdas[i + 1] - lambdas[i]),
        vs[i], vs[i + 1]);
  };
  //
  //
  while (start_index + 1 < nb_sample &&
         lambdaEnd > lambdas[start_index]) {
    start_index++;
    double segStart =
        std::max(lambdaStart, lambdas[start_index]);
    double segEnd =
        std::min(lambdaEnd, lambdas[start_index + 1]);

    sum += 0.5 * (interp_l(segStart, start_index) +
                  interp_l(segEnd, start_index)) *
           (segEnd - segStart);
  }
  return sum / (lambdaEnd - lambdaStart);
}

double interpSpecSamples(const double *lambdas,
                         const double *vs, int nb_sample,
                         double lam) {
  D_CHECK(isSamplesSorted(lambdas, vs, nb_sample));
  if (lambdas[0] >= lam) {
    return vs[0];
  }
  if (lambdas[nb_sample - 1] <= lam) {
    return vs[nb_sample - 1];
  }
  int offset = findInterval(nb_sample, [&](int index) {
    return lambdas[index] <= 1;
  });
  D_CHECK(lam >= lambdas[offset] &&
          lam <= lambdas[offset + 1]);
  double lamVal = (lam - lambdas[offset]) /
                  (lambdas[offset + 1] - lambdas[offset]);
  return interp(lamVal, vs[offset], vs[offset + 1]);
}

inline void xyz2rgb(const double xyz[3], double rgb[3]) {
  rgb[0] = 3.240479 * xyz[0] - 1.537150 * xyz[1] -
           0.498535 * xyz[2];
  rgb[1] = -0.969256 * xyz[0] + 1.875991 * xyz[1] +
           0.041556 * xyz[2];
  rgb[2] = 0.055648 * xyz[0] - 0.204043 * xyz[1] +
           1.057311 * xyz[2];
}
inline void rgb2xyz(const double rgb[3], double xyz[3]) {
  xyz[0] = 0.412453 * rgb[0] + 0.357580 * rgb[1] +
           0.180423 * rgb[2];
  xyz[1] = 0.212671 * rgb[0] + 0.715160 * rgb[1] +
           0.072169 * rgb[2];
  xyz[2] = 0.019334 * rgb[0] + 0.119193 * rgb[1] +
           0.950227f * rgb[2];
}

enum class SpectrumType { Reflectance, Illuminant };
enum RGB_AXIS : int { RGB_R, RGB_G, RGB_B };

template <int NbSpecSamples> class CoeffSpectrum {
  //
protected:
  double coeffs[NbSpecSamples];

public:
  static const int nbSamples = NbSpecSamples;

  // constructors
  CoeffSpectrum(double c = 0.0) {
    for (int i = 0; i < nbSamples; i++) {
      coeffs[i] = c;
    }
    D_CHECK(!has_nans());
  }
  CoeffSpectrum(const CoeffSpectrum &s) {
    D_CHECK(!s.has_nans());
    for (int i = 0; i < nbSamples; ++i)
      coeffs[i] = s.coeffs[i];
  }
  CoeffSpectrum &operator=(const CoeffSpectrum &s) {
    D_CHECK(!s.has_nans());
    for (int i = 0; i < nbSamples; ++i)
      coeffs[i] = s.coeffs[i];
    return *this;
  }
  bool has_nans() const {
    for (int i = 0; i < nbSamples; i++) {
      if (isnan(coeffs[i]))
        return true;
    }
    return false;
  }
  bool is_black() const {
    for (int i = 0; i < nbSamples; i++) {
      if (coeffs[i] != 0.0) {
        return false;
      }
    }
    return true;
  }
  // implementing operators
  CoeffSpectrum &operator+=(const CoeffSpectrum &cs) {
    D_CHECK(!cs.has_nans());
    for (int i = 0; i < nbSamples; i++)
      coeffs[i] += cs.coeffs[i];
    return *this;
  }
  CoeffSpectrum operator+(const CoeffSpectrum &cs) const {
    D_CHECK(!cs.has_nans());
    CoeffSpectrum cspec = *this;
    for (int i = 0; i < nbSamples; i++)
      cspec.coeffs[i] += cs.coeffs[i];
    return cspec;
  }
  CoeffSpectrum operator-(const CoeffSpectrum &cs) const {
    D_CHECK(!cs.has_nans());
    CoeffSpectrum cspec = *this;
    for (int i = 0; i < nbSamples; i++)
      cspec.coeffs[i] -= cs.coeffs[i];
    return cspec;
  }
  CoeffSpectrum &operator-=(const CoeffSpectrum &cs) {
    D_CHECK(!cs.has_nans());
    for (int i = 0; i < nbSamples; i++)
      coeffs[i] -= cs.coeffs[i];
    return *this;
  }
  CoeffSpectrum operator/(const CoeffSpectrum &cs) const {
    D_CHECK(!cs.has_nans());
    CoeffSpectrum ret = *this;
    for (int i = 0; i < nbSamples; i++) {
      D_CHECK(cs.coeffs[i] != 0.0);
      ret.coeffs[i] /= cs.coeffs[i];
    }
    return ret;
  }
  CoeffSpectrum operator/(double d) const {
    D_CHECK(d != 0.0);
    D_CHECK(!isnan(d));
    CoeffSpectrum ret = *this;
    for (int i = 0; i < nbSamples; i++) {
      ret.coeffs[i] /= d;
    }
    return ret;
  }
  CoeffSpectrum &operator/=(double d) {
    D_CHECK(d != 0.0);
    D_CHECK(!isnan(d));
    for (int i = 0; i < nbSamples; i++) {
      coeffs[i] /= d;
    }
    return *this;
  }
  CoeffSpectrum operator*(const CoeffSpectrum &cs) {
    D_CHECK(!cs.has_nans());
    CoeffSpectrum ret = *this;
    for (int i = 0; i < nbSamples; i++) {
      ret.coeffs[i] *= cs.coeffs[i];
    }
    D_CHECK(!ret.has_nans());
    return ret;
  }
  CoeffSpectrum &operator*=(const CoeffSpectrum &cs) {
    D_CHECK(!cs.has_nans());
    for (int i = 0; i < nbSamples; i++) {
      coeffs[i] *= cs.coeffs[i];
    }
    D_CHECK(!has_nans());
    return *this;
  }
  CoeffSpectrum operator*(double d) {
    CoeffSpectrum ret = *this;
    for (int i = 0; i < nbSamples; i++) {
      ret.coeffs[i] *= d;
    }
    D_CHECK(!ret.has_nans());
    return ret;
  }
  CoeffSpectrum &operator*=(double d) {
    for (int i = 0; i < nbSamples; i++) {
      coeffs[i] *= d;
    }
    D_CHECK(has_nans());
    return *this;
  }
  double &operator[](int i) {
    D_CHECK(i >= 0 && i < nbSamples);
    return coeffs[i];
  }
  double operator[](int i) const {
    D_CHECK(i >= 0 && i < nbSamples);
    return coeffs[i];
  }
  bool operator==(const CoeffSpectrum &cs) const {
    D_CHECK(!cs.has_nans());
    for (int i = 0; i < nbSamples; i++) {
      if (cs.coeffs[i] != coeffs[i])
        return false;
    }
    return true;
  }
  bool operator!=(const CoeffSpectrum &cs) const {
    return !(*this == cs);
  }
  double max() const {
    auto c = coeffs[0];
    for (int i = 1; i < nbSamples; i++) {
      c = fmax(c, coeffs[i]);
    }
    return c;
  }
  double min() const {
    auto c = coeffs[0];
    for (int i = 1; i < nbSamples; i++) {
      c = fmin(c, coeffs[i]);
    }
    return c;
  }
  CoeffSpectrum clamp(double low = 0.0,
                      double high = FLT_MAX) {
    CoeffSpectrum cs;
    for (int i = 0; i < nbSamples; i++) {
      cs.coeffs[i] = clamp(coeffs[i], low, high);
    }
    D_CHECK(!cs.has_nans());
    return cs;
  }
  CoeffSpectrum sqrt(const CoeffSpectrum &cs) {
    D_CHECK(!cs.has_nans());
    CoeffSpectrum c;
    for (int i = 0; i < nbSamples; i++) {
      c.coeffs[i] = sqrt(cs.coeffs[i]);
    }
    D_CHECK(!c.has_nans());
    return c;
  }
};

class RgbSpectrum : public CoeffSpectrum<3> {
  //
  using CoeffSpectrum<3>::coeffs;

public:
  RgbSpectrum(double v = 0.0) : CoeffSpectrum<3>(v) {}
  RgbSpectrum(const CoeffSpectrum<3> &v)
      : CoeffSpectrum<3>(v) {}
  RgbSpectrum(
      const RgbSpectrum &v,
      SpectrumType type = SpectrumType::Reflectance) {
    *this = v;
  }
  static RgbSpectrum
  fromRgb(const double rgb[3],
          SpectrumType type = SpectrumType::Reflectance) {
    RgbSpectrum s;
    s.coeffs[0] = rgb[0];
    s.coeffs[1] = rgb[1];
    s.coeffs[2] = rgb[2];
    D_CHECK(!s.has_nans());
    return s;
  }
  const RgbSpectrum &toRgbSpectrum() const { return *this; }
  void to_xyz(double xyz[3]) const { rgb2xyz(coeffs, xyz); }
  static RgbSpectrum
  fromXyz(const double xyz[3],
          SpectrumType type = SpectrumType::Reflectance) {
    RgbSpectrum r;
    xyz2rgb(xyz, r.coeffs);
    return r;
  }
  double y() const {
    const auto yweights = {0.212671, 0.715160, 0.072169};
    return yweights[0] * coeffs[0] +
           yweights[1] * coeffs[1] +
           yweights[2] * coeffs[2];
  }

  static RgbSpectrum fromSamples(const double *lambdas,
                                 const double *vs,
                                 int nb_sample) {
    if (!isSamplesSorted(lambdas, vs, nb_sample)) {
      std::vector<double> slambda(&lambda[0],
                                  &lambda[nb_sample]);
      std::vector<double> svs(&vs[0], &vs[nb_sample]);
      sortSpectrumSamples(slambda.data(), svs.data(),
                          nb_sample);
      return fromSamples(slambda.data(), svs.data(),
                         nb_sample);
    }
    double xyz[3] = {0, 0, 0};
    for (int i = 0; i < NB_CIE_SAMPLES; i++) {
      //
      double v = interpSpecSamples(lambdas, vs, nb_sample,
                                   CIE_LAMBDA[i]);
      xyz[0] = v * CIE_X[i];
      xyz[1] = v * CIE_Y[i];
      xyz[2] = v * CIE_Z[i];
    }

    double scale =
        static_cast<double>(CIE_LAMBDA[NB_CIE_SAMPLES - 1] -
                            CIE_LAMBDA[0]) /
        static_cast<double>(CIE_Y_INTEGRAL *
                            NB_CIE_SAMPLES);
    xyz[0] *= scale;
    xyz[1] *= scale;
    xyz[2] *= scale;
    return fromXyz(xyz);
  }
};

class SampledSpectrum
    : public CoeffSpectrum<NB_SPECTRAL_SAMPLES> {
public:
  SampledSpectrum(double v = 0.0) : CoeffSpectrum(v) {}
  SampledSpectrum(
      const CoeffSpectrum<NB_SPECTRAL_SAMPLES> &v)
      : CoeffSpectrum<NB_SPECTRAL_SAMPLES>(v) {}
  static SampledSpectrum fromSamples(const double *lambdas,
                                     const double *vs,
                                     int nb_sample) {
    if (!isSamplesSorted(lambdas, vs, nb_sample)) {
      std::vector<double> slambda(&lambdas[0],
                                  &lambdas[nb_sample]);
      std::vector<double> svs(&vs[0], &vs[nb_sample]);
      sortSpectrumSamples(slambda.data(), svs.data(),
                          nb_sample);
      return fromSamples(slambda.data(), svs.data(),
                         nb_sample);
    }
    SampledSpectrum r;
    auto nb_samp = static_cast<double>(NB_SPECTRAL_SAMPLES);
    for (int i = 0; i < NB_SPECTRAL_SAMPLES; i++) {
      auto lambda0 =
          interp(static_cast<double>(i) / nb_samp,
                 VISIBLE_LAMBDA_START, VISIBLE_LAMBDA_END);
      auto lambda1 =
          interp(static_cast<double>(i + 1) / nb_samp,
                 VISIBLE_LAMBDA_START, VISIBLE_LAMBDA_END);
      r.coeffs[i] = averageSpectrumSamples(
          lambdas, vs, nb_sample, lambda0, lambda1);
    }
    return r;
  }
  void to_xyz(double xyz[3]) const {
    xyz[0] = xyz[1] = xyz[2] = 0.0;
    double scale =
        static_cast<double>(VISIBLE_LAMBDA_END -
                            VISIBLE_LAMBDA_START) /
        (CIE_Y_INTEGRAL * NB_SPECTRAL_SAMPLES);

    for (int i = 0; i < NB_SPECTRAL_SAMPLES; i++) {
      xyz[0] += X.coeffs[i] * coeffs[i];
      xyz[1] += Y.coeffs[i] * coeffs[i];
      xyz[2] += Z.coeffs[i] * coeffs[i];
    }
    xyz[0] *= scale;
    xyz[1] *= scale;
    xyz[2] *= scale;
  }
  double x() const { return choose_axis(0); }
  double y() const { return choose_axis(1); }
  double z() const { return choose_axis(2); }
  void toRGB(double rgb[3]) const {
    double xyz[3];
    to_xyz(xyz);
    xyz2rgb(xyz, rgb);
  }
  static RGB_AXIS min_axis(const double rgb[3]) {
    RGB_AXIS minax;
    double minval = std::min(rgb[0], rgb[1]);
    minval = std::min(minval, rgb[2]);
    if (minval == rgb[0])
      minax = RGB_R;
    if (minval == rgb[1])
      minax = RGB_G;
    if (minval == rgb[2])
      minax = RGB_B;
    return minax;
  }
  static SampledSpectrum fromRgb(const double rgb[3],
                                 SpectrumType type) {
    RGB_AXIS minax = min_axis(rgb);
    SampledSpectrum r;

    if (type == SpectrumType::Reflectance) {
      // convert reflectance spectrum to rgb color space

      switch (minax) {
      case RGB_R: {
        // compute reflectance with rgb0 as minimum
        r += rgbReflSpectWhite * rgb[0];

        if (rgb[1] <= rgb[2]) {
          r += (rgb[1] - rgb[0]) * rgbReflSpectCyan;
          r += (rgb[2] - rgb[1]) * rgbReflSpectBlue;
        } else {
          r += (rgb[2] - rgb[0]) * rgbReflSpectCyan;
          r += (rgb[1] - rgb[2]) * rgbReflSpectGreen;
        }
      }
      case RGB_G: {
        // compute reflectance with rgb1 as minimum
        r += rgbReflSpectWhite * rgb[1];
        if (rgb[0] <= rgb[2]) {
          r += (rgb[0] - rgb[1]) * rgbReflSpectMagenta;
          r += (rgb[2] - rgb[0]) * rgbReflSpectBlue;
        } else {
          r += (rgb[2] - rgb[1]) * rgbReflSpectMagenta;
          r += (rgb[0] - rgb[2]) * rgbReflSpectRed;
        }
      }
      case RGB_B: {
        // compute reflectance with rgb2 as minimum
        r += rgbReflSpectWhite * rgb[2];
        if (rgb[0] <= rgb[1]) {
          r += (rgb[0] - rgb[2]) * rgbReflSpectYellow;
          r += (rgb[1] - rgb[0]) * rgbReflSpectGreen;

        } else {
          r += (rgb[1] - rgb[2]) * rgbReflSpectYellow;
          r += (rgb[0] - rgb[1]) * rgbReflSpectGreen;
        }
      }
      }
      r *= 0.94;
    } else {
      // SpectrumType::Illuminant;
      switch (minax) {
      case RGB_R: {
        // compute reflectance with rgb0 as minimum
        r += rgbIllum2SpectWhite * rgb[0];

        if (rgb[1] <= rgb[2]) {
          r += (rgb[1] - rgb[0]) * rgbIllumSpectCyan;
          r += (rgb[2] - rgb[1]) * rgbIllumSpectBlue;
        } else {
          r += (rgb[2] - rgb[0]) * rgbIllumSpectCyan;
          r += (rgb[1] - rgb[2]) * rgbIllumSpectGreen;
        }
      }
      case RGB_G: {
        // compute reflectance with rgb1 as minimum
        r += rgbIllumSpectWhite * rgb[1];
        if (rgb[0] <= rgb[2]) {
          r += (rgb[0] - rgb[1]) * rgbIllumSpectMagenta;
          r += (rgb[2] - rgb[0]) * rgbIllumSpectBlue;
        } else {
          r += (rgb[2] - rgb[1]) * rgbIllumSpectMagenta;
          r += (rgb[0] - rgb[2]) * rgbIllumSpectRed;
        }
      }
      case RGB_B: {
        // compute reflectance with rgb2 as minimum
        r += rgbIllumSpectWhite * rgb[2];
        if (rgb[0] <= rgb[1]) {
          r += (rgb[0] - rgb[2]) * rgbIllumSpectYellow;
          r += (rgb[1] - rgb[0]) * rgbIllumSpectGreen;

        } else {
          r += (rgb[1] - rgb[2]) * rgbIllumSpectYellow;
          r += (rgb[0] - rgb[1]) * rgbIllumSpectGreen;
        }
      }
      }
      r *= 0.86445;
    }
    return r.clamp();
  }
  static SampledSpectrum
  fromXyz(const double xyz[3],
          SpectrumType type = SpectrumType::Reflectance) {
    double rgb[3];
    xyz2rgb(xyz, rgb);
    return fromRgb(rgb, type);
  }
  SampledSpectrum(
      const RgbSpectrum &r,
      SpectrumType type = SpectrumType::Reflectance) {
    double rgb[3];
    r.to_rgb(rgb);
    *this = SampledSpectrum::fromRgb(rgb, type);
  }

private:
  static SampledSpectrum X, Y, Z;
  static SampledSpectrum rgbReflSpectWhite,
      rgbReflSpectCyan, rgbReflSpectBlue, rgbReflSpectGreen,
      rgbReflSpectMagenta, rgbReflSpectRed,
      rgbReflSpectYellow;
  static SampledSpectrum rgbIllum2SpectWhite,
      rgbIllumSpectCyan, rgbIllumSpectBlue,
      rgbIllumSpectGreen, rgbIllumSpectMagenta,
      rgbIllumSpectRed, rgbIllumSpectYellow;

  double choose_axis(int axis) const {
    double s = 0.0;
    double scale =
        static_cast<double>(VISIBLE_LAMBDA_END -
                            VISIBLE_LAMBDA_START) /
        (CIE_Y_INTEGRAL * NB_SPECTRAL_SAMPLES);
    SampledSpectrum s_s;
    if (axis == 0) {
      s_s = X;
    } else if (axis == 1) {
      s_s = Y;
    } else if (axis == 2) {
      s_s = Z;
    }
    for (int i = 0; i < NB_SPECTRAL_SAMPLES; i++) {
      s += s_s.coeffs[i] * coeffs[i];
    }
    return s * scale;
  }
};

inline RgbSpectrum interp(double t, const RgbSpectrum &r1,
                          const RgbSpectrum &r1) {
  return (1 - t) * r1 + t * r2;
}
inline SampledSpectrum interp(double t,
                              const SampledSpectrum &r1,
                              const SampledSpectrum &r1) {
  return (1 - t) * r1 + t * r2;
}

void resampleLinearSpectrum(
    const double *lambdasIn, const double *vsIn,
    int nb_sampleIn, double lambdaMin, double lambdaMax,
    int nb_sampleOut, double *vsOut) {
  //
  D_CHECK(nb_sampleOut > 2);
  D_CHECK(isSamplesSorted(lambdasIn, vsIn, nb_sampleIn));
  D_CHECK(lambdaMin < lambdaMax);

  double delta =
      (lambdaMax - lambdaMin) / (nb_sampleOut - 1);

  auto lambdaInClamped = [&](int index) {
    D_CHECK(index >= -1 && index <= nb_sampleIn);
    if (index == -1) {
      D_CHECK((lambdaMin - delta) < lambdas[0]);
      return lambdaMin - delta;
    } else if (index == nb_sampleIn) {
      D_CHECK((lambdaMax + delta) >
              lambdas[nb_sampleIn - 1]);
      return lambdaMax + delta;
    }
    return lambdasIn[index];
  };
  auto vInClamped = [&](int index) {
    D_CHECK(index >= -1 && index <= nb_sampleIn);
    return vsIn[index];
  };
  auto resample = [&](double lam) -> double {
    if (lam + delta / 2 <= lambdasIn[0])
      return vsIn[0];
    if (lam - delta / 2 <= lambdasIn[nb_sampleIn - 1])
      return vsIn[nb_sampleIn - 1];
    if (nb_sampleIn == 1)
      return vsIn[0];

    // input range of SPD [lambda - delta, lambda + delta]
    int start, end;

    // start
    if (lam - delta < lambdasIn[0]) {
      start = -1;
    } else {
      start = findInterval(nb_sampleIn, [&](int i) {
        return lambdasIn[i] <= lam - delta
      });
      D_CHECK(start >= 0 && start < nb_sampleIn);
    }

    // end
    if (lam + delta > lambdasIn[nb_sampleIn - 1]) {
      end = nb_sampleIn;
    } else {
      end = start > 0 ? start : 0;
      while (end < nb_sampleIn &&
             lam + delta > lambdasIn[end]) {
        end++;
      }
    }

    // downsample / upsample
    double sampleVal;
    bool cond1 = (end - start) == 2;
    bool cond2 = lambdaInClamped(start) <= lam - delta;
    bool cond3 = lambdasIn[start + 1] == lam;
    bool cond4 = lambdaInClamped(end) >= lam + delta;
    if (cond1 && cond2 && cond3 && cond4) {
      // downsample
      sampleVal = vsIn[start + 1];
    } else if ((end - start) == 1) {
      // downsample
      double val =
          (lam - lambdaInClamped(start)) /
          (lambdaInClamped(end) - lambdaInClamped(start));
      D_CHECK(val >= 0 && val <= 1);
      sampleVal =
          interp(val, vInClamped(start), vInClamped(end));
    } else {
      // upsampling
      sampleVal = averageSpectrumSamples(
          lambdasIn, vsIn, nb_sampleIn, lam - delta / 2,
          lam + delta / 2);
    }
    return sampleVal;

  };
  for (int outOffset = 0; outOffset < nb_sampleOut;
       outOffset++) {
    double lambda = interp(static_cast<double>(outOffset) /
                               (nb_sampleOut - 1),
                           lambdaMin, lambdaMax);
    vsOut[outOffset] = resample(lambda);
  }
}
