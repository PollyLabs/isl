domain: "[n] -> { B[i, j] : 0 <= i < n and 0 <= j < n; A[i, j] : 0 <= i < n and 0 <= j < n }"
child:
  sequence:
  - filter: "[n] -> { A[i, j] }"
  - filter: "[n] -> { B[i, j] }"
    child:
      schedule: "[n] -> [{ B[i, j] -> [(i)] }]"
      permutable: 1
      coincident: [ 1 ]
