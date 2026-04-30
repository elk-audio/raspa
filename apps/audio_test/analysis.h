#ifndef AUDIO_TEST_ANALYSIS_H
#define AUDIO_TEST_ANALYSIS_H
#include <vector>
#include <numeric>
#include <valarray>
#include <complex>
#include <cassert>

using Complex = std::complex<double>;

namespace at {

constexpr float MIN_120DB = 0.000001f;

inline float to_db(float lin)
{
    return  20.0f * std::log10(std::max(lin, MIN_120DB));
}

inline float db_to_gain(float db)
{
    return  std::pow(10, db / 20.0f);
}

inline float to_log(float value, float start, float end)
{
    return start * powf(2, value / end * 10.0f);
}

inline float calc_rms(const std::vector<float>& input)
{
    return std::sqrt(std::accumulate(input.begin(), input.end(), 0.0f, [](const float& a, const float& b){return a + b * b;}) / input.size());
}

inline void normalize(std::vector<float>& input)
{
    auto [min, max] = std::minmax_element(input.begin(), input.end());
    float scale = 1.0f / std::max(*max , std::abs(*min));
    std::transform(input.begin(), input.end(), input.begin(), [&](const float& x){return x * scale;});
}

inline float hann_window(float pos, int length)
{
    float w = sin(M_PI * pos / length);
    return w * w;
}

/* Cooley Tukey form https://tfetimes.com/c-fast-fourier-transform/ */
inline void recursive_fft(std::valarray<Complex>& x)
{
    const auto num_points = x.size();
    if (num_points <= 1)
    {
        return;
    }

    std::valarray <Complex> even = x[std::slice(0, num_points / 2, 2)];
    std::valarray <Complex> odd = x[std::slice(1, num_points / 2, 2)];

    recursive_fft(even);
    recursive_fft(odd);

    // combine
    for (size_t k = 0; k < num_points / 2; k++)
    {
        Complex t = std::polar(1.0, -2 * M_PI * k / num_points) * odd[k];
        x[k] = even[k] + t;
        x[k + num_points / 2] = even[k] - t;
    }
}

inline std::valarray <Complex> fft(const std::vector<float>& signal)
{
    assert(signal.size() > 2);

    auto x = std::valarray<Complex>(0.0, signal.size());
    std::copy(signal.begin(), signal.end(), begin(x));
    recursive_fft(x);
    return x;
}

inline std::vector<float> complex_abs(const std::valarray<Complex>& x)
{
    std::vector<float> result;
    for (const auto& x : x)
    {
        result.push_back(std::abs(x));
    }
    return result;
}

inline std::vector<float> xcorr(const std::vector<float>& a, const std::vector<float>& b)
{
    std::vector<float> result;
    for (int i = 0; i < a.size(); ++i)
    {
        float sum = 0.0f;
        for (int j = 0 ; j <= i ; ++j)
        {
            sum += a[a.size() - 1 -i + j] * b[j];
        }
        result.push_back(sum);
     }
    for (int i = 1; i < a.size(); ++i)
    {
        float sum = 0.0f;
        for (int j = 0 ; j <= a.size() - 1 - i ; ++j)
        {
            sum += a[j] * b[i + j];
        }
        result.push_back(sum);
    }
    result.push_back(0.0); //
    return result;
}

}

#endif //AUDIO_TEST_ANALYSIS_H