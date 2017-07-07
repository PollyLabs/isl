domain: "[N] -> { A[i, j] : 0 <= i < N and 0 <= j < N; B[i, j, k] : 0 <= i < N and 0 <= j < N and 0 <= k < N; C[i, j, k] : 0 <= i < N and 0 <= j < N and 0 <= k < N }"
child:
  schedule: "[N] -> [{ A[i, j] -> [(i)]; B[i, j, k] -> [(i)]; C[i, j, k] -> [(i)] }, { A[i, j] -> [(-j)]; B[i, j, k] -> [(-j)]; C[i, j, k] -> [(-j)] }]"
  permutable: 1
  coincident: [ 1, 1 ]
  child:
    schedule: "[N] -> [{ A[i, j] -> [(0)]; B[i, j, k] -> [(k)]; C[i, j, k] -> [(k)] }]"
    permutable: 1
    coincident: [ 1 ]
    child:
      sequence:
      - filter: "[N] -> { A[i, j] }"
      - filter: "[N] -> { B[i, j, k] }"
      - filter: "[N] -> { C[i, j, k] }"
