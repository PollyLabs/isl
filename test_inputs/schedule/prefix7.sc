# Test case with invalid prefix.
# Result is arbitrary.
domain: [n] -> { A[i, j] : 0 <= i, j < n; B[i, j] : 0 <= i, j < n }
validity: { A[i, j] -> B[i', j'] }
prefix: [{ A[i, j] -> [(i)] }, { A[i, j] -> [(j)] }]
