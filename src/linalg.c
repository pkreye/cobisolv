/* _outer_prod

   Assumes `result` has dimensions (`len_a`, `len_b`).
 */
void _outer_prod(int *a, size_t len_a, int *b, size_t len_b, int **result)
{
    size_t i, j;

    for (i = 0; i < len_a; i++) {
        for (j = 0; j < len_b; j++) {
            result[i][j] = a[i] * b[j];
        }
    }
}

void _scalar_mult(int *a, size_t len, int scalar)
{
    size_t i;
    for (i = 0; i < len; i++) {
        a[i] = a[i] * scalar;
    }
}

/* _transpose

   Returns newly allocated 2d array containing the result.
 */
int **_transpose(int **a, int w, int h)
{
    int **result = _malloc_array2d(h, w);

    int i, j;
    for (i = 0; i < w; i++) {
        for (j = 0; j < h; j++) {
            result[j][i] = a[i][j];
        }
    }

    return result;
}

/* Element-wise addition of 2 2D arrays. Result stored in first array. */
void _add_array2d(int **a, int **b, int w, int h)
{
    int i, j;
    for (i = 0; i < w; i++) {
        for (j = 0; j < h; j++) {
            a[i][j] += b[i][j];
        }
    }
}

int **_matrix_mult(int **a, int a_w, int a_h, int **b, int b_h)
{
    // b is assumed to have dimensions (a_h, b_h)
    // int b_w = a_h;

    int **result = _malloc_array2d(a_w, b_h);

    int i, j, k;
    for(i = 0; i < a_w; i++) { // each row of a
        for(j = 0; j < b_h; j++) { // each col in b
            result[i][j] = 0;
            for(k = 0; k < a_h; k++) { // each col a
                result[i][j] += a[i][k] * b[k][j];
            }
        }
    }

    return result;
}

// Elementwise multiplication of 2D arrays
int **_array2d_element_mult(int **a, int **b, int w, int h)
{
    int **result = _malloc_array2d(w, h);

    int i, j;
    for (i = 0; i < w; i++) {
        for (j = 0; j < h; j++) {
            result[i][j] = a[i][j] * b[i][j];
        }
    }

    return result;
}

// void cobi_simple_descent(int *spins, int **weights)
// {
//     // Assumption: NUM_GROUPS == len(weights) == len(weights[i]) == len(spins)
//     int size = NUM_GROUPS;

//     int neg_spins[NUM_GROUPS];
//     for (int i = 0; i < size; i++) {
//         // _scalar_mult(neg_spins, size, -1);
//         neg_spins[i] = spins[i] * -1;
//     }

//     int **cross = _malloc_array2d(size, size);
//     // int cross[NUM_GROUPS][NUM_GROUPS]
//     _outer_prod(spins, size, neg_spins, size, cross);

//     // # prod = cross * (weights+weights.transpose())
//     int **wt_transpose = _transpose(weights, size, size);
//     // add weights to wt_transpose, storing result in wt_transpose
//     _add_array2d(wt_transpose, weights, size, size);
//     int **prod = _array2d_element_mult(cross, wt_transpose, size, size);

//     // # diffs = np.sum(prod,0)
//     int diffs[NUM_GROUPS];
//     // Column-wise sum
//     for (int i = 0; i < size; i++) {
//         diffs[i] = 0;
//         for (int j = 0; j < size; j++) {
//             diffs[i] += prod[j][i];
//         }
//     }
//     // TODO fix recursive structure...
//     _free_array2d((void**)cross, size);
//     _free_array2d((void**)wt_transpose, size);
//     _free_array2d((void**)prod, size);

//     for (int i = 0; i < size; i++) {
//         if (diffs[i] < 0) {
//             spins[i] *= -1;

//             return cobi_simple_descent(spins, weights);
//         }
//     }
// }
