domain: "[N, M] -> { S_7[i, j] : 2*floor((i)/2) = i and 2*floor((j)/2) = j and 0 <= i <= -2 + N and 0 <= j <= -2 + M; S_4[i, j] : 2*floor((i)/2) = i and 2*floor((j)/2) = j and 0 <= i <= -2 + N and 0 <= j <= -2 + M; S_5[i, j] : 2*floor((i)/2) = i and 2*floor((j)/2) = j and 0 <= i <= -2 + N and 0 <= j <= -2 + M; S_11[i, j] : 2*floor((i)/2) = i and 2*floor((j)/2) = j and 0 <= i <= -2 + N and 0 <= j <= -2 + M; S_9[i, j] : 2*floor((i)/2) = i and 2*floor((j)/2) = j and 0 <= i <= -2 + N and 0 <= j <= -2 + M; S_8[i, j] : 2*floor((i)/2) = i and 2*floor((j)/2) = j and 0 <= i <= -2 + N and 0 <= j <= -2 + M; S_6[i, j] : 2*floor((i)/2) = i and 2*floor((j)/2) = j and 0 <= i <= -2 + N and 0 <= j <= -2 + M; S_10[i, j] : 2*floor((i)/2) = i and 2*floor((j)/2) = j and 0 <= i <= -2 + N and 0 <= j <= -2 + M }"
child:
  sequence:
  - filter: "[N, M] -> { S_4[i, j]; S_8[i, j]; S_6[i, j]; S_10[i, j] }"
    child:
      schedule: "[N, M] -> [{ S_4[i, j] -> [(i)]; S_8[i, j] -> [(1 + i)]; S_6[i, j] -> [(i)]; S_10[i, j] -> [(1 + i)] }, { S_4[i, j] -> [(j)]; S_8[i, j] -> [(j)]; S_6[i, j] -> [(1 + j)]; S_10[i, j] -> [(1 + j)] }]"
      permutable: 1
      coincident: [ 1, 1 ]
  - filter: "[N, M] -> { S_7[i, j]; S_5[i, j]; S_11[i, j]; S_9[i, j] }"
    child:
      schedule: "[N, M] -> [{ S_7[i, j] -> [(1 + j)]; S_5[i, j] -> [(j)]; S_11[i, j] -> [(1 + j)]; S_9[i, j] -> [(j)] }, { S_7[i, j] -> [(i)]; S_5[i, j] -> [(i)]; S_11[i, j] -> [(1 + i)]; S_9[i, j] -> [(1 + i)] }]"
      permutable: 1
      coincident: [ 1, 1 ]
