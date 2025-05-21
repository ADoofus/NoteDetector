#include <iostream>
#include <fstream>
#include <vector>
#include <array>
#include <numeric>
#include <algorithm>
#include <cstdio>
#include <type_traits>
#include <cmath>
#include <limits>

using std::array, std::vector, std::size_t;

using sample_type = double;            // data sample type, either int or double
constexpr int Global_Filter_N = 41; // filter length, must be odd
constexpr int min_peak_spacing = 500;

// vector<sample_type> high_pass_filter(const vector<sample_type>& vin, sample_type alpha = 0.95)
// {
//     vector<sample_type> vout;
//     if (vin.empty()) return vout;

//     vout.reserve(vin.size());
//     sample_type x_prev = vin[0];
//     sample_type y_prev = 0;

//     vout.push_back(0); // first output is assumed to be 0
//     for (size_t i = 1; i < vin.size(); ++i)
//     {
//         sample_type y = alpha * (y_prev + vin[i] - x_prev);
//         vout.push_back(y);
//         y_prev = y;
//         x_prev = vin[i];
//     }

//     return vout;
// }


// moving average filter
template <typename T=sample_type, int N=Global_Filter_N>
class Filter_MA
{
public:
    T clk(T in)
    {
        sum += in - buf[index];
        buf[index] = in;
        index = (index + 1) % N;
        if constexpr (std::is_floating_point_v<T>)
            return sum / N;
        else
            return (sum + (N / 2)) / N;
    }
    bool update_vectors(const vector<T>& vin, vector<T>* pvout, vector<T>* prawout = nullptr)
    {
        if (vin.size() <= N || pvout == nullptr)
            return false;
        pvout->reserve(vin.size() - N);
        if (prawout != nullptr)
            pvout->reserve(vin.size() - N);
        for (size_t i = 0; i < N; i++)
            clk(vin[i]);
        for (size_t i = N; i < vin.size(); i++)
        {
            pvout->push_back(clk(vin[i]));
            if (prawout != nullptr)
                prawout->push_back(vin[i - N / 2]);
        }
        return true;
    }
private:
    array<T, N> buf{};  // moving average buffer
    T sum{};            // running sum of buffer
    size_t index{};     // current loc remove output, add input 
};

template <typename T=sample_type>
std::pair<T, T> peak_detect(T y1, T y2, T y3)
{
    // scale pk location by 100 to work with int arith
    T pk = 100* (y1 - y3) / (2 * (y1 - 2 * y2 + y3));
    T mag =  2 * y2 - y1 - y3;
    return std::pair{ pk, mag };
}

struct WaveInfo {
    sample_type w_mean{};
    sample_type w_max{};
    sample_type w_min{};
    vector<sample_type> peaks;
    vector<sample_type> mags;
};

inline WaveInfo get_wave_info(std::vector<sample_type> v)
{
    constexpr int N = Global_Filter_N;
    static_assert(Global_Filter_N & 1, "filter must be odd number");
    WaveInfo w;
    w.w_max = *std::max_element(v.begin(), v.end());
    w.w_min = *std::min_element(v.begin(), v.end());
    // "0ll + sample_type{}" Produces either a double or long long int depending on sample_type to stop overflow if > 2M samples
    w.w_mean = static_cast<sample_type>(std::accumulate(v.begin(), v.end(), 0ll + sample_type{}) / std::size(v));
    sample_type pos_thresh = w.w_mean + (w.w_max - w.w_mean) / 5;  // 10% above ave.
    sample_type neg_thresh = 1000000000000000000;
    sample_type lastVal = 0;
    // double nextCoef = 10000;
    int search_polarity = 0;    // if 0 prior peak polarity not determined
    int lastPeakIndex = -min_peak_spacing;

    for (int i = 0; i < int(v.size()) - N; i++)
    {
        if (i-lastPeakIndex < min_peak_spacing) { continue; }
        const int center = N/2;
        // /(v[i] - lastVal) < (w.w_max * nextCoef) && 
        if (v[i] > pos_thresh && v[i] > v[i + N - 1] && v[i] < v[i + center] && search_polarity >= 0)
        {
            search_polarity = -1;
            auto results = peak_detect(v[i], v[i + center], v[i + N - 1]);
            w.peaks.push_back(results.first * center / 100 + i + center);
            w.mags.push_back(results.second);
            lastPeakIndex = i;
        }
        if (v[i] < neg_thresh && v[i] < v[i + N - 1] && v[i] > v[i + center] && search_polarity <= 0)
        {
            search_polarity = 1;
            auto results = peak_detect(v[i], v[i + N / 2], v[i + N - 1]);
            w.peaks.push_back(results.first * center / 100 + i + center);
            w.mags.push_back(-results.second);
            lastPeakIndex = i;
        }
        lastVal = v[i];
    }
    return w;
}

// Used to get text file int samples
// vector<sample_type> get_raw_data()
// {
//     std::ifstream in("raw_data.txt");
//     vector<sample_type> v;
//     int x;
//     while(in >> x)
//         v.push_back(x);
//     return v;
// }

// int main()
// {
//     Filter_MA filter;
//     vector<sample_type> vn = get_raw_data();
//     vector<sample_type> vfiltered;
//     vector<sample_type> vraw;
//     if (!filter.update_vectors(vn, &vfiltered, &vraw))
//         return 1;   // exit if update failed

//     // file with aligned raw and filtered data
//     std::ofstream out("waves.txt");
//     for (size_t i = 0; i < vfiltered.size(); i++)
//         out << vraw[i] << " " << vfiltered[i] << '\n';

//     // get filtered file metrics
//     WaveInfo info = get_wave_info(vfiltered);
//     out.close();

//     //  file with peak locs and magnitudes
//     out.open("peaks.txt");
//     for (size_t i = 0; i < info.peaks.size(); i++)
//         out << info.peaks[i] << " " << info.mags[i] << '\n';
// }