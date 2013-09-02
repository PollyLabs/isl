domain: "[n] -> { S[i, j] : 0 <= i < n and 0 <= j < n }"
child:
  schedule: "[n] -> [{ S[i, j] -> [(j)] }]"
  permutable: 1
  coincident: [ 1 ]
