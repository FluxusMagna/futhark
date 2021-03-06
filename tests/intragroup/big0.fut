-- This needs more than 2**31 threads, but its input isn't that big.
-- ==
-- tags { no_python }
-- compiled random input { [10000000]f32 } auto output

let main (xs: []f32) =
  map (\x -> iota 256 |> map r32 |> map (+x) |> scan (+) 0 |> f32.sum) xs
