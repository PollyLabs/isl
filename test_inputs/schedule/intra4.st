domain: "[N] -> { S[i, j] : i >= 0 and i <= j < N }"
child:
  schedule: "[N] -> [{ S[i, j] -> [(j)] }, { S[i, j] -> [(-i)] }]"
  permutable: 1
  coincident: [ 1, 1 ]
