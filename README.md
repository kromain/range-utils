# range-utils
A set of container adapters for use with C++ range-for loops

## make_reversible()

This helper allows iterating backwards over any container that supports `begin()`/`end()` and `rbegin()`/`rend()` within a range-for loop.
The extra boolean parameter allows toggling forward/backward iteration at runtime with a single for-loop body.

Usage example:

```cpp
const QVector<int> values = {0, 1, 2, 3};
bool revert = true;
for (int&& value : make_reversible(values, revert)) {
    qDebug() << value;
}
// will print:
// 3
// 2
// 1
// 0

revert = false;
for (int&& value : make_reversible(values, revert)) {
    qDebug() << value;
}
// will print:
// 0
// 1
// 2
// 3
```

## make_synchronized()

This helper allows iterating over any number of containers that support `begin()`/`end()` at once within a range-for loop.

The range iterator returned by this helper returns a `std::tuple` with the current values for each container,
which allows extracting the values as structured bindings with c++17, and it works for any number of containers at the same time.

If the containers do not have the same element count (ie. don't take the same number of iterations to go from `begin()` to `end()`),
then iteration stops when any of the iterators reaches `end()`.

Usage example:

```cpp
const QVector<int> values = {0, 1, 2, 3, 4, 5};
const QStringList labels = {"0", "1", "2", "3"};
for(auto&& [value, label] : make_synchronized(values, labels)) {
   qDebug() << value << "->" label;
}
// will print:
// 0 -> "0"
// 1 -> "1"
// 2 -> "2"
// 3 -> "3"
```

## make_keyval()

This helper allows iterating over both keys and values of a `QMap` or `QHash` container within a range-for loop.

The range iterator returned by this helper returns a `std::pair<key,value>` for each element in the container,
which allows extracting both key and value together as structured bindings with c++17.

The helper is non-mutating, so both the container and the key-value pairs are handled as const-references.
This means wrapping the container with `qAsConst` is not required when using this helper, even for non-const lvalues.
Passing temporary objects is also supported, with the lifetime of the temporary automatically extended
to the end of the iteration.

Usage example:

```cpp
const QMap<int, QString> digitMap = { {1, "one"}, {2, "two"}, {3, "three"} };
for (auto [intKey, strVal] : make_keyval(digitMap)) {
    qDebug() << intKey << "->" << strVal;
}
// will print:
// 1 -> "one"
// 2 -> "two"
// 3 -> "three"

const auto json = QJsonDocument::fromJson("{ '1': 'one', '2': 'two', '3': 'three' }");
for (auto [propKey, propVal] : make_keyval(json.toObject().toVariantMap())) {
    qDebug() << propKey << "->" << propVal.toString();
}
// will print:
// "1" -> "one"
// "2" -> "two"
// "3" -> "three"
```
