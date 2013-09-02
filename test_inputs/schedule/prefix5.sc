domain: [n] -> { A[i, j] : 0 <= i, j < n; B[i, j] : 0 <= i, j < n }
validity: { A[i, j] -> B[i', j'] }
prefix: [{ A[i, j] -> [(i)]; B[i,j] -> [i + j] }, { A[i, j] -> [(j)]; B[i,j] -> [0] }]
