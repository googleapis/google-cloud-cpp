# How-to Guide: Modify Github Actions (GHA)

Our GHA configurations live in `.github/workflows`.

We have 4 actions that run our builds on Windows and MacOS. 
 - `macos-bazel.yml`
 - `macos-cmake.yml`
 - `windows-bazel.yml`
 - `windows-cmake.yml`

To test changes to one of those GHA, you need to push it to a branch on the main repo with the prefix `ci-gha-`

```shell
git checkout -b ci-gha-XXX
git push -u upstream
```

Then delete the branch on the upstream repo when you're done
```shell
git push upstream --delete <branch>
```