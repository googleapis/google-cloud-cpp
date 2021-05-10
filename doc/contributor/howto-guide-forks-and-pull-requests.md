# How-to Guide: Forks and Pull Requests

We wrote this document because many Googlers do not use GitHub on a daily basis.
If you are an experienced GitHub contributor much of the information here will
be familiar to you, feel free to skip it. If you are new to GitHub or have not
used it in a while and want a refresher this might be useful.

## Creating a Fork

In this project we use the (more or less) standard
[GitHub workflow][workflow-link]:

You create a [fork][fork-link] of [google-cloud-cpp][repo-link]. You can think
of a "fork" as a full copy of the original repository, including all its history
and branches. Then [clone][about-clone] that fork into your workstation:
```console
git clone git@github.com:YOUR-USER-NAME/google-cloud-cpp.git
```

This creates a *second* copy of the original repository, with all its history
and branches. You can create new branches in this copy, change the history of
branches (you can, but don't!), and generally do all the version control things
you may be used to. Note that these local changes do not affect either of the
previous two copies.

The cloned repo that you created in the previous step will have its `origin`
set to your forked repo. You should now tell git about the the main
`upstream` repo, which you'll use to pull commits made by others in order to
keep your local repo and fork up to date.

```console
git remote add upstream git@github.com:googleapis/google-cloud-cpp.git
git remote -v  # Should show 'origin' (your fork) and 'upstream' (main repo)
```

To pull new commits from `upstream` into your local repo and
[sync your fork][syncing-a-fork] you can do the following:

```console
git checkout main
git pull --ff-only upstream main
git push  # Pushes new commits up to your fork on GitHub
```

> :attention: you probably want to do this periodically, and almost certainly
> before starting any new branches. Keeping your default branch (aka `main`)
> in sync is important to make your pull requests easy to review.

## Preparing to make a pull requests

Changes to the main repository must go through a review by one of the project
owners (even project owners have their code reviewed by a peer). To submit your
change for review you need to create a pull request. Typically you start by:

1. Picking an existing [GitHub bug][mastering-issues] to work on.
1. Create a new [branch][about-branches] for each feature (or bug fix).
   ```console
   git checkout main
   git checkout -b my-feature-branch
   git push -u origin my-feature-branch  # Tells fork on GitHub about new branch
   # make your changes
   git push
   ```
1. And then submit a [pull-request][about-pull-requests] to merge your branch
   into `googleapis/google-cloud-cpp`.
1. Your reviewers may ask questions, suggest improvements or alternatives. You
   address those by either answering the questions in the review or
   **adding more [commits][about-commits]** to your branch and `git push` -ing
   those commits to your fork.

### Resolving Conflicts and Rebasing

From time to time your pull request may have conflicts with the destination
branch (likely `main`). If so, we request that you [rebase][about-rebase]
your branch instead of merging. The reviews can become very confusing if you
merge during a pull request. You should first ensure that your `main`
branch has all the latest commits by syncing your fork (see above), then do
the following:

```shell
git checkout my-feature-branch
git rebase main
git push --force-with-lease
```

### Reproducing CI build failures

If you are a Googler, when you submit your pull request a number (about 40 at
last count) of builds start automatically. For non-Googlers, a project owner
needs to kickoff these builds for you.

We run so many builds because we need to test the libraries under as many
compilers, platforms, and configurations as possible. Our customers will not
necessarily use the same environment we use to build and run these libraries.

There is a completely separate
[guide](howto-guide-running-ci-builds-locally.md) explaining how to run these
builds locally in case one fails. It's also a good idea to run some of the
builds locally before sending a PR to verify that your change (likely) works.
Running the following builds locally should identify most common issues:

```
ci/cloudbuild/build.sh -t checkers-pr --docker
ci/cloudbuild/build.sh -t clang-tidy-pr --docker
ci/cloudbuild/build.sh -t asan-pr --docker
```

## Merging the changes

Eventually the reviewers accept your changes, and they are merged into the
`main` branch. We use "squash commits", where all your commits become a single
commit into the default branch. A project owner needs to merge your changes,
if you are a project owner, the expectation is that you will perform the merge
operation, and update the commit comments to something readable.

[workflow-link]: https://guides.github.com/introduction/flow/
[fork-link]: https://guides.github.com/activities/forking/
[repo-link]: https://github.com/googleapis/google-cloud-cpp.git
[mastering-issues]: https://guides.github.com/features/issues/
[about-clone]: https://help.github.com/articles/cloning-a-repository/
[about-branches]: https://help.github.com/articles/about-branches/
[about-pull-requests]: https://help.github.com/articles/about-pull-requests/
[about-commits]: https://help.github.com/desktop/guides/contributing-to-projects/committing-and-reviewing-changes-to-your-project/#about-commits
[about-rebase]: https://help.github.com/articles/about-git-rebase/
[syncing-a-fork]: https://help.github.com/articles/syncing-a-fork/
