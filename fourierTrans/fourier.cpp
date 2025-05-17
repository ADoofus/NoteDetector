#include <iostream>
#include <complex>
#include <vector>
#include <cmath>

using namespace std;
using Complex = complex<double>;
using CArray = vector<Complex>;

const double PI = acos(-1);
std::vector<double> FFTfreq(int N, double d = 1.0) {
    std::vector<double> freqs(N);
    int N_half = (N - 1) / 2 + 1; // ceiling(N/2)

    for (int i = 0; i < N_half; ++i)
        freqs[i] = i / (N * d);
    for (int i = N_half; i < N; ++i)
        freqs[i] = (i - N) / (N * d);

    return freqs;
}
void FFT(CArray &x) {
    const size_t N = x.size();
    if (N <= 1) return;

    // Divide
    CArray even(N / 2);
    CArray odd(N / 2);
    for (size_t i = 0; i < N / 2; ++i) {
        even[i] = x[i * 2];
        odd[i] = x[i * 2 + 1];
    }

    // Conquer
    FFT(even);
    FFT(odd);

    // Combine
    for (size_t k = 0; k < N / 2; ++k) {
        Complex t = polar(1.0, -2 * PI * k / N) * odd[k];
        x[k] = even[k] + t;
        x[k + N / 2] = even[k] - t;
    }
}