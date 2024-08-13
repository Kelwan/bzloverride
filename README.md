A CLI util tool for [Bazel](https://bazel.build/) to write [local_path_override](https://bazel.build/rules/lib/globals/module#local_path_override) to your MODULE file. Searches up your file tree for the local repo and uses a relative path. Also has filtered search for being lazy and can take in multiple args.

```
bzloverride example_bazel_dep
// Multiple args
bzloverride example_bazel_dep1 example_bazel_dep2 example_bazel_dep3 
```