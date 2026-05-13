- DTS/AR: DTS or AR (mandatory, see the [Allocated Requirements List](https://gitlab.huawei.com/boole-compiler/llvm-project/wikis/Allocated-requirements))
- IMPACT: affected module or area of code base, separated by commas (mandatory)
- patch_type: huawei/opensource (optional, keep this line when "opensource" is selected)
- opensource_URL: upstream URL (optional, keep this line when this is an "opensource" patch)

## Pre-acceptance Checklist

- [ ] Design document has been uploaded to SVN.
- [ ] Status of this feature is up-to-date in the [Feature List](https://gitlab.huawei.com/boole-compiler/llvm-project/wikis/Boole-Features).
- [ ] Trivial commits are squashed. Commit messages follow LLVM community convention.
- [ ] Feature branch has been rebased on the target branch and passed the smoke test.
- [ ] Performance test results have been included in the description or discussion.

## Description

Description of the merge request. You can use the commit message if the branch
contains only one commit, or you can summarize all the commits more concisely.
Note, however, that if your branch contains multiple unrelated commits, you
should organize them into multiple merge requests. You are encouraged to
include relevant performance data (a before-and-after comparison, ideally).
