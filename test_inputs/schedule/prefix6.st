domain: "[n] -> { S[i, j, k] : 0 <= i < n and 0 <= j < n and 0 <= k < n }"
child:
  schedule: "[n] -> [{ S[i, j, k] -> [(j)] }, { S[i, j, k] -> [(k)] }]"
  permutable: 1
  coincident: [ 1, 1 ]
