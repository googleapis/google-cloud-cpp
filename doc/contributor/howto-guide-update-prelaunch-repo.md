# How-to Guide: update pre-launch repository

# Merging from Upstream

Clone a repository (you may already have one) that has both the public and
`*-prelaunch` repos as remotes. We choose a configuration that makes it really
hard to accidentally push prelaunch code to the public repo:

```shell
git clone https://github.com/googleapis/google-cloud-cpp.git
cd google-cloud-cpp
git remote add prelaunch git@github.com:googleapis/cpp-storage-prelaunch.git
```

Update your clone to the latest versions:

```shell
git fetch prelaunch
git fetch origin
```

Find out the latest commit from the public repo already present in the
`*-prelaunch` repo:

```shell
A=$(git rev-parse prelaunch/upstream)
```

Find out the latest commit from the public repo main branch:

```shell
B=$(git rev-parse origin/main)
```

Update the `upstream` branch on the `*-prelaunch` repository:

```shell
git push prelaunch origin/main:upstream
```

I use a separate workspace for the next steps:

```shell
cd $HOME/
git clone git@github.com:googleapis/cpp-storage-prelaunch.git
cd cpp-storage-prelaunch
git checkout pre-launch-acv2
git checkout -b chore-merge-from-public-circa-$(date +%Y-%m-%d)
git cherry-pick ${A}..${B}
```

Send a PR and squash commit as usual.
