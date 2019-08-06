# Algorithm
In the alogrithm module there are several functions that can be useful.

`let enumerate = [H](first: integer, last: integer, on_element: H): std.unit`
> This is one way of iterating over a list of integers from first as the lower bound to last as the upper bound. The on_element parameter defines the function that is called with every number of the range. For example
> enumerate(1, 5, a) is the same as:
> a(1)
> a(2)
> a(3)
> a(4)
> a(5)

`let any_of = [E, P](elements: ranges.random_access[E], test_element: P): boolean`
> This function checks if any element from the random access data structure satisfy the function `test_element` and returns the index of it.

`let find = [E](haystack: ranges.random_access[E], needle: E): std.option[integer]`
> This function returns the index of a given object in a random access data structure or none if it doesn't exist.

