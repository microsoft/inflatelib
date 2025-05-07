#!/bin/bash -e

uname -a | grep -qi wsl && echo "true" || echo "false"
