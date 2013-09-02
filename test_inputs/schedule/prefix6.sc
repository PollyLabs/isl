domain: [n] -> { S[i, j, k] : 0 <= i, j, k < n }
prefix: [{ S[i, j, k] -> [i] : i >= 0; S[i, j, k] -> [i + k] : i < 0 }, { S[i, j, k] -> [i + j] : i >= 0; S[i, j, k] -> [k] : i < 0 }]
