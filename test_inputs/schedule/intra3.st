domain: "[N] -> { S[i, j] : i >= 0 and i <= j < N - i }"
child:
  schedule: "[N] -> [{ S[i, j] -> [(i + j)] }, { S[i, j] -> [(-2i)] }]"
  permutable: 1
  coincident: [ 1, 1 ]
