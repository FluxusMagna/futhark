-- Can we nest a parametric module?
-- ==
-- input { 3 } output { 6 }

module type MT1 = {
  val f: i32 -> i32 -> i32
}

module M = {
  module T(P: MT1) = {
    fun g(x: i32) = P.f x x
  }
}

module T = M.T({fun f (x: i32) (y: i32) = x + y})

fun main (x: i32) = T.g x
