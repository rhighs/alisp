#!/usr/bin/env bash

_OLD_PATH=$PATH
export PATH="$(pwd)/build/bin:$_OLD_PATH"

_OLD_PS1=$PS1
export PS1="(alisp-env) $_OLD_PS1"

deactivate () {
    export PS1=$_OLD_PS1
    export PATH=$_OLD_PATH
}
