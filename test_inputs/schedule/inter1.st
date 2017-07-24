domain: "[N] -> { S_1[i] : 0 <= i < N; S_0[i] : 0 <= i < N }"
child:
  schedule: "[N] -> [{ S_1[i] -> [(N + i)]; S_0[i] -> [(i)] }]"
  permutable: 1
  coincident: [ 1 ]
