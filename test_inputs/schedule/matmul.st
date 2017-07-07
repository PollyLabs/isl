domain: "[N, M, K] -> { B[i, j, k] : 0 <= i < M and 0 <= j < N and 0 <= k < K; A[i, j] : 0 <= i < M and 0 <= j < N }"
child:
  schedule: "[N, M, K] -> [{ B[i, j, k] -> [(i)]; A[i, j] -> [(i)] }, { B[i, j, k] -> [(k)]; A[i, j] -> [(0)] }, { B[i, j, k] -> [(j)]; A[i, j] -> [(j)] }]"
  permutable: 1
  coincident: [ 1, 1, 1 ]
  child:
    sequence:
    - filter: "[N, M, K] -> { A[i, j] }"
    - filter: "[N, M, K] -> { B[i, j, k] }"
