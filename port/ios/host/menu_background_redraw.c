/* Apache-2.0. Source-fitted menu band and source-preserved frame strips. */
#include "menu_background_redraw.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int stopped(const volatile int *cancel) { return cancel && *cancel; }
static double clamp(double v) { return fmin(255, fmax(0, v)); }
int menu_background_redraw_source(const uint8_t *source, unsigned width, unsigned height)
{
    if (!source || width != 4 || height != 512) return -1;
    uint64_t hash = UINT64_C(14695981039346656037);
    for (size_t i = 0; i < (size_t)width * height * 4; i++)
        hash = (hash ^ source[i]) * UINT64_C(1099511628211);
    return hash == UINT64_C(0x09acbf2862befc24) ? 0 : -1;
}

/* The band is gently curved, rather than linear. Fit each channel from the
   player's interior texels, excluding the line and its one-row alpha transition. */
static int fit(const uint8_t *source, double coefficients[4][4])
{
    double matrix[4][8] = {{0}};
    for (unsigned y = 72; y < 409; y++) {
        double t = (y - 240.0) / 168.0, powers[7] = {1};
        for (unsigned i = 1; i < 7; i++) powers[i] = powers[i - 1] * t;
        for (unsigned i = 0; i < 4; i++) {
            for (unsigned j = 0; j < 4; j++) matrix[i][j] += powers[i + j];
            for (unsigned c = 0; c < 4; c++) {
                double value = 0;
                for (unsigned x = 0; x < 4; x++) value += source[(y * 4 + x) * 4 + c] / 4.0;
                matrix[i][4 + c] += powers[i] * value;
            }
        }
    }
    for (unsigned i = 0; i < 4; i++) {
        unsigned pivot = i;
        for (unsigned j = i + 1; j < 4; j++)
            if (fabs(matrix[j][i]) > fabs(matrix[pivot][i])) pivot = j;
        if (fabs(matrix[pivot][i]) < 1e-10) return 0;
        for (unsigned j = 0; j < 8; j++) {
            double v = matrix[i][j]; matrix[i][j] = matrix[pivot][j]; matrix[pivot][j] = v;
        }
        double divisor = matrix[i][i];
        for (unsigned j = i; j < 8; j++) matrix[i][j] /= divisor;
        for (unsigned row = 0; row < 4; row++) if (row != i) {
            double factor = matrix[row][i];
            for (unsigned j = i; j < 8; j++) matrix[row][j] -= factor * matrix[i][j];
        }
    }
    for (unsigned i = 0; i < 4; i++)
        for (unsigned c = 0; c < 4; c++) coefficients[c][i] = matrix[i][4 + c];
    return 1;
}

int menu_background_redraw(const uint8_t *source, unsigned width, unsigned height,
    uint8_t *rgba, unsigned scale, const volatile int *cancel,
    struct menu_background_redraw_metrics *metrics)
{
    clock_t start = clock();
    if (metrics) memset(metrics, 0, sizeof(*metrics));
    if (!rgba || (scale != 1 && scale != 2 && scale != 4) ||
        menu_background_redraw_source(source, width, height) < 0) return 0;
    if (stopped(cancel)) return -1;
    double coefficients[4][4];
    if (!fit(source, coefficients)) return 0;
    size_t bytes = (size_t)width * height * scale * scale * 4;
    uint8_t *result = malloc(bytes);
    if (!result) return 0;
    for (unsigned y = 0; y < height * scale; y++) {
        if (stopped(cancel)) { free(result); return -1; }
        double sy = (y + 0.5) / scale - 0.5;
        unsigned row = y / scale;
        uint8_t pixel[4];
        for (unsigned c = 0; c < 4; c++) {
            double value;
            if (row >= 72 && row < 409) {
                double t = (sy - 240.0) / 168.0;
                value = ((coefficients[c][3] * t + coefficients[c][2]) * t + coefficients[c][1]) * t + coefficients[c][0];
            } else {
                /* Preserve the source's line positions and opacities. Averaging
                   the four equivalent columns removes sub-byte alpha noise. */
                value = 0;
                for (unsigned x = 0; x < width; x++) value += source[(row * width + x) * 4 + c] / 4.0;
            }
            pixel[c] = (uint8_t)lround(clamp(value));
        }
        for (unsigned x = 0; x < width * scale; x++) memcpy(result + ((size_t)y * width * scale + x) * 4, pixel, 4);
    }
    if (stopped(cancel)) { free(result); return -1; }
    memcpy(rgba, result, bytes); free(result);
    if (metrics) { metrics->seconds = (double)(clock() - start) / CLOCKS_PER_SEC; metrics->scratch_bytes = bytes; }
    return 1;
}
