domain: "[N] -> { S[i, j] : i >= 0 and 0 <= j < N - i }"
child:
  schedule: "[N] -> [{ S[i, j] -> [(i + j)] }, { S[i, j] -> [(i)] }]"
  permutable: 1
  coincident: [ 1, 1 ]
