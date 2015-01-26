#!/bin/bash
stop_all="docker stop $(docker ps -a -q)"
rm_all="docker rm $(docker ps -a -q)"
${stop_all}
${rm_all}
