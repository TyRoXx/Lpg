# Equality
This contains the interface a structure needs to be comparable and the function to compare it to.

## Equals
`let equals = [T](first: T, second: T): boolean`
> Comparing two elements of type T and calling the equality function on the first element. So this is equals to: first.equals(second)

## Equality comparison
```lpg
let equality_comparison = interface[T]
    equals(other: T): boolean
```

# Algorithm
In the algorithm module there are several functions that can be useful.

## Enumerate
`let enumerate = [H](first: integer, last: integer, on_element: H): std.unit`
> This is one way of iterating over a list of integers from first as the lower bound to last as the upper bound. The `on_element` parameter defines the function that is called with every number of the range.

For example:
`enumerate[type_of(a)](1, 5, a)` is the same as: `a(1) a(2) a(3) a(4) a(5)`

## Any of
`let any_of = [E, P](elements: ranges.random_access[E], test_element: P): boolean`
> This function checks if any element from the random access data structure satisfy the function `test_element` and returns the result as a boolean.

## Find
`let find = [E](haystack: ranges.random_access[E], needle: E): std.option[integer]`
> This function returns the index of a given object in a random access data structure or none if it doesn't exist.

# Set
Are a collection of elements without duplicates. For an element to be used in a `set` it has to implement the `equality_comparision` interface. Providing the following interface:
```lpg
let set = interface[T]
    contains(key: T): std.boolean
    remove(key: T): std.boolean
    add(key: T): std.boolean
    clear(): std.unit
```

## Linear set
A set similar to the array.
