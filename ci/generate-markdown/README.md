# Tools for Generating Specific Markdown Files

The CI scripts and Dockerfiles are more complex than what most users need to
interact with. These scripts install development tools, such as code formatters
and drivers for integration tests. Our user-facing documentation aims to be
concise and straightforward.

Manually verifying these instructions can be quite tedious. To streamline this,
we use Docker as a scripting tool to execute the instructions, and then
automatically convert the resulting "script" into markdown format.

## How to Generate Markdown Files

Markdown files are generated automatically during the `checker-pr` build process.
If you modify Docker files or build instructions, you can regenerate the markdown
files using the following command:

```bash
ci/cloudbuild/build.sh -t checkers-pr --docker
git status # should display the modified files
```

> [!TIP]
> You can also manually run individual generator scripts, like this:
> ```bash
> ci/generate-markdown/generate-packaging.sh >doc/packaging.md
> ```
