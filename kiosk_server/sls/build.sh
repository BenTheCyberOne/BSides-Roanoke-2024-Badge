#!/bin/bash
# A build script for the website

set -e

pushd ../kiosk
ng build
popd

sls client deploy --no-delete-contents
