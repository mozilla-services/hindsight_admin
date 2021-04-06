#!/bin/bash
curl -sL https://raw.githubusercontent.com/travis-ci/artifacts/main/install | bash
tar -C dist -zcvf hindsight_admin.tgz .

export ARTIFACTS_PERMISSIONS="public-read"
export ARTIFACTS_TARGET_PATHS="/${TRAVIS_REPO_SLUG}/${TRAVIS_BRANCH}/${TRAVIS_JOB_NUMBER}/centos7/"
export ARTIFACTS_PATHS="hindsight_admin.tgz:dist/"
artifacts upload

echo "$ARTIFACTS_TARGET_PATHS" > latest.txt
export ARTIFACTS_TARGET_PATHS="/${TRAVIS_REPO_SLUG}/${TRAVIS_BRANCH}/centos7/"
export ARTIFACTS_PATHS="hindsight_admin.tgz:latest.txt"
artifacts upload
