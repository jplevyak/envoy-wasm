#!/bin/sh
python3 /code/service.py &
envoy -c /etc/service-envoy.yaml --service-cluster backend-service
