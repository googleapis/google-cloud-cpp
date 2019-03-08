## The GCS Library represents optional parameters as variadic function arguments.

**Status**: accepted

**Context**: most APIs in the GCS library have a number of optional parameters,
for example, the API can use `ifMetagenerationMatch` to apply an operation only
if the metadata generation matches a given number. The question arose of how to
represent these parameters, as properties that we modify in the client or
object, or as per-request parameters in the function used to access the API.
That is, we had two proposals, one where the application would write code like
this:

```C++
void AppCode(Bucket b, FixedArgument1 foo, FixedArgument2 bar) {
  b.ApiName(
      foo, bar, storage::IfMetagenerationMatch(7), UserProject("my-project"));
```

vs. code like this:

```C++
void AppCode(Bucket b, FixedArgument1 foo, FixedArgument2 bar) {
  // Create a new bucket handle that applies the given optional parameters to
  // all requests.
  auto bucket = b.ApplyModifiers(
      storage::IfMetagenerationMatch(7), UserProject("my-project"))
  bucket.ApiName(foo, bar);
}
```

**Decision**: The parameters are passed as variadic arguments into any function
that needs them. That is, all APIs look like this:

```C++
class Bucket /* or Object as applicable */ { public:
template <typename... Parameters>
ReturnType ApiName(
    FixedArgument1 a1, FixedArgument2 a2,
    Parameters&&... p);
```

and are used like this:

```C++
void AppCode(Bucket b, FixedArgument1 foo, FixedArgument2 bar) {
  b.ApiName(
      foo, bar, storage::IfMetagenerationMatch(7), UserProject("my-project"));
```

**Consequences**: The advantages of this approach include:

- It is easier to use parameters in an API, it does not require to create a new
  bucket or object or client handle just for changing the parameters in one
  request.

The downsides include:

- All APIs become templates, we should be careful not to create massive header
  files that are slow to compile.
- It is harder to overload APIs.
- It is not clear how other optional parameters of the APIs, such as timeouts,
  fit with this structure.
