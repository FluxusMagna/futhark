-- Test u8.mul_hi
-- ==
-- entry: test_u8_mul_hi
-- input  { [10u8, 20u8, 2u8, 1u8, 2u8, 3u8, 2u8]
--          [10u8, 20u8, 127u8, 255u8, 255u8, 127u8, 128u8] }
-- output { [0u8, 1u8, 0u8, 0u8, 1u8, 1u8, 1u8] }

entry test_u8_mul_hi (xs: []u8) (ys: []u8) =
  map2 u8.mul_hi xs ys

-- Test i8.mul_hi
-- ==
-- entry: test_i8_mul_hi
-- input  { [10i8, 20i8, 2i8, 1i8, 2i8, 3i8, 2i8]
--          [10i8, 20i8, 127i8, -1i8, -1i8, 127i8, 128i8] }
-- output { [0i8, 1i8, 0i8, 0i8, 1i8, 1i8, 1i8] }

entry test_i8_mul_hi (xs: []i8) (ys: []i8) =
  map2 i8.mul_hi xs ys

-- Test u16.mul_hi
-- ==
-- entry: test_u16_mul_hi
-- input  { [10u16, 20u16, 2u16, 3u16, 2u16, 1u16, 2u16, 2u16, 2u16, 3u16, 65535u16]
--          [10u16, 20u16, 127u16, 127u16, 128u16, 255u16, 255u16, 32768u16, 65535u16, 65535u16, 65535u16] }
-- output { [0u16, 0u16, 0u16, 0u16, 0u16, 0u16, 0u16, 1u16, 1u16, 2u16, 65534u16] }

entry test_u16_mul_hi (xs: []u16) (ys: []u16) =
  map2 u16.mul_hi xs ys

-- Test i16.mul_hi
-- ==
-- entry: test_i16_mul_hi
-- input  { [10i16, 20i16, 2i16, 3i16, 2i16, 1i16, 2i16, 2i16, 2i16, 3i16, -1i16]
--          [10i16, 20i16, 127i16, 127i16, 128i16, 255i16, 255i16, 32768i16, -1i16, -1i16, -1i16] }
-- output { [0i16, 0i16, 0i16, 0i16, 0i16, 0i16, 0i16, 1i16, 1i16, 2i16, -2i16] }

entry test_i16_mul_hi (xs: []i16) (ys: []i16) =
  map2 i16.mul_hi xs ys

-- Test u32.mul_hi
-- ==
-- entry: test_u32_mul_hi
-- input  { [10u32, 20u32, 2u32, 3u32, 2u32, 1u32, 2u32, 2u32, 2u32, 3u32, 65535u32, 1u32, 2u32, 5u32, 4294967295u32]
--          [10u32, 20u32, 127u32, 127u32, 128u32, 255u32, 255u32, 32768u32, 65535u32, 65535u32, 65535u32, 4294967295u32, 4294967295u32, 4294967295u32, 4294967295u32] }
-- output { [0u32, 0u32, 0u32, 0u32, 0u32, 0u32, 0u32, 0u32, 0u32, 0u32, 0u32, 0u32, 1u32, 4u32, 4294967294u32] }

entry test_u32_mul_hi (xs: []u32) (ys: []u32) =
  map2 u32.mul_hi xs ys

-- Test i32.mul_hi
-- ==
-- entry: test_i32_mul_hi
-- input  { [10i32, 20i32, 2i32, 3i32, 2i32, 1i32, 2i32, 2i32, 2i32, 3i32, 65535i32, 1i32, 2i32, 5i32, -1i32]
--          [10i32, 20i32, 127i32, 127i32, 128i32, 255i32, 255i32, 32768i32, 65535i32, 65535i32, 65535i32, -1i32, -1i32, -1i32, -1i32] }
-- output { [0i32, 0i32, 0i32, 0i32, 0i32, 0i32, 0i32, 0i32, 0i32, 0i32, 0i32, 0i32, 1i32, 4i32, -2i32] }

entry test_i32_mul_hi (xs: []i32) (ys: []i32) =
  map2 i32.mul_hi xs ys

-- Test u64.mul_hi
-- ==
-- entry: test_u64_mul_hi

-- input  { [10u64, 20u64, 2u64, 3u64, 2u64, 1u64, 2u64, 2u64, 2u64, 3u64, 65535u64, 1u64, 2u64 5u64, 4294967295u64, 1u64, 2u64, 18446744073709551615u64]
--          [10u64, 20u64, 127u64, 127u64, 128u64, 255u64, 255u64, 32768u64, 65535u64, 65535u64, 65535u64, 4294967295u64, 4294967295u64, 4294967295u64, 4294967295u64, 18446744073709551615u64, 18446744073709551615u64, 18446744073709551615u64] }
-- output { [0u64, 0u64, 0u64, 0u64, 0u64, 0u64, 0u64, 0u64, 0u64, 0u64, 0u64, 0u64, 0u64, 0u64, 0u64, 0u64, 1u64, 18446744073709551614u64] }

entry test_u64_mul_hi (xs: []u64) (ys: []u64) =
  map2 u64.mul_hi xs ys

-- Test i64.mul_hi
-- ==
-- entry: test_i64_mul_hi
-- input  { [10i64, 20i64, 2i64, 3i64, 2i64, 1i64, 2i64, 2i64, 2i64, 3i64, 65535i64, 1i64, 2i64, 5i64, 4294967295i64, 1i64, 2i64, -1i64]
--          [10i64, 20i64, 127i64, 127i64, 128i64, 255i64, 255i64, 32768i64, 65535i64, 65535i64, 65535i64, 4294967295i64, 4294967295i64, 4294967295i64, 4294967295i64, -1i64, -1i64, -1i64] }
-- output { [0i64, 0i64, 0i64, 0i64, 0i64, 0i64, 0i64, 0i64, 0i64, 0i64, 0i64, 0i64, 0i64, 0i64, 0i64, 0i64, 1i64, -2i64] }

entry test_i64_mul_hi (xs: []i64) (ys: []i64) =
  map2 i64.mul_hi xs ys