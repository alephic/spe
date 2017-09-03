
SPE is a functional language.

The syntax is as follows:

Symbol: a sequence of non-reserved characters (reserved characters: <>[]{}()*? )
Apply: a closed (parenthesized or single-term) expression followed by another expression
  example: f x y
  In this example, "f x y" is equivalent to "(f x) y", where f is applied to x and the result is applied to y
Lambda: an angle-bracketed symbol followed by another expression
  example: <x> y
Declare: a curly-braced "declared" expression followed by another expression
  example: {a x} y
Constrain: a square-bracketed "constraint" expression followed by another expression
  example: [b x] x
Wildcard: an asterisk
  example: *
Arbitrary: a question mark
  example: ?

Lambdas and Applies work like they do in lambda calculus:

> (<x> x) a
a
> (<x> <y> c x y) a b
c a b

Declares, consisting of the curly-braced "declared" expression, establish that the "declared" expression holds within their body.
Constrains limit the domain of their body to that in which their constraint holds:

> {a} [a] c
c
> {a} [b] c
!Empty set

Wildcards represent all possible values:

> {a x} {a y} (<z> [a z] z) *
x
y

Arbitraries represent a unique anonymous value:

> {a x} (<a> {a y} (<z> [a z] z) *) ?
y

In the SPE repl, the command :def (name) (expr) introduces a binding for the specified name:

> :def id <x> x
> id a
a

The command :decl (expr) introduces a declaration:

> :decl = (c a) b
> (<x> [= (c x) b] x) a
a
> (<x> [= (c x) b] x) d
!Empty set