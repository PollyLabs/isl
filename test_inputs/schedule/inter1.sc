domain: [N] -> { S_0[i] : 0 <= i < N; S_1[i] : 0 <= i < N }
intra_consecutivity: ([N] -> { S_0[i] -> [[] -> [(i)]] },[N] -> { S_1[i] -> [[] -> [(i)]] },[N] -> { S_0[i] -> intra_0[[] -> [(i)]] },[N] -> { S_1[i] -> intra_1[[] -> [(N + i)]] })
inter_consecutivity: ([N] -> { [S_0[i = 0] -> intra_0[]] -> [S_1[i' = 0] -> intra_1[]] : N = 1; [S_0[i = -1 + N] -> intra_0[]] -> [S_1[i' = 0] -> intra_1[]] : N >= 2 })
