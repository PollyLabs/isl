domain: "[N] -> { S_1[i] : 2*floor((i)/2) = i and 0 <= i < N; S_0[i] : 2*floor((i)/2) = i and 0 <= i < N }"
child:
  schedule: "[N] -> [{ S_1[i] -> [(1 + i)]; S_0[i] -> [(i)] }]"
  permutable: 1
  coincident: [ 1 ]
