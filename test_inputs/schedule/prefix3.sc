domain: [n] -> { S[i, j] : 0 <= i < n and 0 <= j < n }
prefix: [{ S[i, j] -> [floor(i/32)] }, { S[i, j] -> [(j mod 32)] }]
